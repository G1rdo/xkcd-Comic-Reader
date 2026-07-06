#include <Adafruit_GFX.h> // Core graphics library
#include <Adafruit_EPD.h> // E-Paper Library
#include <SPI.h>
#include <WiFi.h> // Used to connect to WiFi
#include <HTTPClient.h> // Used to get the data from xkcd.com
#include <ArduinoJson.h> // Used to parse the JSON
#include <PNGdec.h> // Used to decode the PNG line by line

#include <secret.h>
// Warning, this code is completely untested on an actual board, as Hack Club Fallout requires a barebones driver written for design submission
// It is also effectively the second C++ program I have ever written, so please take that as a warning.
// This is a case of make it, then make it good.

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
const int BUTTON_KEY1 = 2;   // KEY0 - GPIO2
const int BUTTON_KEY2 = 3;   // KEY1 - GPIO3
const int BUTTON_KEY3 = 5;   // KEY2 - GPIO5

// Button state variables
bool lastKey1State = HIGH;
bool lastKey2State = HIGH;
bool lastKey3State = HIGH;

// Instantiate the monochrome driver configuration for the UC8253 (Width: 240, Height: 416)
Adafruit_UC8253 display(240, 416, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

#define BLACK EPD_BLACK
#define uS_TO_S_FACTOR 1000000ULL // Multiply by micro seconds to get seconds
RTC_DATA_ATTR int8_t day = 1; // Sets the day, with day 1 being Sunday, and day 7 being Saturday
// On days 2, 4, and 6 (Monday, Wednesday, Friday) new XKCD comics are released
RTC_DATA_ATTR int bootNumber = 1;
RTC_DATA_ATTR int xkcdComicNumber = 1;
//int status = WL_IDLE_STATUS;

PNG xkcdComicPng;
uint8_t *pngBuffer = nullptr;
size_t pngSize = 0;

HTTPClient http;


void setup() {
  // Initialize serial and wait for it to connect
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("Adafruit EPD test");
  display.begin();
  display.setRotation(1); // Set rotation to landscape

  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());


  // Set the physical buttons for the EE04
  pinMode(BUTTON_KEY1, INPUT_PULLUP);
  pinMode(BUTTON_KEY2, INPUT_PULLUP);
  pinMode(BUTTON_KEY3, INPUT_PULLUP);

  lastKey1State = digitalRead(BUTTON_KEY1);
  lastKey2State = digitalRead(BUTTON_KEY2);
  lastKey3State = digitalRead(BUTTON_KEY3);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_5, 1); // Use BUTTON_KEY3 as wakeup button

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

String getxkcdInfo() {
  String payload;
  bool downloadSuccessful = false;
  while (!downloadSuccessful) {
    // If WiFi not connected, try to connect and restart while loop
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin();
      delay(15000);
      continue;
    }

    // Initialize HTTP request to get info about the newest XKCD comic
    http.begin("https://xkcd.com/info.0.json");
    // Perform the GET request and retrieve the JSON data about the newest XKCD comic
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {  // This url location should stay the same, so anything other than 200 is abnormal
      payload = http.getString();
      http.end();
      downloadSuccessful = true;
    } else {
      if (httpResponseCode > 0) {
        Serial.print("Abnormal Response Code:");
        Serial.print(httpResponseCode);
        Serial.print("Payload:");
        Serial.print(http.getString());
        http.end();

        // Delay for a bit and restart the loop to try again
        delay(5000);
        continue;
      } else {
        Serial.print("Error Code:");
        Serial.print(httpResponseCode);
        http.end();

        // Delay for a bit and restart the loop to try again
        delay(5000);
        continue;
      }
    }
  }
  return payload;
}

// Get the image via the link from getXKCDInfo TODO: Make this more elegant
// 200 means successful, (no error code). 1 Means no wifi, 2 means invalid image size
int getxkcdComicImg(String imgUrl) {
  if (WiFi.status() != WL_CONNECTED) {
    return 1;
  }

  // Initialize HTTP request to get img from XKCD comic
  http.begin(imgUrl);
  // Perform the GET request and retrieve the JSON data about the newest XKCD comic
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {  // This url location should stay the same, so anything other than 200 is abnormal
    int contentLength = http.getSize();
    if (contentLength <= 0 || contentLength > 500 * 1024) {
      Serial.println("Invalid image size");
      http.end();
      return 2;
    }

    // This won't work if the XKCD server doesn't send contentLength
    pngBuffer = (uint8_t *)ps_malloc(contentLength);

    if (!pngBuffer) {
      Serial.println("PSRAM allocation failed");
      http.end();
      return 3;
    }

    WiFiClient *stream = http.getStreamPtr();

    size_t received = 0;

    while (http.connected() && received < contentLength) {
      size_t available = stream -> available();
      if (available) {
        int len = stream -> readBytes(
          pngBuffer + received,
          min((size_t)available, contentLength - received));
        received += len;
      }
      delay(1);
    }

    http.end();

    if (received != contentLength) {
      free(pngBuffer);
      pngBuffer = nullptr;
      Serial.println("Did not receieve full content of png");
      delay(5000);
      return 4;
    }

    pngSize = received;

    Serial.printf("Downloaded %u bytes\n", pngSize);

    // Image successfully stored in PSRAM, pngBuffer contains the image and pngSize contains its size.
    return 200;

  } else {
    if (httpResponseCode > 0) {
      Serial.print("Abnormal Response Code:");
      Serial.print(httpResponseCode);
      Serial.print("Payload:");
      Serial.print(http.getString());
      http.end();

      return httpResponseCode;
    } else {
      Serial.print("Error Code:");
      Serial.print(httpResponseCode);
      http.end();

      return httpResponseCode;
    }
  }
}

// TODO: Need to make this be able to resize the image as well, but it works for testing with specific XKCD comics
void drawxkcdComicPng() {
  int returnCode = xkcdComicPng.openRAM(pngBuffer, pngSize, PNGDrawLine);

  if (returnCode == PNG_SUCCESS) {
    display.clearBuffer();

    Serial.printf("PNG: %d x %d\n", xkcdComicPng.getWidth(), xkcdComicPng.getHeight());
    while (xkcdComicPng.decode(NULL, 0) == PNG_SUCCESS) {

    }
    xkcdComicPng.close();
  } else {
    Serial.printf("PNG open failed: %d\n", returnCode);
  }
}


int PNGDrawLine(PNGDRAW *pngDraw) {
    static uint16_t lineBuffer[240];

    xkcdComicPng.getLineAsRGB565(
        pngDraw,
        lineBuffer,
        PNG_RGB565_BIG_ENDIAN,
        0);

    for (int x = 0; x < pngDraw->iWidth && x < 240; x++) {

        uint16_t c = lineBuffer[x];

        uint8_t r = ((c >> 11) & 0x1F) << 3;
        uint8_t g = ((c >> 5) & 0x3F) << 2;
        uint8_t b = (c & 0x1F) << 3;


        uint8_t gray = (30 * r + 59 * g + 11 * b) / 100;

        display.drawPixel(
          x,
          pngDraw->y,
          gray > 128 ? EPD_WHITE : EPD_BLACK);
    }
    return 1;
}

void loop() {
  // Temporary, used for testing
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error, WiFi not connected");
  }
    
  // Get the info about the most recent xkcd comic
  String infoRequestPayload = getxkcdInfo();

  // Get the new URL by parsing the JSON. TODO While we're here, also remember what month, day, and year it is (Need to do math to update the day counter if I want to include it)
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, infoRequestPayload);

  // If there is a Json error, print it and try again in 5000 ms
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    delay(5000);
    return; // Try the whole loop again
  }

  const String safe_title = doc["safe_title"];
  const String alt = doc["alt"];
  const String imgUrl = doc["img"];
  xkcdComicNumber = doc["num"]; // RTC variable, already declared above


  int imgResponseCode = getxkcdComicImg(imgUrl);
  switch (imgResponseCode) {
    case 200:
      // Image now stored in pngBuffer
      break;
    case 1:
      // Wifi.status is not connected
      Serial.println("Wifi not connected");
      break;
    case 2:
      // Invalid image size
      Serial.println("Invalid image size");
      break;
    case 3:
      // PSRAM allocation failed
      Serial.println("PSRAM allocation failed");
      break;
    case 4:
      // Did not receive full content of png
      Serial.println("Did not receive full content of png");
      break;
    default:
      Serial.println("httpResponseCode" + imgResponseCode);
      break;
      
      
  }

  drawxkcdComicPng(); // Draw the image stored in pngBuffer line by line

  esp_sleep_enable_timer_wakeup(86400 * uS_TO_S_FACTOR); // Wakeup the same time every day
  esp_deep_sleep_start();
}
