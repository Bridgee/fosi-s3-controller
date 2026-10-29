#include "s3_client.h"
#include "intro_ai_gate.h"
#include "network_clock.h"
#include "iphone_trigger.h"
#include "weather.h"
#include "lyrics.h"
#include "s3_protocol.h"
#include "artwork_decoder.h"
#include "bounded_body.h"
#include "http_header_budget.h"
#include "discovery_policy.h"
#include "wifi_recovery.h"
#include "source_status.h"
#include <mdns.h>
#include <atomic>
#include <esp_heap_caps.h>
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

static_assert(!ORBIT_LIVE || sizeof(ORBIT_WIFI_SSID)>1,
              "Live firmware requires a Wi-Fi SSID in config_local.h (or build flags)");

namespace s3 {
namespace {
QueueHandle_t snapshots=nullptr, commands=nullptr, volumes=nullptr, artworks=nullptr, artworkRequests=nullptr;
Snapshot displayed;
struct ArtworkRequest { char url[512],origin[192];uint32_t revision; };
// Only the network worker owns these; artwork jobs carry an immutable origin.
String apiBase;char apiIp[16]={};
std::atomic<bool> online(false),bluetoothInfoRequested(false),powerInFlight(false);
std::atomic<uint8_t> wifiDisconnectReason(0);
void wifiEvent(arduino_event_t *event) {
    if(!event)return;
    if(event->event_id==ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        wifiDisconnectReason.store(event->event_info.wifi_sta_disconnected.reason);
    else if(event->event_id==ARDUINO_EVENT_WIFI_STA_GOT_IP){wifiDisconnectReason.store(0);orbit_weather::network_changed();}
}
struct PsramAllocator {
    void *allocate(size_t n) {return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
    void deallocate(void *p) {free(p);}
    void *reallocate(void *p,size_t n) {return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
};
enum class Result { Ok, Missing, Error, Transport };
struct BodyClock {
    uint32_t now() const {return millis();}
    void idle() const {vTaskDelay(1);}
};
void publish(Snapshot &s) {
    if(!s.connected) {
        s.bluetooth=BluetoothStatus::Unknown;s.volumeLimitKnown=false;
        s.power=PowerState::Unknown;s.powerTransition=false;
        if(s.link!=LinkState::Waiting) {s.powerPending=false;powerInFlight.store(false);}
    }
    s.wifiConnected=WiFi.status()==WL_CONNECTED;
    s.automaticAddress=!strlen(ORBIT_S3_URL);
    s.wifiRssi=s.wifiConnected?WiFi.RSSI():0;
    copy(s.localIp,s.wifiConnected?WiFi.localIP().toString().c_str():"");
    copy(s.deviceIp,apiIp);
    online.store(s.connected);xQueueOverwrite(snapshots,&s);
}

String escape(const char *text) {
    String result;
    const char *hex="0123456789ABCDEF";
    for(const unsigned char *p=reinterpret_cast<const unsigned char*>(text);*p;++p) {
        if(isalnum(*p)||*p=='-'||*p=='_'||*p=='.'||*p=='~') result+=char(*p);
        else {result+='%';result+=hex[*p>>4];result+=hex[*p&15];}
    }
    return result;
}
Result request(const String &endpoint, JsonDocument &reply, const String *body=nullptr) {
    BodyClock headerClock;
    HeaderBudgetClient<WiFiClient,BodyClock> plain(headerClock);
    HeaderBudgetClient<WiFiClientSecure,BodyClock> secure(headerClock);
    // HTTPClient borrows the transports, including after begin/end failures.
    // Destroy it first: pinned Arduino2 may keep the client pointer on disconnect.
    HTTPClient http;
    String url=apiBase+endpoint;
    bool tls=url.startsWith("https://");
    if(tls) {
        if(strlen(ORBIT_S3_CA_CERT)==0) return Result::Error;
        if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<64*1024 ||
           heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<48*1024) return Result::Error;
        secure.setCACert(ORBIT_S3_CA_CERT);
        secure.setHandshakeTimeout(3);
    }
    if(!http.begin(tls?static_cast<WiFiClient&>(secure):plain,url)) return Result::Error;
    http.setConnectTimeout(1500);http.setTimeout(1500);
    // A stream JSON parser must not see HTTP chunk framing.
    http.useHTTP10(true);
    const char *responseHeaders[]={"Transfer-Encoding"};
    http.collectHeaders(responseHeaders,1);
    http.addHeader("Accept","application/json");
    plain.armHeaders();secure.armHeaders();
    int code;
    if(body) {http.addHeader("Content-Type","application/json");code=http.POST(*body);}
    else code=http.GET();
    plain.finishHeaders();secure.finishHeaders();
    if(code>0) {
        IPAddress ip=tls?secure.remoteIP():plain.remoteIP();
        if(ip[0]) copy(apiIp,ip.toString().c_str());
    }
    Result result=Result::Error;
    if(code<=0 || plain.headerLimitHit() || secure.headerLimitHit()) result=Result::Transport;
    else if(code==404) result=Result::Missing;
    else if(http.getSize()>int(ApiBodyLimit)) result=Result::Error;
    // getStream() exposes chunk framing. Reject an unexpected transfer coding
    // rather than feeding it to the JSON parser after requesting HTTP/1.0.
    else if(http.header("Transfer-Encoding").length()) result=Result::Error;
    else if(code>=200 && code<300 && body && http.getSize()==0) result=Result::Ok;
    else {
        auto *buffer=static_cast<char*>(heap_caps_malloc(ApiBodyLimit+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(buffer) {
            BodyClock clock;size_t size=0;
            auto readResult=read_body(http.getStream(),[&http]{return http.connected();},clock,
                                     buffer,ApiBodyLimit,http.getSize(),size);
            if(readResult==BodyResult::Ok) {
                buffer[size]=0;reply.clear();
                // Const input makes ArduinoJson own strings in the PSRAM
                // document. They must not refer to this temporary body buffer.
                auto error=deserializeJson(reply,static_cast<const char*>(buffer),size);
                if(!error && !reply.overflowed()) {
                    const char *name=reply["error"]["name"] | "";
                    if(strstr(name,"invalidPath")) result=Result::Missing;
                    else if(code>=200 && code<300 && reply["error"].isNull()) result=Result::Ok;
                } else if(body && code>=200 && code<300 && error==DeserializationError::EmptyInput) {
                    result=Result::Ok;
                }
            } else if(readResult!=BodyResult::TooLarge) result=Result::Transport;
            free(buffer);
        }
    }
    http.end();return result;
}
uint32_t ipValue(const IPAddress &ip) {
    return (uint32_t(ip[0])<<24)|(uint32_t(ip[1])<<16)|(uint32_t(ip[2])<<8)|ip[3];
}
bool discover(JsonDocument &doc,char (&pinned)[65],char (&message)[80]) {
    mdns_result_t *results=nullptr;
    // Bound both discovery time and response count; never sweep IP addresses.
    esp_err_t error=mdns_query_ptr("_googlecast","_tcp",1200,8,&results);
    DiscoveryChoice choice;
    if(error==ESP_OK) for(auto *r=results;r;r=r->next) {
        const char *model=nullptr,*id=nullptr;
        for(size_t i=0;i<r->txt_count;++i) {
            if(r->txt[i].key && !strcmp(r->txt[i].key,"md"))model=r->txt[i].value;
            if(r->txt[i].key && !strcmp(r->txt[i].key,"id"))id=r->txt[i].value;
        }
        if(!s3_model(model) || !cast_id(id))continue;
        for(auto *address=r->addr;address;address=address->next) {
            if(address->addr.type!=MDNS_IP_PROTOCOL_V4)continue;
            IPAddress ip(address->addr.u_addr.ip4.addr);
            if(!local_candidate(ipValue(ip),ipValue(WiFi.localIP()),ipValue(WiFi.subnetMask())))continue;
            choice.consider(ip.toString().c_str(),id,pinned);
        }
    }
    if(results)mdns_query_results_free(results);
    if(!choice.unique()) {
        copy(message,choice.multiple?"Multiple S3 devices / set a manual URL":"Searching for S3 / check network or set URL");
        return false;
    }
    apiBase=String("http://")+choice.ip;
    // Cast metadata alone is insufficient. Confirm an S3 identity AND a
    // valid S3 source node through the read-only nSDK API before any controls.
    const char *identityPaths[]={"settings:/system/productName","settings:/deviceName"};
    bool identity=false;
    for(const char *path:identityPaths) {
        if(request("/api/getData?path="+escape(path)+"&roles=value",doc)!=Result::Ok)continue;
        auto value=unwrap(doc.as<JsonVariantConst>());
        if(value.is<const char*>() && s3_model(value.as<const char*>())) {identity=true;break;}
    }
    Snapshot probe;
    bool verified=identity && request("/api/getData?path="+escape(path(Node::Source))+"&roles=value",doc)==Result::Ok &&
        parse(Node::Source,doc.as<JsonVariantConst>(),probe);
    if(!verified) {apiBase="";apiIp[0]=0;copy(message,"S3 discovery unverified / set a manual URL");return false;}
    copy(pinned,choice.id);copy(apiIp,choice.ip);copy(message,"S3 discovered / reading status");return true;
}
void clearTarget(Snapshot &state) {
    uint32_t revision=state.artworkRevision+1;
    state=Snapshot{};state.volumeCeiling=ORBIT_VOLUME_CEILING;state.artworkRevision=revision;
    apiBase="";apiIp[0]=0;
    xQueueReset(commands);xQueueReset(volumes);
    ArtworkRequest cancel={};cancel.revision=revision;xQueueOverwrite(artworkRequests,&cancel);
}
Result read(Node node, Snapshot &s, JsonDocument &doc,bool &outputPreferred) {
    auto fetch=[&](bool preferred) {
        const char *p=preferred?"ui:/custom/audioOutputModeTrue":path(node);
        String url="/api/getData?path="+escape(p)+(preferred?"&roles=preferred&type=structure":"&roles=value")+
            "&_nocache="+String(millis());
        Result result=request(url,doc);
        if(result==Result::Ok && !parse(node,doc.as<JsonVariantConst>(),s)) return Result::Error;
        if(result==Result::Ok && node==Node::Position)s.positionSampleMs=millis();
        return result;
    };
    Result result=fetch(node==Node::Output && outputPreferred);
    if(node==Node::Output && !outputPreferred && result==Result::Missing) {
        result=fetch(true);
        if(result==Result::Ok) outputPreferred=true; // Avoid an extra failed read on every poll.
    }
    return result;
}
void dropCommands() { xQueueReset(commands);xQueueReset(volumes);powerInFlight.store(false); }
bool downloadArtwork(const ArtworkRequest &job) {
    Artwork art;copy(art.url,job.url);art.revision=job.revision;
    String origin=job.origin,url=job.url;
    if(url.startsWith("/")) url=origin+url;
    BodyClock headerClock;
    HeaderBudgetClient<WiFiClient,BodyClock> plain(headerClock);
    HeaderBudgetClient<WiFiClientSecure,BodyClock> secure(headerClock);
    // HTTPClient borrows the transports, including after begin/end failures.
    // Destroy it first: pinned Arduino2 may keep the client pointer on disconnect.
    HTTPClient http;
    const bool tls=url.startsWith("https://");
    if((!tls && !url.startsWith("http://")) || (tls && !strlen(ORBIT_ARTWORK_CA_CERT))) return false;
    // Two concurrent TLS sessions need substantial INTERNAL heap, even with
    // image data in PSRAM. Prefer the record over exhausting Wi-Fi/TLS memory.
    size_t tlsReserve=origin.startsWith("https://")?192*1024:128*1024;
    if(tls && (heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<tlsReserve ||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<64*1024)) return false;
    if(tls) {secure.setCACert(ORBIT_ARTWORK_CA_CERT);secure.setHandshakeTimeout(3);}
    if(!http.begin(tls?static_cast<WiFiClient&>(secure):plain,url)) return false;
    http.useHTTP10(true);http.setConnectTimeout(1200);http.setTimeout(1200);
    plain.armHeaders();secure.armHeaders();
    int code=http.GET(),size=http.getSize();
    plain.finishHeaders();secure.finishHeaders();
    uint8_t *compressed=nullptr;
    bool complete=false;size_t transferred=0;
    if(code==200 && !plain.headerLimitHit() && !secure.headerLimitHit() &&
       size>8 && size<=int(ArtworkDownloadLimit)) {
        compressed=static_cast<uint8_t*>(heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(compressed) {
            auto &stream=http.getStream();size_t got=0;uint32_t started=millis();
            // Overall body deadline: readBytes would restart its timeout for
            // every arriving byte and could keep a trickling transfer alive.
            while(got<size && uint32_t(millis()-started)<2500 && online.load() &&
                  !uxQueueMessagesWaiting(artworkRequests)) {
                int available=stream.available();
                if(available>0) {
                    size_t amount=size-got;if(amount>1024) amount=1024;
                    if(amount>size_t(available)) amount=available;
                    int count=stream.read(compressed+got,amount);
                    if(count>0) got+=count;
                } else if(!http.connected()) break;
                vTaskDelay(1);
            }
            transferred=got;complete=got==size;
        }
    }
    http.end();
    if(ORBIT_DIAGNOSTICS && Serial)Serial.printf("Artwork: http=%d length=%d received=%u complete=%u\n",code,size,unsigned(transferred),unsigned(complete));
    if(complete && online.load() && !uxQueueMessagesWaiting(artworkRequests)) {
        uint32_t started=millis();art.large=decode_artwork(compressed,size,ArtworkLargeSide);
        if(art.large){art.largeSize=ArtworkLargeBytes;art.data=artwork_thumbnail(art.large);}
        if(ORBIT_DIAGNOSTICS && Serial) Serial.printf("Artwork: decode_ms=%u, ready=%d stack_hwm=%u\n",unsigned(millis()-started),art.data!=nullptr,unsigned(uxTaskGetStackHighWaterMark(nullptr)));
        if(art.data) art.size=ArtworkBytes;
    }
    free(compressed);
    if(!art.data){free(art.large);return false;}
    if(!online.load() || uxQueueMessagesWaiting(artworkRequests)) {free(art.data);free(art.large);return false;}
    Artwork discarded;
    if(xQueueReceive(artworks,&discarded,0)==pdTRUE){free(discarded.data);free(discarded.large);}
    if(xQueueSend(artworks,&art,0)!=pdTRUE) {free(art.data);free(art.large);return false;}
    return true;
}
void artworkWorker(void *) {
    ArtworkRequest job={};bool pending=false,attempted=false;uint32_t attemptedAt=0;uint8_t failures=0;
    static const uint32_t retryDelays[]={15000,30000,60000,120000,300000};
    for(;;) {
        ArtworkRequest newer;
        if(xQueueReceive(artworkRequests,&newer,pdMS_TO_TICKS(1000))==pdTRUE) {job=newer;pending=job.url[0]!=0;attempted=false;failures=0;}
        orbit_intro_async::MediaLease mediaLease;
        if(!mediaLease)continue; // retain artwork job; AI owns background TLS
        // Repeated broken/unavailable artwork must not download/decode every15s.
        // A new revision resets immediately; this job backs off to at most5min.
        if(ORBIT_ARTWORK_ENABLED && pending && online.load() && (!attempted || uint32_t(millis()-attemptedAt)>=retryDelays[failures?failures-1:0])) {
            pending=!downloadArtwork(job);attempted=true;attemptedAt=millis();
            if(pending && failures<5)++failures;
        }
        const bool defer=powerInFlight.load() || uxQueueMessagesWaiting(commands) || uxQueueMessagesWaiting(volumes) || orbit_iphone::busy();
        if(!orbit_lyrics::worker_tick(defer))orbit_weather::worker_tick(defer);
    }
}
void worker(void *) {
    Snapshot state;state.volumeCeiling=ORBIT_VOLUME_CEILING;
    BasicJsonDocument<PsramAllocator> doc(24576);
    uint32_t nextPoll=0, lastVolume=0, lastStackReport=0;
    uint8_t pollFailures=0;
    bool curveSettled=false,outputPreferred=false,mdnsReady=false;
    PowerState expectedPower=PowerState::Unknown;uint32_t powerCommandAt=0;
    const bool automatic=!strlen(ORBIT_S3_URL);
    char pinned[65]={};uint32_t nextDiscovery=0,lastDiscoveryInfo=0;
    apiBase=ORBIT_S3_URL;

    // Keep connection settings in RAM; avoid credential writes to NVS while
    // the RGB panel is already scanning from external memory.
    WiFi.persistent(false);
    WiFi.onEvent(wifiEvent);
    const bool stationReady=WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);
    const auto initialWifi=WiFi.begin(ORBIT_WIFI_SSID,ORBIT_WIFI_PASSWORD);
    WifiRecovery wifiRecovery;wifiRecovery.connected(millis());
    int lastWifiStatus=-1,lastWifiReason=-1;uint32_t lastWifiLog=0;
    if(ORBIT_DIAGNOSTICS && Serial)Serial.printf("WiFi: station=%u initial_status=%u\n",unsigned(stationReady),unsigned(initialWifi));
    bool clockStarted=false;
    for(;;) {
        {orbit_intro_async::MediaLease phoneLease;if(phoneLease)orbit_iphone::worker_tick();else if(orbit_iphone::busy())orbit_intro_async::preemptAi();}
        const auto wifiStatus=WiFi.status();const uint32_t wifiNow=millis();
        if(wifiStatus!=WL_CONNECTED) {
            pollFailures=0;
            if(automatic && apiBase.length())clearTarget(state);
            state.connected=false;state.link=LinkState::Offline;
            const auto reason=wifiDisconnectReason.load();
            const bool waitingDhcp=wifiStatus==WL_IDLE_STATUS;
            if(wifiRecovery.due(wifiNow,waitingDhcp)) {
                wifiRecovery.attempted(wifiNow);
                // Reapply the same configured credentials in RAM. No scanning,
                // insecure fallback/AP, mode cycling, NVS write or command replay.
                auto result=WiFi.begin(ORBIT_WIFI_SSID,ORBIT_WIFI_PASSWORD);
                if(ORBIT_DIAGNOSTICS && Serial)Serial.printf("WiFi: retry_status=%u reason=%u backoff=%u\n",unsigned(result),unsigned(reason),unsigned(wifiRecovery.backoff));
            }
            const uint32_t remaining=wifiRecovery.remaining(wifiNow,waitingDhcp);
            snprintf(state.message,sizeof(state.message),"Wi-Fi offline / reason %u / retry %us",unsigned(reason),unsigned((remaining+999)/1000));
            if(ORBIT_DIAGNOSTICS && Serial && (int(wifiStatus)!=lastWifiStatus || int(reason)!=lastWifiReason || uint32_t(wifiNow-lastWifiLog)>=30000)) {
                Serial.printf("WiFi: status=%u reason=%u retry_ms=%u\n",unsigned(wifiStatus),unsigned(reason),unsigned(remaining));lastWifiLog=wifiNow;
            }
            lastWifiStatus=wifiStatus;lastWifiReason=reason;
            curveSettled=false;outputPreferred=false;state.volumeMax=0;
            dropCommands();publish(state);vTaskDelay(pdMS_TO_TICKS(1000));continue;
        }
        if(lastWifiStatus!=WL_CONNECTED)orbit_weather::network_changed();
        if(lastWifiStatus!=WL_CONNECTED && ORBIT_DIAGNOSTICS && Serial)Serial.println("WiFi: connected / DHCP ready");
        lastWifiStatus=WL_CONNECTED;lastWifiReason=0;wifiRecovery.connected(wifiNow);
        // Start after DHCP, once. The existing SNTP client retries/re-syncs in
        // the background; clock and TLS share UTC system time, no UI waits.
        if(!clockStarted){orbit_clock::begin();clockStarted=true;}
        uint32_t now=millis();
        if(automatic && !state.wifiConnected)nextDiscovery=now;
        if(automatic && !apiBase.length()) {
            if(int32_t(now-nextDiscovery)>=0) {
                state.link=LinkState::Searching;copy(state.message,"Searching for S3");publish(state);
                if(!mdnsReady)mdnsReady=mdns_init()==ESP_OK;
                if(mdnsReady && discover(doc,pinned,state.message)) {pollFailures=0;state.link=LinkState::Starting;dropCommands();curveSettled=false;outputPreferred=false;nextPoll=millis();}
                else if(!mdnsReady) {state.link=LinkState::Error;copy(state.message,"Discovery unavailable / set a manual URL");}
                nextDiscovery=millis()+30000;publish(state);lastDiscoveryInfo=millis();
            } else if(uint32_t(now-lastDiscoveryInfo)>=2000) {
                publish(state);lastDiscoveryInfo=now;
            }
            vTaskDelay(pdMS_TO_TICKS(250));continue;
        }
        if(!powerInFlight.load()) {expectedPower=PowerState::Unknown;state.powerPending=false;}
        orbit_intro_async::MediaLease controlTlsLease(apiBase.startsWith("https://"));
        if(!controlTlsLease){if(uxQueueMessagesWaiting(commands)||uxQueueMessagesWaiting(volumes)||powerInFlight.load())orbit_intro_async::preemptAi();vTaskDelay(pdMS_TO_TICKS(20));continue;}
        Command command;
        bool hasCommand=xQueueReceive(commands,&command,0)==pdTRUE;
        if(!hasCommand && uint32_t(now-lastVolume)>=300) {
            hasCommand=xQueueReceive(volumes,&command,0)==pdTRUE;
            if(hasCommand) lastVolume=now;
        }
        if(hasCommand) {
            StaticJsonDocument<512> payload;
            if(uint32_t(now-command.created)<=2500 && encode(command,state,payload)) {
                if(power_action(command.action)) {
                    expectedPower=command.action==Action::Standby?PowerState::Standby:PowerState::Online;
                    powerCommandAt=millis();state.powerPending=true;
                }
                String body;serializeJson(payload,body);
                Result result=request("/api/setData",doc,&body);
                if(result==Result::Transport) {
                    // An uncertain write is not proof the device/address vanished.
                    // Stop queued controls, preserve context, read back once; never replay.
                    state.connected=false;state.volumeKnown=false;state.volumeLimitKnown=false;
                    copy(state.message,"Command response lost / checking S3");dropCommands();
                    state.link=LinkState::Waiting;
                    if(power_action(command.action))powerInFlight.store(true);
                } else {
                    if(power_action(command.action) && result!=Result::Ok) {
                        state.powerPending=false;expectedPower=PowerState::Unknown;powerInFlight.store(false);
                    }
                    state.link=result==Result::Ok?LinkState::Waiting:LinkState::Error;
                    copy(state.message,result==Result::Ok?"Waiting for S3 confirmation":"Command failed / check S3");
                }
                publish(state);
                // Device writes can take ~500 ms to become visible. No command
                // is retried automatically, especially a transport toggle.
                const uint32_t due=millis()+650;
                if(command.action!=Action::Volume || int32_t(nextPoll-due)>0)nextPoll=due;
            } else {
                if(power_action(command.action)) {
                    state.powerPending=false;expectedPower=PowerState::Unknown;powerInFlight.store(false);
                }
                state.link=LinkState::Error;copy(state.message,"Command expired or unavailable");publish(state);
            }
        }
        if(int32_t(millis()-nextPoll)>=0) {
            // Read real power first. Standby needs only this small status read;
            // player/volume endpoints may be unavailable while the audio core sleeps.
            state.power=PowerState::Unknown;state.powerTransition=false;
            const Result powerResult=read(Node::Power,state,doc,outputPreferred);
            bool powerTimeout=false;
            if(expectedPower!=PowerState::Unknown) {
                const bool confirmed=powerResult==Result::Ok && state.power==expectedPower && !state.powerTransition;
                powerTimeout=!confirmed && uint32_t(millis()-powerCommandAt)>=8000;
                if(confirmed || powerTimeout) {
                    expectedPower=PowerState::Unknown;state.powerPending=false;powerInFlight.store(false);
                }
            }
            if(powerResult==Result::Ok && power_blocks_media(state)) {
                if(state.power!=PowerState::Online) {
                    const bool hadArtwork=state.artwork[0]!=0;
                    state.sourceKnown=state.outputKnown=state.volumeKnown=state.muteKnown=state.volumeLimitKnown=false;
                    state.playerKnown=state.playing=state.positionKnown=false;
                    state.canToggle=state.canPlay=state.canPause=state.canPrevious=state.canNext=false;
                    state.title[0]=state.artist[0]=state.protocol[0]=state.artwork[0]=0;
                    state.elapsed=state.duration=0;state.positionMs=0;
                    state.bluetooth=BluetoothStatus::Unknown;
                    curveSettled=false;outputPreferred=false;state.volumeMax=0;
                    xQueueReset(volumes);
                    if(hadArtwork) {
                        ArtworkRequest cancel={};cancel.revision=++state.artworkRevision;
                        xQueueOverwrite(artworkRequests,&cancel);
                    }
                }
                state.connected=true;pollFailures=0;
                state.link=powerTimeout?LinkState::Error:state.powerPending||state.powerTransition?LinkState::Waiting:LinkState::Ready;
                copy(state.message,state.power==PowerState::Standby?"S3 in standby":"S3 changing power state");
                publish(state);nextPoll=millis()+(state.powerPending||state.powerTransition?650:2000);
            } else {
            bool transportFailure=powerResult==Result::Transport, valid=false, complete=true,interrupted=false;
            const char *failure=nullptr;
            if(!curveSettled && !transportFailure) {
                Result r=read(Node::VolumeMap,state,doc,outputPreferred);
                curveSettled=r==Result::Ok || r==Result::Missing;
                transportFailure=r==Result::Transport;
            }
            char oldUrl[512],oldTitle[160],oldArtist[160];
            copy(oldUrl,state.artwork);copy(oldTitle,state.title);copy(oldArtist,state.artist);
            const Node nodes[]={Node::Source,Node::VolumeLimit,Node::Volume,Node::Mute,Node::Output,Node::Player,Node::Position};
            for(Node node:nodes) {
                if(transportFailure) break;
                if(!poll_node(node,state))continue;
                Result r=read(node,state,doc,outputPreferred);
                // Optional for link health, required before numeric volume writes.
                if(node==Node::VolumeLimit) {
                    if(r!=Result::Ok)state.volumeLimitKnown=false;
                    if(uxQueueMessagesWaiting(commands)) {complete=false;interrupted=true;break;}
                    continue;
                }
                transportFailure=r==Result::Transport;
                valid=valid || r==Result::Ok;
                complete=complete && r==Result::Ok;
                if(r!=Result::Ok) {
                    if(!failure)failure=failed_node_detail(node);
                    // Never leave a stale capability enabled after a failed
                    // read; a different source may have taken over remotely.
                    if(node==Node::Player) {state.playerKnown=false;state.canToggle=state.canPrevious=state.canNext=false;}
                    if(node==Node::Source) state.sourceKnown=false;
                    if(node==Node::Volume) state.volumeKnown=false;
                    if(node==Node::Mute) state.muteKnown=false;
                    if(node==Node::Output) state.outputKnown=false;
                }
                // Allow queued controls between reads, rather than waiting
                // for every HTTP request in the six-node poll to finish.
                if(uxQueueMessagesWaiting(commands) ||
                   (node!=Node::Source && uxQueueMessagesWaiting(volumes))) {complete=false;interrupted=true;break;}
            }
            if(state.sourceKnown && state.input>1) {
                state.playerKnown=false;state.playing=false;
                state.canToggle=state.canPlay=state.canPause=state.canPrevious=state.canNext=false;
                state.title[0]=state.artist[0]=state.protocol[0]=state.artwork[0]=0;
                state.elapsed=state.duration=0;state.positionKnown=false;state.positionMs=0;
            }
            bool artworkChanged=strcmp(oldUrl,state.artwork) || strcmp(oldTitle,state.title) || strcmp(oldArtist,state.artist);
            if(artworkChanged) ++state.artworkRevision;
            state.connected=valid && !transportFailure;
            if(!state.connected) {curveSettled=false;outputPreferred=false;state.volumeMax=0;}
            copy(state.message,!state.connected?"S3 offline / reconnecting":
                complete?connected_source_detail(state):failure?failure:
                interrupted?"Reading S3 status":"S3 status incomplete");
            bool retryKnownTarget=false;
            if(!state.connected) {
                dropCommands();
                if(automatic) {
                    if(pollFailures<2)++pollFailures;
                    retryKnownTarget=apiBase.length() && pollFailures<2;
                    if(retryKnownTarget) {
                        // Keep the verified address/artwork as context, but no stale controls.
                        state.sourceKnown=state.outputKnown=state.volumeKnown=state.muteKnown=false;
                        state.volumeLimitKnown=state.playerKnown=state.positionKnown=false;
                        state.canToggle=state.canPlay=state.canPause=state.canPrevious=state.canNext=false;
                        copy(state.message,"S3 response unavailable / retrying known address");
                    } else {
                        clearTarget(state);curveSettled=false;outputPreferred=false;pollFailures=0;
                        copy(state.message,"S3 offline / rediscovering");nextDiscovery=millis()+5000;
                    }
                }
#if ORBIT_DIAGNOSTICS
                if(Serial)Serial.printf("S3: poll_failed transport=%u retry_known=%u wifi_status=%u\n",
                    unsigned(transportFailure),unsigned(retryKnownTarget),unsigned(WiFi.status()));
#endif
            } else pollFailures=0;
            state.link=!state.connected?(retryKnownTarget?LinkState::Waiting:LinkState::Offline):complete?LinkState::Ready:
                interrupted && !failure?LinkState::Waiting:LinkState::Limited;
            // Optional telemetry: poll only in BT mode or while About is open.
            // A missing BT endpoint must not turn a healthy S3 link into a fault.
            state.bluetooth=BluetoothStatus::Unknown;
            if(state.connected && !interrupted && !uxQueueMessagesWaiting(commands) &&
               !uxQueueMessagesWaiting(volumes) && (bluetoothInfoRequested.load() ||
               (state.sourceKnown && state.input==1))) {
                if(read(Node::Bluetooth,state,doc,outputPreferred)!=Result::Ok)
                    state.bluetooth=BluetoothStatus::Unknown;
            }
            publish(state);nextPoll=millis()+(retryKnownTarget?1000:state.connected?2000:5000);
            if(ORBIT_ARTWORK_ENABLED && artworkChanged) {
                ArtworkRequest job={};copy(job.url,state.artwork);copy(job.origin,apiBase.c_str());job.revision=state.artworkRevision;
                xQueueOverwrite(artworkRequests,&job);
            }
                if(powerTimeout) {state.link=LinkState::Error;copy(state.message,"Power change not confirmed");publish(state);}
            }
        }
        if(ORBIT_DIAGNOSTICS && Serial && uint32_t(millis()-lastStackReport)>=30000) {
            Serial.printf("Network: stack_hwm=%u\n",unsigned(uxTaskGetStackHighWaterMark(nullptr)));
            lastStackReport=millis();
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
}
void requestBluetoothInfo(bool aboutVisible) {bluetoothInfoRequested.store(aboutVisible);}
void begin() {
    snapshots=xQueueCreate(1,sizeof(Snapshot));
    commands=xQueueCreate(8,sizeof(Command));volumes=xQueueCreate(1,sizeof(Command));
    artworks=xQueueCreate(1,sizeof(Artwork));artworkRequests=xQueueCreate(1,sizeof(ArtworkRequest));
    if(!snapshots || !commands || !volumes || !artworks || !artworkRequests) {
        Serial.println("Network queues could not be allocated.");
        if(snapshots) {Snapshot failure;failure.link=LinkState::Error;copy(failure.message,"Network memory unavailable");publish(failure);}
        return;
    }
    Snapshot initial;
    String url=ORBIT_S3_URL;
    if(!strlen(ORBIT_WIFI_SSID) || (url.length() && !url.startsWith("http://") && !url.startsWith("https://"))) {
        initial.link=LinkState::Error;copy(initial.message,"Check Wi-Fi / optional S3 URL");publish(initial);return;
    }
    if(url.length()>=sizeof(ArtworkRequest{}.origin) || url.endsWith("/") || (url.startsWith("https://") && !strlen(ORBIT_S3_CA_CERT))) {
        initial.link=LinkState::Error;copy(initial.message,"Check S3 URL / HTTPS certificate");publish(initial);return;
    }
    publish(initial);
    if(xTaskCreatePinnedToCore(worker,"s3-network",12288,nullptr,2,nullptr,0)!=pdPASS) {
        initial.link=LinkState::Error;copy(initial.message,"Network task could not start");publish(initial);return;
    }
    // HTTPS X.509/ECDSA verification on IDF4.4 exceeds the old8KiB artwork-only stack.
    // Keep one shared task; weather logs its measured remaining stack after TLS.
    if(xTaskCreatePinnedToCore(artworkWorker,"media-background",16384,nullptr,1,nullptr,0)!=pdPASS)
        Serial.println("Background media task unavailable.");
}
bool receive(Snapshot &s) {
    if(!snapshots || xQueueReceive(snapshots,&s,0)!=pdTRUE) return false;
    displayed=s;return true;
}
bool send(Action action,int value) {
    if(!displayed.connected || !online.load() || !commands || !volumes) return false;
    Command command{action,value,millis()};
    // Check against the most recently displayed capabilities before queueing.
    StaticJsonDocument<512> doc;
    if(!encode(command,displayed,doc)) return false;
    if(power_action(action)) {
        bool expected=false;
        if(!powerInFlight.compare_exchange_strong(expected,true))return false;
        xQueueReset(commands);xQueueReset(volumes);
        if(xQueueSend(commands,&command,0)==pdTRUE){orbit_intro_async::preemptAi();return true;}
        powerInFlight.store(false);return false;
    }
    if(powerInFlight.load())return false;
    const bool accepted=action==Action::Volume?xQueueOverwrite(volumes,&command)==pdTRUE:xQueueSend(commands,&command,0)==pdTRUE;
    if(accepted)orbit_intro_async::preemptAi();return accepted;
}
bool receiveArtwork(Artwork &artwork) {
    return artworks && xQueueReceive(artworks,&artwork,0)==pdTRUE;
}
}
