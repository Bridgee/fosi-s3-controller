"""Check production HTTP/transport declaration blocks on all local cleanup paths."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'firmware/OrbitS3/s3_client.cpp').read_text()
blocks=[]
for signature,end in [('Result request(', '    String url=apiBase+endpoint;'),('bool downloadArtwork(', '    const bool tls=url.startsWith')]:
 start=s.index(signature);stop=s.index(end,start);prefix=s[start:stop]
 begin=min(prefix.index('HTTPClient http;'),prefix.index('BodyClock headerClock;'))
 blocks.append(prefix[begin:])
fixture=r'''
#include <cassert>
#include <set>
#include <cstdint>
#include <cstdio>
struct BodyClock {uint32_t now()const{return 0;}void idle()const{}};
struct WiFiClient {
 static std::set<const WiFiClient*> live;bool connectedFlag=false;
 WiFiClient(){live.insert(this);}virtual ~WiFiClient(){live.erase(this);}
 virtual void stop(){};
};std::set<const WiFiClient*> WiFiClient::live;
struct WiFiClientSecure:WiFiClient{};
template<class Transport,class Clock>struct HeaderBudgetClient:Transport{explicit HeaderBudgetClient(Clock&) {}};
// Match pinned Arduino2.0.17 behavior: begin stores a borrowed client even on
// parse failure; end does not clear it when disconnected; destructor calls stop.
struct HTTPClient {
 WiFiClient *client=nullptr;
 bool begin(WiFiClient &c,bool valid){client=&c;return valid;}
 void end(){if(client&&client->connectedFlag){client->stop();client=nullptr;}}
 ~HTTPClient(){if(client){assert(WiFiClient::live.count(client)&&"HTTP destructor used an expired transport");client->stop();}}
};
'''
for i,block in enumerate(blocks):
 fixture+='void actual_%d(bool tls,int path){\n'%i+block+r'''
 WiFiClient &client=tls?static_cast<WiFiClient&>(secure):plain;
 if(!http.begin(client,path!=0))return;
 client.connectedFlag=path==2;
 if(path!=3)http.end();
}
'''
fixture+='int main(){for(bool tls:{false,true})for(int path=0;path<4;++path){actual_0(tls,path);actual_1(tls,path);}puts("PASS: actual S3 HTTP declaration lifetimes, plain/TLS begin failure/disconnected/connected/early cleanup");}\n'
with tempfile.TemporaryDirectory(prefix='orbit-http-lifetime-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
