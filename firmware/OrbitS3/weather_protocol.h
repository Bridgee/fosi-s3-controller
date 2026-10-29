#pragma once
#include "weather_model.h"
#include "timezone_data.h"
#include <ArduinoJson.h>
#include <initializer_list>
namespace orbit_weather {
inline bool numeric(JsonVariantConst value,double low,double high) {
    return !value.is<bool>() && (value.is<double>() || value.is<int64_t>() || value.is<uint64_t>()) && in_range(value.as<double>(),low,high);
}
inline bool integer(JsonVariantConst value,int64_t low,int64_t high) {
    return !value.is<bool>() && (value.is<int64_t>() || value.is<uint64_t>()) && value.as<int64_t>()>=low && value.as<int64_t>()<=high;
}
inline bool parse_location(JsonVariantConst root,Location &output) {
    if(!root["success"].is<bool>() || !root["success"].as<bool>() || !numeric(root["latitude"],-90,90) || !numeric(root["longitude"],-180,180))return false;
    auto name=root["city"].as<JsonString>();if(name.isNull() || !name.size())name=root["region"].as<JsonString>();
    Location next;if(!city_name(name.c_str(),name.size(),next.city))return false;
    auto timezone=root["timezone"]["id"].as<JsonString>();
    if(timezone.isNull() || timezone.size()>=sizeof(next.timezone) || !orbit_zone::find(timezone.c_str(),timezone.size()))return false;
    memcpy(next.timezone,timezone.c_str(),timezone.size());
    next.latitude=round(root["latitude"].as<double>()*100)/100;
    next.longitude=round(root["longitude"].as<double>()*100)/100;
    output=next;return true;
}
inline bool unit(JsonVariantConst value,const char *expected){return value.is<const char*>() && strcmp(value.as<const char*>(),expected)==0;}
inline bool parse_forecast(JsonVariantConst root,uint32_t epoch,Conditions &output) {
    auto c=root["current"],d=root["daily"],u=root["current_units"],v=root["daily_units"];
    if(!unit(u["temperature_2m"],"°C") || !unit(u["apparent_temperature"],"°C") || !unit(u["wind_speed_10m"],"km/h") ||
       !unit(u["relative_humidity_2m"],"%") || !unit(u["time"],"unixtime") ||
       !unit(v["temperature_2m_max"],"°C") || !unit(v["temperature_2m_min"],"°C") || !unit(v["precipitation_probability_max"],"%") || !unit(v["time"],"unixtime"))return false;
    if(!numeric(c["temperature_2m"],-90,65)||!numeric(c["apparent_temperature"],-110,80)||!numeric(c["wind_speed_10m"],0,450)||
       !integer(c["relative_humidity_2m"],0,100)||!integer(c["is_day"],0,1)||!integer(c["weather_code"],0,99)||!known_code(c["weather_code"].as<int>())||
       !integer(c["time"],1704067200,4102444799LL))return false;
    for(const char *key:{"time","temperature_2m_max","temperature_2m_min","precipitation_probability_max"})if(!d[key].is<JsonArrayConst>() || d[key].size()!=1)return false;
    if(!numeric(d["temperature_2m_max"][0],-90,65)||!numeric(d["temperature_2m_min"][0],-90,65)||!integer(d["precipitation_probability_max"][0],0,100)||!integer(d["time"][0],1704067200,4102444799LL))return false;
    Conditions next;
    next.observed=c["time"].as<uint32_t>();const uint32_t midnight=d["time"][0].as<uint32_t>();
    // Allow one minute of clock/model skew; normalize that small future edge.
    if(next.observed>epoch+60 || (epoch>next.observed && epoch-next.observed>MaxAgeMs/1000) || midnight>next.observed || next.observed-midnight>=25*3600)return false;
    if(next.observed>epoch)next.observed=epoch;
    next.temperature=c["temperature_2m"].as<float>();next.feels=c["apparent_temperature"].as<float>();next.wind=c["wind_speed_10m"].as<float>();
    next.high=d["temperature_2m_max"][0].as<float>();next.low=d["temperature_2m_min"][0].as<float>();if(next.high<next.low)return false;
    next.humidity=c["relative_humidity_2m"].as<uint8_t>();next.rain=d["precipitation_probability_max"][0].as<uint8_t>();
    next.code=c["weather_code"].as<uint8_t>();next.day=c["is_day"].as<int>()==1;output=next;return true;
}
}
