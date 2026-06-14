#include <Adafruit_GFX.h> // Core graphics library
#include <Adafruit_EPD.h> // E-Paper Library
#include <SPI.h>
#include <WiFi.h> // Used to connect to WiFi
#include <HTTPClient.h> // Used to get the data from xkcd.com
#include <ArduinoJson.h> // Used to parse the JSON 

#include <secret.h>
// Warning, this code is completely untested on an actual board, as Hack Club Fallout requires a barebones driver written for design submission

// Onboard EE04 internal hardware pin routings mapped to ESP32-S3 GPIOs
// Source https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/
#define EPD_CS      2
#define EPD_DC      4
#define SRAM_CS    -1   // EE04 board does not feature an onboard SRAM chip
#define EPD_RESET   1
#define EPD_BUSY    3

#define EPD_MOSI    9
#define EPD_SCLK    7
#define EPD_MISO   -1   // MISO pin is unused for display-only SPI

// Define button pins according to schematic
const int BUTTON_KEY0 = 2;   // KEY0 - GPIO2
const int BUTTON_KEY1 = 3;   // KEY1 - GPIO3
const int BUTTON_KEY2 = 5;   // KEY2 - GPIO5

// Button state variables
bool lastKey0State = HIGH;
bool lastKey1State = HIGH;
bool lastKey2State = HIGH;

// Instantiate the monochrome driver configuration for the UC8253 (Width: 240, Height: 416)
Adafruit_UC8253 display(416, 240, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

#define BLACK EPD_BLACK
#define uS_TO_S_FACTOR 1000000ULL // Multiply by micro seconds to get seconds
RTC_DATA_ATTR int8_t day = 1; // Sets the day, with day 1 being Sunday, and day 7 being Saturday
// On days 2, 4, and 6 (Monday, Wednesday, Friday) new XKCD comics are released
RTC_DATA_ATTR int bootNumber = 1;
RTC_DATA_ATTR int xkcdComicNumber = 1;
//int status = WL_IDLE_STATUS;


void setup() {
  // Initialize serial and wait for it to connect
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("Adafruit EPD test");
  display.begin();

  // Set the physical buttons for the EE04
  pinMode(BUTTON_KEY0, INPUT_PULLUP);
  pinMode(BUTTON_KEY1, INPUT_PULLUP);
  pinMode(BUTTON_KEY2, INPUT_PULLUP);

  lastKey0State = digitalRead(BUTTON_KEY0);
  lastKey1State = digitalRead(BUTTON_KEY1);
  lastKey2State = digitalRead(BUTTON_KEY2);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1); // Use BUTTON_KEY0 as wakeup button

  // Setup the WiFi to connect with credentials (high power use) the first time and use cached credentials on boots after that.
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  if (bootNumber == 1) {
    WiFi.begin(ssid, pass);
  } else {
    WiFi.begin(); // Use cached credentials
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Temporary, used for testing
  pinMode(LED_BUILTIN, OUTPUT);

  bootNumber++;
}

void loop() {
  // Temporary, used for testing
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    const String mostRecentURL = "https://xkcd.com/info.0.json";

    // Initialize HTTP request to get info about the newest XKCD comic
    http.begin(mostRecentURL);
    // Perform the GET request and retrieve the JSON data about the newest XKCD comic
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {  // This url location should stay the same, so anything other than 200 is abnormal
      String payload = http.getString();
    } else {
      if (httpResponseCode > 0) {
        Serial.print("Abnormal Response Code:");
        Serial.print(httpResponseCode);
        Serial.print("Payload:");
        Serial.print(http.getString());
      } else {
        Serial.print("Error Code:");
        Serial.print(httpResponseCode);
      }
    }

    // Get the new URL by parsing the JSON. TODO While we're here, also remember what month, day, and year it is (Need to do math to update the day counter if I want to include it)
    //String currentImageURL = 
  }

  //esp_sleep_enable_timer_wakeup(60 * uS_TO_S_FACTOR);
  esp_sleep_enable_timer_wakeup(86400 * uS_TO_S_FACTOR); // Wakeup the same time every day
  esp_deep_sleep_start();
}
