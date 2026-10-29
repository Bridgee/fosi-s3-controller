#include "weather_protocol.h"
#include <assert.h>
#include <fstream>
#include <iterator>
#include <string>
using namespace orbit_weather;
int main(int argc,char **argv){
    assert(argc==2);std::ifstream f(argv[1]);std::string body((std::istreambuf_iterator<char>(f)),{});
    DynamicJsonDocument doc(8192);assert(!deserializeJson(doc,body));const uint32_t epoch=1793012700;
    Conditions out;assert(parse_forecast(doc.as<JsonVariantConst>(),epoch,out));assert(out.temperature==21.4f && out.rain==20 && out.code==2 && out.observed==1793012400);
    auto reload=[&]{doc.clear();assert(!deserializeJson(doc,body));};
    for(const char *key:{"temperature_2m","apparent_temperature","relative_humidity_2m","is_day","weather_code","wind_speed_10m","time"}){
        for(int mode=0;mode<5;++mode){reload();if(mode==0)doc["current"].remove(key);else if(mode==1)doc["current"][key]=nullptr;else if(mode==2)doc["current"][key]=true;else if(mode==3)doc["current"][key]="12";else doc["current"][key]=1e20;auto old=out;assert(!parse_forecast(doc.as<JsonVariantConst>(),epoch,out));assert(out.observed==old.observed && out.temperature==old.temperature);}
    }
    for(const char *key:{"time","temperature_2m_max","temperature_2m_min","precipitation_probability_max"}){
        reload();doc["daily"][key].as<JsonArray>().add(5);assert(!parse_forecast(doc.as<JsonVariantConst>(),epoch,out));
        reload();doc["daily"][key][0]=nullptr;assert(!parse_forecast(doc.as<JsonVariantConst>(),epoch,out));
    }
    reload();doc["current_units"]["temperature_2m"]="°F";assert(!parse_forecast(doc.as<JsonVariantConst>(),epoch,out));
    reload();doc["daily_units"].remove("time");assert(!parse_forecast(doc.as<JsonVariantConst>(),epoch,out));
    reload();doc["daily"]["temperature_2m_max"][0]=10;assert(!parse_forecast(doc.as<JsonVariantConst>(),epoch,out));
    reload();assert(!parse_forecast(doc.as<JsonVariantConst>(),1793020000,out));assert(!parse_forecast(doc.as<JsonVariantConst>(),1793010000,out));
    for(int code=0;code<100;++code){reload();doc["current"]["weather_code"]=code;assert(parse_forecast(doc.as<JsonVariantConst>(),epoch,out)==known_code(code));}
    for(unsigned code=0;code<256;++code)for(bool day:{false,true}){
        const auto shape=icon(uint8_t(code),day);assert((shape!=Icon::Count)==known_code(code));
        if(known_code(code))assert(strcmp(description(uint8_t(code),day),"Weather"));
    }
    for(unsigned code:{0u,1u,2u,80u,81u,82u,85u,86u})assert(icon(code,true)!=icon(code,false));
    assert(icon(45,true)!=icon(48,true));assert(icon(56,true)!=icon(51,true));
    assert(icon(66,true)!=icon(61,true));assert(icon(95,true)!=icon(96,true));
    assert(icon(96,true)!=icon(99,true));assert(icon(71,true)!=icon(75,true));
    reload();doc["current"]["is_day"]=0;assert(parse_forecast(doc.as<JsonVariantConst>(),epoch,out) && !out.day && icon(0,false)==Icon::Moon);
    // 25-hour DST day, small model/clock skew and failed output atomicity.
    reload();doc["daily"]["time"][0]=epoch-89900;doc["current"]["time"]=epoch;assert(parse_forecast(doc.as<JsonVariantConst>(),epoch,out));
    reload();doc["current"]["time"]=epoch+30;assert(parse_forecast(doc.as<JsonVariantConst>(),epoch,out)&&out.observed==epoch);
    Location loc;doc.clear();deserializeJson(doc,R"({"success":true,"city":"Los Angeles","latitude":34.0522,"longitude":-118.2437,"timezone":{"id":"America/Los_Angeles"}})");assert(parse_location(doc.as<JsonVariantConst>(),loc));assert(loc.latitude==34.05f && loc.longitude==-118.24f);
    for(const char *key:{"latitude","longitude","success"}){doc.clear();deserializeJson(doc,R"({"success":true,"city":"Los Angeles","latitude":34.0522,"longitude":-118.2437,"timezone":{"id":"America/Los_Angeles"}})");auto old=loc;doc[key]=false;assert(!parse_location(doc.as<JsonVariantConst>(),loc));assert(!strcmp(old.city,loc.city));}
    char name[80];assert(city_name("São Paulo",10,name));assert(!city_name("A\nB",3,name));assert(!city_name("A\0B",3,name));assert(!city_name("\xc0\x80",2,name));assert(!city_name("\xed\xa0\x80",3,name));assert(!city_name("\xe2\x80\xae",3,name));
    std::string longName;for(int i=0;i<45;++i)longName+="é";assert(city_name(longName.data(),longName.size(),name)&&strlen(name)==78);
    Schedule schedule;schedule.success(0xfffffff0u,100);assert(!schedule.due(30));assert(schedule.due(100));
    schedule.failure(100);assert(schedule.delay==60000);schedule.failure(100);assert(schedule.delay==300000);schedule.failure(100);assert(schedule.delay==900000);schedule.failure(100);assert(schedule.delay==1800000);schedule.success(100,RefreshMs);assert(!schedule.failures);
    out.observed=epoch;assert(usable(out,epoch+60,1000));assert(!usable(out,epoch+7201,1000));assert(!usable(out,epoch+60,MaxAgeMs+1));assert(!usable(out,epoch-1,0));
    puts("Weather protocol, UTF-8, WMO, cache age and wrap/backoff checks PASS");
}
