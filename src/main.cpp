#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <FastLED.h>
#include <TimeLib.h>
#include <BluetoothSerial.h>

const char* ssid = "ssid";
const char* password = "password";
const char* host = "script.google.com";
const char* googleScript = "your google api key here";
const int httpsPort = 443;

HTTPClient http;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
const int dayLightOffset_sec = 3600;
String eventStr;
int prevMin;

const int pinLEDs = 22;
const int brightness = 30;
const int matrixHeight = 8;
const int matrixWidth = 32;
const int numLEDs = matrixHeight * matrixWidth;
CRGB leds[numLEDs];
int ledDays[32];

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
BluetoothSerial SerialBT;

bool fetchCalendar(void);
int XY(int, int);
bool isLeapYear(int);
int daysInMonth(int, int);
void displayCalendar(time_t);
void syncTime(void);
void displayEvents(String);
void displayDigit(int, int, CRGB);
void displayClock(void);

void setup() {
  Serial.begin(115200);
  delay(10);

  FastLED.addLeds<NEOPIXEL, pinLEDs>(leds, numLEDs);
  FastLED.setBrightness(brightness);
  FastLED.clear(true);

  Serial.println();
  Serial.print(F("Connecting to network: "));
  Serial.println(ssid);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  //wait for WiFi to connect before getting gcalendar
  int i = 0;
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
    if (i < 15) {
      leds[XY(i, 7)] = CRGB::White;
      i++;
      FastLED.show();
    }
  }
  leds[XY(i, 7)] = CRGB::Green;
  FastLED.show();
  Serial.println("");
  Serial.println(F("WiFi is connected"));
  Serial.println(F("IP address is set: "));
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());

  SerialBT.begin("ESPCalendar");
  Serial.println("Bluetooth initialized");

  FastLED.clear();
  syncTime();
  displayClock();
  fetchCalendar();
  FastLED.show();
}

void syncTime() {
  configTime(gmtOffset_sec, dayLightOffset_sec, ntpServer);

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
    timeinfo.tm_mday, 1 + timeinfo.tm_mon, 1900 + timeinfo.tm_year);
  prevMin = minute();
}

void loop() {
  if (minute() != prevMin) {
    FastLED.clear();
    displayClock();
    fetchCalendar();
    prevMin = minute();
    FastLED.show();
  }

  if (SerialBT.available()) {
    char incomingChar = SerialBT.read();
    if (incomingChar == '+') {
      FastLED.clear();
      setTime(hour(), minute(), second(), day(), month() + 1, year());
      displayClock();
      fetchCalendar();
      FastLED.show();
    }
    else if (incomingChar == '-') {
      FastLED.clear();
      setTime(hour(), minute(), second(), day(), month() - 1, year());
      displayClock();
      fetchCalendar();
      FastLED.show();
    }
    else if (incomingChar == '0') {
      FastLED.clear();
      syncTime();
      displayClock();
      fetchCalendar();
      FastLED.show();
    }
    Serial.write(incomingChar);
    SerialBT.println("Adjusted calendar!");
  }

  delay(20);
}

bool fetchCalendar() {
  Serial.println("Getting calendar");
  http.end();
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(String("https://script.google.com/macros/s/") + googleScript + "/exec" +
    "?year=" + year() + "&month=" + month())) {
    Serial.println("Cannot connect to google script");
    return false;
  }
  Serial.println("Connected to google script");
  int returnCode = http.GET();
  Serial.print("Return code: ");
  Serial.println(returnCode);
  String response = http.getString();
  Serial.print("Response: ");
  Serial.println(response);

  displayCalendar(now());
  displayEvents(response);
  
  return true;
}

int XY (int x, int y) {
  if (x % 2 == 1) {
    return matrixHeight * (matrixWidth - (x + 1)) + y;
  }
  else {
    return matrixHeight * (matrixWidth - x) - (y+1);
  }
}

bool isLeapYear(int year) {
	return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int daysInMonth(int year, int month) {
	int daysInMonth;
	if (month == 4 || month == 6 || month == 9 || month == 11) {
		daysInMonth = 30;
	}
	else if (month == 2) {
		if (isLeapYear(year)) {
			daysInMonth = 29;
		}
		else {
			daysInMonth = 28;
		}
	}
	else {
		daysInMonth = 31;
	}
	return daysInMonth;
}

void displayCalendar(time_t time) {
  int yr = year(time);
  int m = month(time);
  int d = day(time);
  int mDays = daysInMonth(yr, m);
  int x = 7;
  int y = 6;
  int z = weekday(time) - 1;
  for (int i = d; i > 1; i--) {
    if (z == 0) {
      z = 6;
    }
    else {
      z--;
    }
  }
  for (int k = 1; k <= mDays; k++) {  
    ledDays[k] = XY(x, y);
    if (z == 6) {
      if (d == k) {
        leds[ledDays[k]] = CRGB::Green;
      }
      else {
        leds[ledDays[k]] = CRGB::Blue;
      }
      x -= 6;
      y--;
      z = 0;
    }
    else {
      if (d == k) {
        leds[ledDays[k]] = CRGB::Green;
      }
      else if (z == 0) {
        leds[ledDays[k]] = CRGB::Blue;
      }
      else {
        leds[ledDays[k]] = CRGB::Red;
      }
      x++;
      z++;
    }
  }
  switch (m) {
    case 1:
      leds[XY(10, 2)] = CRGB::Blue;
      break;

    case 2:
      leds[XY(11, 2)] = CRGB::Blue;
      break;

    case 3:
      leds[XY(12, 2)] = CRGB::Blue;
      break;

    case 4:
      leds[XY(13, 2)] = CRGB::Blue;
      break;

    case 5:
      leds[XY(3, 1)] = CRGB::Blue;
      break;

    case 6:
      leds[XY(4, 1)] = CRGB::Blue;
      break;

    case 7:
      leds[XY(5, 1)] = CRGB::Blue;
      break;

    case 8:
      leds[XY(6, 1)] = CRGB::Blue;
      break;

    case 9:
      leds[XY(7, 1)] = CRGB::Blue;
      break;

    case 10:
      leds[XY(8, 1)] = CRGB::Blue;
      break;

    case 11:
      leds[XY(9, 1)] = CRGB::Blue;
      break;

    case 12:
      leds[XY(10, 1)] = CRGB::Blue;
      break;
    
    default:
      break;
  }
  Serial.println("Calendar set");
}

void displayEvents(String events) {
  int i = 0;
  bool holiday = true;
  for (int i = events.indexOf("-"); i > 0; i = events.indexOf("-")) {
    if (events.charAt(0) == 'X') {
      events = events.substring(1);
      i--;
      holiday = false;
    }
    if (holiday) {
      leds[ledDays[events.substring(0, i).toInt()]] = CRGB::Cyan;
    }
    else {
      leds[ledDays[events.substring(0, i).toInt()]] = CRGB::Yellow;
    }
    events = events.substring(i + 1);
  }
  Serial.println("Events set");
}

void displayDigit(int offset, int digit, CRGB color) {
  switch (digit) {
    case 1:
      leds[XY(offset + 1, 1)] = color;
      leds[XY(offset + 2, 1)] = color;
      leds[XY(offset + 3, 1)] = color;
      leds[XY(offset + 2, 2)] = color;
      leds[XY(offset + 2, 3)] = color;
      leds[XY(offset + 2, 4)] = color;
      leds[XY(offset + 1, 5)] = color;
      leds[XY(offset + 2, 5)] = color;
      leds[XY(offset + 2, 6)] = color;
      break;

    case 2:
      leds[XY(offset, 1)] = color;
      leds[XY(offset + 1, 1)] = color;
      leds[XY(offset + 2, 1)] = color;
      leds[XY(offset + 3, 1)] = color;
      leds[XY(offset + 1, 2)] = color;
      leds[XY(offset + 2, 3)] = color;
      leds[XY(offset + 3, 4)] = color;
      leds[XY(offset, 5)] = color;
      leds[XY(offset + 3, 5)] = color;
      leds[XY(offset + 1, 6)] = color;
      leds[XY(offset + 2, 6)] = color;
      break;

    case 3:
      leds[XY(offset + 1, 1)] = color;
      leds[XY(offset + 2, 1)] = color;
      leds[XY(offset + 3, 2)] = color;
      leds[XY(offset + 3, 3)] = color;
      leds[XY(offset + 1, 4)] = color;
      leds[XY(offset + 2, 4)] = color;
      leds[XY(offset + 3, 5)] = color;
      leds[XY(offset + 1, 6)] = color;
      leds[XY(offset + 2, 6)] = color;
      break;

    case 4:
      leds[XY(offset + 3, 1)] = color;
      leds[XY(offset + 3, 2)] = color;
      leds[XY(offset + 1, 3)] = color;
      leds[XY(offset + 2, 3)] = color;
      leds[XY(offset + 3, 3)] = color;
      leds[XY(offset + 1, 4)] = color;
      leds[XY(offset + 3, 4)] = color;
      leds[XY(offset + 2, 5)] = color;
      leds[XY(offset + 3, 5)] = color;
      leds[XY(offset + 3, 6)] = color;
      break;

    case 5:
      leds[XY(offset + 1, 1)] = color;
      leds[XY(offset + 2, 1)] = color;
      leds[XY(offset + 3, 2)] = color;
      leds[XY(offset + 3, 3)] = color;
      leds[XY(offset + 1, 4)] = color;
      leds[XY(offset + 2, 4)] = color;
      leds[XY(offset + 3, 4)] = color;
      leds[XY(offset + 1, 5)] = color;
      leds[XY(offset + 1, 6)] = color;
      leds[XY(offset + 2, 6)] = color;
      leds[XY(offset + 3, 6)] = color;
      break;

    case 6:
      leds[XY(offset + 1, 1)] = color;
      leds[XY(offset + 2, 1)] = color;
      leds[XY(offset, 2)] = color;
      leds[XY(offset + 3, 2)] = color;
      leds[XY(offset, 3)] = color;
      leds[XY(offset + 3, 3)] = color;
      leds[XY(offset, 4)] = color;
      leds[XY(offset + 1, 4)] = color;
      leds[XY(offset + 2, 4)] = color;
      leds[XY(offset, 5)] = color;
      leds[XY(offset + 1, 6)] = color;
      leds[XY(offset + 2, 6)] = color;
      break;

    case 7:
      leds[XY(offset + 1, 1)] = color;
      leds[XY(offset + 1, 2)] = color;
      leds[XY(offset + 1, 3)] = color;
      leds[XY(offset + 2, 4)] = color;
      leds[XY(offset + 3, 5)] = color;
      leds[XY(offset, 6)] = color;
      leds[XY(offset + 1, 6)] = color;
      leds[XY(offset + 2, 6)] = color;
      leds[XY(offset + 3, 6)] = color;
      break;

    case 8:
      leds[XY(offset + 1, 1)] = color;
      leds[XY(offset + 2, 1)] = color;
      leds[XY(offset, 2)] = color;
      leds[XY(offset + 3, 2)] = color;
      leds[XY(offset, 3)] = color;
      leds[XY(offset + 3, 3)] = color;
      leds[XY(offset + 1, 4)] = color;
      leds[XY(offset + 2, 4)] = color;
      leds[XY(offset, 5)] = color;
      leds[XY(offset + 3, 5)] = color;
      leds[XY(offset + 1, 6)] = color;
      leds[XY(offset + 2, 6)] = color;
      break;
    
    case 9:
      leds[XY(offset + 1, 1)] = color;
      leds[XY(offset + 2, 1)] = color;
      leds[XY(offset + 3, 2)] = color;
      leds[XY(offset + 1, 3)] = color;
      leds[XY(offset + 2, 3)] = color;
      leds[XY(offset + 3, 3)] = color;
      leds[XY(offset, 4)] = color;
      leds[XY(offset + 3, 4)] = color;
      leds[XY(offset, 5)] = color;
      leds[XY(offset + 3, 5)] = color;
      leds[XY(offset + 1, 6)] = color;
      leds[XY(offset + 2, 6)] = color;
      break;

    case 0:
      leds[XY(offset + 1, 1)] = color;
      leds[XY(offset + 2, 1)] = color;
      leds[XY(offset, 2)] = color;
      leds[XY(offset + 3, 2)] = color;
      leds[XY(offset, 3)] = color;
      leds[XY(offset + 3, 3)] = color;
      leds[XY(offset, 4)] = color;
      leds[XY(offset + 3, 4)] = color;
      leds[XY(offset, 5)] = color;
      leds[XY(offset + 3, 5)] = color;
      leds[XY(offset + 1, 6)] = color;
      leds[XY(offset + 2, 6)] = color;
      break;

    default:
      break;
  }
}

void displayClock() {
  displayDigit(16, hour() / 10, CRGB::Red);
  displayDigit(20, hour() % 10, CRGB::Green);
  displayDigit(24, minute() / 10, CRGB::Blue);
  displayDigit(28, minute() % 10, CRGB::Violet);
}