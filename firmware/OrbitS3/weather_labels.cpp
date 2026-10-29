#include "weather.h"
namespace orbit_weather {
Icon icon(uint8_t code,bool day) {
    switch(code){
    case 0:return day?Icon::Sun:Icon::Moon;
    case 1:return day?Icon::MostlyClearDay:Icon::MostlyClearNight;
    case 2:return day?Icon::PartlyCloudy:Icon::PartlyCloudyNight;
    case 3:return Icon::Cloud;
    case 45:return Icon::Fog;
    case 48:return Icon::RimeFog;
    case 51:return Icon::LightDrizzle;
    case 53:return Icon::Drizzle;
    case 55:return Icon::HeavyDrizzle;
    case 56:return Icon::LightFreezingDrizzle;
    case 57:return Icon::FreezingDrizzle;
    case 61:return Icon::LightRain;
    case 63:return Icon::Rain;
    case 65:return Icon::HeavyRain;
    case 66:return Icon::LightFreezingRain;
    case 67:return Icon::FreezingRain;
    case 71:return Icon::LightSnow;
    case 73:return Icon::Snow;
    case 75:return Icon::HeavySnow;
    case 77:return Icon::SnowGrains;
    case 80:return day?Icon::LightShowersDay:Icon::LightShowersNight;
    case 81:return day?Icon::ShowersDay:Icon::ShowersNight;
    case 82:return day?Icon::HeavyShowersDay:Icon::HeavyShowersNight;
    case 85:return day?Icon::SnowShowersDay:Icon::SnowShowersNight;
    case 86:return day?Icon::HeavySnowShowersDay:Icon::HeavySnowShowersNight;
    case 95:return Icon::Thunder;
    case 96:return Icon::Hail;
    case 97:return Icon::HeavyThunder;
    case 99:return Icon::HeavyHail;
    default:return Icon::Count;}
}
const char *description(uint8_t code,bool day) {
    switch(code){
    case 0:return day?"Clear sky":"Clear night";case 1:return "Mostly clear";case 2:return "Partly cloudy";case 3:return "Overcast";
    case 45:return "Fog";case 48:return "Rime fog";case 51:return "Light drizzle";case 53:return "Drizzle";case 55:return "Dense drizzle";case 56:return "Light freezing drizzle";case 57:return "Freezing drizzle";
    case 61:return "Light rain";case 63:return "Rain";case 65:return "Heavy rain";case 66:return "Light freezing rain";case 67:return "Heavy freezing rain";
    case 71:return "Light snow";case 73:return "Snow";case 75:return "Heavy snow";case 77:return "Snow grains";
    case 80:return "Light showers";case 81:return "Rain showers";case 82:return "Heavy showers";case 85:return "Snow showers";case 86:return "Heavy snow showers";
    case 95:return "Thunderstorm";case 96:return "Thunder with hail";case 99:return "Thunder with heavy hail";case 97:return "Heavy thunderstorm";default:return "Weather";
    }
}
}
