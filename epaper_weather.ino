#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
    // display.print("Weather Report");
#include <Adafruit_GFX.h>
#include <GxEPD2_BW.h>
#include <time.h>

#include "DHT.h"
#include "icons/icons.h"        // includes all your 32×32 icons stored in icons/*.h


// --------------------------
// DISPLAY SETUP
// --------------------------
#define EPD_CS   7
#define EPD_DC   5
#define EPD_RST  4
#define EPD_BUSY 6

GxEPD2_BW<GxEPD2_420_GDEY042T81, 
          GxEPD2_420_GDEY042T81::HEIGHT> display(
            GxEPD2_420_GDEY042T81(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);



// --------------------------
// DHT11 (INDOOR SENSOR)
// --------------------------
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);


// --------------------------
// WIFI + NTP
// --------------------------
#include "arduino_secrets.h"   // contains ssid, password, LATITUDE, LONGITUDE

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 15UL * 60UL * 1000UL;


// --------------------------
// WEATHER API (Open-Meteo)
// --------------------------
String buildWeatherURL() {
  return String("https://api.open-meteo.com/v1/forecast?")
    + "latitude=" + String(LATITUDE)
    + "&longitude=" + String(LONGITUDE)
    + "&current=temperature_2m,relative_humidity_2m,weather_code,"
      "pressure_msl,apparent_temperature,wind_speed_10m,winddirection_10m,is_day"
    + "&timezone=auto&forecast_days=1";
}



// --------------------------
// LOCATION API (Geoapify)
// --------------------------
String locationName = "Loading";
bool useImperial = false;    // global

bool fetchLocationName() {
  HTTPClient http;
  String url = String("https://api.geoapify.com/v1/geocode/reverse?")
      + "lat=" + String(LATITUDE)
      + "&lon=" + String(LONGITUDE)
      + "&apiKey=" + GEOAPIFY_API_KEY;

  http.begin(url);
  int code = http.GET();
  if (code != 200) return false;

  String payload = http.getString();
  http.end();

  StaticJsonDocument<4096> doc;
  if (deserializeJson(doc, payload)) return false;

  String city   = doc["features"][0]["properties"]["city"] | "";
  String county = doc["features"][0]["properties"]["county"] | "";
  String country = doc["features"][0]["properties"]["country"] | "";
  String state = doc["features"][0]["properties"]["state"] | "";
  String iso = doc["features"][0]["properties"]["country_code"] | "";
  iso.toUpperCase();

  // Your existing location formatting...
  if (iso == "CN") {
    locationName = county + ", " + state;
  } else if (iso == "US") {
    String stateCode = doc["features"][0]["properties"]["state_code"] | "";
    locationName = city + ", " + stateCode;
  } else {
    locationName = city + ", " + country;
  }

  // NEW: imperial vs metric. You asked for US and Liberia.
  if (iso == "US" || iso == "LR") {
    useImperial = true;
  } else {
    useImperial = false;
  }

  return true;
}





// --------------------------
// WIND DIRECTION
// --------------------------
String windDirectionText(int deg) {
  deg = (deg + 360) % 360;

  if (deg < 15)  return "N";
  if (deg < 45)  return "NNE";
  if (deg < 75)  return "NE";
  if (deg < 105) return "E";
  if (deg < 135) return "ESE";
  if (deg < 165) return "SE";
  if (deg < 195) return "S";
  if (deg < 225) return "SSW";
  if (deg < 255) return "SW";
  if (deg < 285) return "WSW";
  if (deg < 315) return "W";
  if (deg < 345) return "WNW";
  return "N";
}


// --------------------------
// WEATHER CODE → TEXT + ICON
// --------------------------
String weatherCodeToText(int code) {
  if (code == 0) return "Clear";
  if (code == 1) return "Mainly Clear";
  if (code == 2) return "Partly Cloudy";
  if (code == 3) return "Overcast";
  if (code == 45 || code == 48) return "Fog";
  if (code == 51 || code == 53 || code == 55) return "Drizzle";
  if (code == 61) return "Light Rain";
  if (code == 63) return "Moderate Rain";
  if (code == 65) return "Heavy Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Rain Showers";
  if (code == 95) return "Thunder";
  if (code == 96 || code == 99) return "Hail";
  return "Unknown";
}


// Returns a pointer to the correct 32x32 bitmap for the given code and time-of-day.
// `isNight == true` uses the nt_* icons.
const unsigned char* weatherCodeToIcon(int code, bool isNight)
{
  // Helper macro: choose day or night variant
  #define ICON(day, night) (isNight ? (night) : (day))

  switch (code)
  {
    // 0: Clear sky
    case 0:
      return ICON(sunny_bits, nt_sunny_bits);  // nt_sunny = moon/clear night

    // 1: Mainly clear
    case 1:
      return ICON(mostlysunny_bits, nt_mostlysunny_bits);

    // 2: Partly cloudy
    case 2:
      return ICON(partlycloudy_bits, nt_partlycloudy_bits);

    // 3: Overcast
    case 3:
      return ICON(cloudy_bits, nt_cloudy_bits);

    // 45,48: Fog / depositing rime fog
    case 45:
    case 48:
      return ICON(fog_bits, nt_fog_bits);

    // 51,53,55: Drizzle
    case 51:
    case 53:
    case 55:
      return ICON(chancerain_bits, nt_chancerain_bits);

    // 56,57: Freezing drizzle (treat as sleet-ish)
    case 56:
    case 57:
      return ICON(chancesleet_bits, nt_chancesleet_bits);

    // 61: Slight rain
    case 61:
      return ICON(rain_bits, nt_rain_bits);

    // 63,65: Moderate / heavy rain
    case 63:
    case 65:
      return ICON(rain_bits, nt_rain_bits);

    // 80–82: Rain showers (slight/moderate/violent)
    case 80:
    case 81:
    case 82:
      return ICON(rain_bits, nt_rain_bits);

    // 66,67: Freezing rain
    case 66:
    case 67:
      return ICON(sleet_bits, nt_sleet_bits);

    // 71,73,75: Snow fall
    // 77: Snow grains
    case 71:
    case 73:
    case 75:
    case 77:
      return ICON(snow_bits, nt_snow_bits);

    // 85,86: Snow showers
    case 85:
    case 86:
      return ICON(chancesnow_bits, nt_chancesnow_bits);

    // 95: Thunderstorm
    case 95:
      return ICON(tstorms_bits, nt_tstorms_bits);

    // 96,99: Thunderstorm with hail / heavy hail
    case 96:
    case 99:
      return ICON(chancetstorms_bits, nt_chancetstorms_bits);

    // Everything else → unknown
    default:
      return ICON(unknown_bits, nt_unknown_bits);
  }

  #undef ICON
}


// --------------------------
// TIMESTAMP
// --------------------------
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
    return "1970-01-01 00:00";

  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &timeinfo);
  return String(buf);
}


// --------------------------
// FETCH WEATHER
// --------------------------
bool getWeather(
  float &tempC, float &humidity, int &code, float &pressure,
  float &feelsC, float &feelsF, float &windSpd, int &windDir,
  bool &isNight
) {
  HTTPClient http;
  
  String url = buildWeatherURL();
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<4096> doc;
  if (deserializeJson(doc, payload)) return false;

  JsonObject cur = doc["current"];

  tempC    = cur["temperature_2m"];
  humidity = cur["relative_humidity_2m"];
  code     = cur["weather_code"];
  pressure = cur["pressure_msl"];
  feelsC   = cur["apparent_temperature"];
  feelsF   = feelsC * 9/5 + 32;

  windSpd  = cur["wind_speed_10m"];
  windSpd  = windSpd * 0.6213712;
  windDir  = cur["winddirection_10m"];

  // NEW: use is_day from the API (1 = day, 0 = night)
  int isDayRaw = cur["is_day"] | 1;   // default to 1 (day) if missing
  isNight = (isDayRaw == 0);

  return true;
}


// --------------------------
// DRAW THE FULL DISPLAY
// --------------------------
// void drawPortrait(
//   float outC, float outH, int code, float pressure,
//   float feelsC, float feelsF, float windSpd, int windDir,
//   float inC, float inF, float inH,
//   bool isNight
// ) {
//   display.setRotation(0);
//   display.setFullWindow();

//   float outF = outC * 9/5 + 32;
//   float press_mmHg = pressure * 0.750062;

//   display.firstPage();
//   do {
//     display.fillScreen(GxEPD_WHITE);

//     // ---------------------------------
//     // HEADER (0–45)
//     // ---------------------------------
//     display.fillRect(0, 0, 400, 45, GxEPD_LIGHTGREY);
//     display.setTextColor(GxEPD_BLACK);
//     display.setTextSize(3);
//     // display.setCursor(10, 32);
//     // display.print("Weather Report");

//     // display.drawLine(0, 45, 400, 45, GxEPD_BLACK);  

//     // ---------------------------------
//     // OUTDOOR SECTION (60–190)
//     // ---------------------------------
//     display.fillRect(0, 60, 400, 130, GxEPD_LIGHTGREY);

//     // Label
//     display.setTextSize(2);
//     display.setCursor(10, 20);
//     // display.print("OUTDOOR");
//     display.print("WEATHER " + locationName);

//     // Location
//     display.setTextSize(1);
//     int16_t x1, y1; uint16_t w, h;
//     display.getTextBounds(locationName, 0, 0, &x1, &y1, &w, &h);
//     display.setCursor(380 - w, 40);
//     // display.print("OUTDOOR" + locationName);

//     // Line below "WEATHER (location)"
//     display.drawLine(0, 45, 400, 45, GxEPD_BLACK);

//     // Icon
//     int xicon=10, yicon=50;
//     display.drawXBitmap(xicon, yicon,
//         weatherCodeToIcon(code, isNight),
//         32, 32,
//         GxEPD_BLACK
//     );

//     int xgap=10, ygap=12;
//     // Condition text
//     display.setCursor(xicon+32+xgap, yicon+ygap);
//     display.setTextSize(2);
//     display.print(weatherCodeToText(code));

//     // Temperature
//     display.setCursor(10, 90);
//     display.setTextSize(2);
//     display.printf("Actual %.0fF (%.0fC)", outF, outC);
//     display.printf("\n Feels %.0fF (%.0fC)", feelsF, feelsC);
//     // display.setTextSize(2);
//     // display.setCursor(10, 155);
//     // display.printf("(%.0f C)", outC);

//     // Humidity, pressure, feels, wind
//     display.setTextSize(1);
//     display.setCursor(200, 95);
//     display.printf("Feels %.0fF (%.0fC)", feelsF, feelsC);

//     display.setCursor(200, 135);
//     display.printf("Hum: %.1f %%", outH);

//     display.setCursor(200, 150);
//     display.printf("Pres: %.1f mmHg", press_mmHg);

//     display.setCursor(200, 165);
//     display.printf("Wind: %.1f km/h %s",
//                    windSpd, windDirectionText(windDir).c_str());

//     // ---------------------------------
//     // INDOOR SECTION (205–260)
//     // ---------------------------------
//     display.drawLine(0, 195, 400, 180, GxEPD_BLACK);

//     display.setTextSize(2);
//     display.setCursor(10, 190);
//     display.print("INDOOR");

//     display.fillRect(0, 220, 400, 65, GxEPD_LIGHTGREY);

//     display.setTextSize(2);
//     display.setCursor(10, 235);
//     display.printf("Temp: %.0f F (%.0f C)", inF, inC);

//     display.setCursor(10, 265);
//     display.printf("Humidity: %.1f%%", inH);

//     // ---------------------------------
//     // FOOTER (260–295)
//     // ---------------------------------
//     display.setTextSize(1);
//     display.setCursor(10, 288);
//     display.print("Last updated: ");
//     display.print(getTimestamp());

//   } while (display.nextPage());
// }

String formatTemp(float c) {
  int c_int = (int)round(c);
  int f_int = (int)round(c * 9.0 / 5.0 + 32.0);

  if (useImperial) {
    // F first, then C
    return String(f_int) + " F (" + String(c_int) + " C)";
  } else {
    // C first, then F
    return String(c_int) + " C (" + String(f_int) + " F)";
  }
}

String formatWind(float kmh, const String &dir) {
  if (useImperial) {
    float mph = kmh * 0.621371f;
    int mph_int = (int)round(mph);
    return String(mph, 1) + " mph " + dir;
  } else {
    int kmh_int = (int)round(kmh);
    return String(kmh, 1) + " km/h " + dir;
  }
}

// hPa input for both remote (API) and local sensor
String formatPressure(float hPa) {
  if (useImperial) {
    // inHg, typical 2 decimal places
    float inHg = hPa * 0.029529983f;
    return String(inHg, 2) + " inHg";
  } else {
    // mmHg
    float mmHg = hPa * 0.750062f;
    return String(mmHg, 1) + " mmHg";
  }
}


void drawPortrait(
  float outC, float outH, int code, float pressure,
  float feelsC, float feelsF, float windSpd, int windDir,
  float inC, float inF, float inH,
  bool isNight
) {
  // Rotation is set in setup(); don't change it here.
  display.setFullWindow();

  // Round temps to nearest whole number
  int outF_int    = (int)round(outC * 9.0 / 5.0 + 32.0);
  int outC_int    = (int)round(outC);
  int feelsF_int  = (int)round(feelsF);
  int feelsC_int  = (int)round(feelsC);
  int inF_int     = (int)round(inF);
  int inC_int     = (int)round(inC);

  float press_mmHg = pressure * 0.750062;

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    //
    // ========================
    // OUTDOOR TITLE
    // ========================
    //
    display.setTextSize(2);
    display.setCursor(10, 12);   // moved up a bit
    // display.print("OUTDOOR ");
    display.print(locationName);

    //
    // ========================
    // OUTDOOR MAIN BLOCK
    // ========================
    //
    int iconX    = 10;
    int iconY    = 45;
    int iconSize = 32;
    display.drawLine(0, 1, 400, 1, GxEPD_BLACK);
    display.drawLine(0, iconY-10, 400, iconY-10, GxEPD_BLACK);

    // Icon (using XBM)
    display.drawXBitmap(
      iconX, iconY,
      weatherCodeToIcon(code, isNight),
      iconSize, iconSize,
      GxEPD_BLACK
    );

    // Condition text aligned vertically with icon
    int condX     = iconX + iconSize + 10;
    int condBaseY = iconY + iconSize / 2 - 8;  // nudged up
    display.setTextSize(2);
    display.setCursor(condX, condBaseY);
    display.print(weatherCodeToText(code));

    // LEFT COLUMN: Actual, Feels, Wind — all in text size 2
    int leftX   = 10;
    int actualY = iconY + iconSize + 18;

    display.setTextSize(2);
    display.setCursor(leftX, actualY);
    // display.print("Actual ");
    // display.printf("%d F (%d C)", outF_int, outC_int);
    display.print("Actual ");
    display.print(formatTemp(outC));    // outC is °C from API


    int feelsY = actualY + 22;
    display.setCursor(leftX, feelsY);
    // display.print("Feels  ");
    // display.printf("%d F (%d C)", feelsF_int, feelsC_int);
    display.print("Feels  ");
    display.print(formatTemp(feelsC));  // feelsC is apparent temp in °C

    int windY = feelsY + 22;
    display.setCursor(leftX, windY);
    display.print("Wind   ");
    display.print(formatWind(windSpd, windDirectionText(windDir)));
    // display.printf("%.1f mph %s", windSpd, windDirectionText(windDir).c_str());

    // RIGHT COLUMN: Hum, Pres
    int rightX    = 260;
    int rightTopY = iconY + 2;

    // Hum
    display.setTextSize(1);
    display.setCursor(rightX, actualY);
    // display.print("Humidity ");
    display.printf("Humidity %.1f%%", outH);

    // Pres
    int presY = rightTopY + 24;
    display.setTextSize(1);
    display.setCursor(rightX, feelsY);
    display.print("Pressure ");
    // display.printf("Pressure %.1f mmHg", press_mmHg);
    display.print(formatPressure(pressure));  // pass hPa, not mmHg

    //
    // ========================
    // INDOOR SECTION
    // ========================
    //
    int indoorTop = 180;  // slightly lower than before

    // Line above INDOOR (lowered slightly)
    display.drawLine(0, indoorTop, 400, indoorTop, GxEPD_BLACK);

    // INDOOR label
    display.setTextSize(2);
    display.setCursor(10, indoorTop + 10);
    display.print("Local");

    // Line below INDOOR (lowered slightly vs before)
    int indoorLineBelow = indoorTop + 32;
    display.drawLine(0, indoorLineBelow, 400, indoorLineBelow, GxEPD_BLACK);

    // Indoor TEMP and HUM on separate lines
    int indoorTextY = indoorLineBelow + 16;

    display.setTextSize(2);
    display.setCursor(10, indoorTextY);
    display.print("Temp ");
    display.print(formatTemp(inC));  // inC in °C from DHT/BME/BME280
    // display.printf("%d F (%d C)", inF_int, inC_int);

    int indoorHumY = indoorTextY + 24;
    display.setCursor(10, indoorHumY);
    display.print("Hum  ");
    display.printf("%.1f%%", inH);

    //
    // ========================
    // FOOTER
    // ========================
    //
    display.setTextSize(1);
    display.setCursor(10, 292);
    display.print("Last updated ");
    display.print(getTimestamp());
    display.print(" (C) pianocargo");

  } while (display.nextPage());
}

// --------------------------
// SETUP
// --------------------------
void setup() {
  Serial.begin(115200);
  dht.begin();

  display.init();
  display.setRotation(0);

  // boot screen
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    delay(1000);
    display.setTextColor(GxEPD_BLACK);
    delay(1000);
    display.setTextSize(2);
    display.setCursor(20, 100);
    display.println("Booting...");
    display.setCursor(20, 140);
    display.println("Connecting WiFi...");
  } while (display.nextPage());

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(200);

  configTime(-8*3600, 3600, "pool.ntp.org", "time.nist.gov");

  fetchLocationName(); // once
}


// --------------------------
// MAIN LOOP
// --------------------------
void loop() {
  unsigned long now = millis();

  if (now - lastUpdate > updateInterval || lastUpdate == 0) {
    lastUpdate = now;

    float outC,outH,pressure,feelsC,feelsF,windSpd;
    int code,windDir;
    bool isNight;

    if (!getWeather(outC,outH,code,pressure,feelsC,feelsF,windSpd,windDir,isNight)) {
      Serial.println("Weather fetch failed.");
      return;
    }

    float inH = dht.readHumidity();
    float inC = dht.readTemperature();
    float inF = dht.readTemperature(true);

  

    drawPortrait(outC,outH,code,pressure,
                 feelsC,feelsF,windSpd,windDir,
                 inC,inF,inH,
                 isNight);
  }

  delay(1000);
}
