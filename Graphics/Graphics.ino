/**
 * @file      Graphics.ino
 * @author    Christian Windt (cwindt0@gmail.com)
 * @license   MIT
 * @copyright Copyright (c) 2023  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2023-07-13
 * @note      Arduino Setting
 *            Tools ->
 *                  Board:"ESP32S3 Dev Module"
 *                  USB CDC On Boot:"Enable"
 *                  USB DFU On Boot:"Disable"
 *                  Flash Size : "16MB(128Mb)"
 *                  Flash Mode"QIO 80MHz
 *                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)"
 *                  PSRAM:"OPI PSRAM"
 *                  Upload Mode:"UART0/Hardware CDC"
 *                  USB Mode:"Hardware CDC and JTAG"
 *  Arduino IDE User need move all folders in [libdeps folder](./libdeps/)  to Arduino library folder
 */
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <time.h>
#include <lvgl.h>
#include "gui.h"
#include <esp_sntp.h>
#include "zones.h"
#include <Adafruit_NeoPixel.h>      //https://github.com/adafruit/Adafruit_NeoPixel
#include <AceButton.h>
#include <cJSON.h>
#include "rootCa.h"
#include <esp_wifi.h>

using namespace ace_button;

//! You can use EspTouch to configure the network key without changing the WiFi password below
#ifndef WIFI_SSID
#define WIFI_SSID             "Your WiFi SSID"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD         "Your WiFi PASSWORD"
#endif

// !Your Coinmarketcap API KEY , See https://coinmarketcap.com/api/
#ifndef COINMARKETCAP_APIKEY
#define COINMARKETCAP_APIKEY   ""
#endif
// !Your OpenWeatherMap API KEY, See https://openweathermap.org/api
#ifndef OPENWEATHERMAP_APIKEY
#define OPENWEATHERMAP_APIKEY   ""
#endif

// !Geographical coordinates (latitude) , See https://openweathermap.org/current
#ifndef OPENWEATHERMAP_LAT
#define OPENWEATHERMAP_LAT      "22.64610787469689"
#endif

// !Geographical coordinates (longitude) , See https://openweathermap.org/current
#ifndef OPENWEATHERMAP_LON
#define OPENWEATHERMAP_LON      "114.05498017285154"
#endif

// For temperature in Celsius
#define OPENWEATHERMAP_USE_CELSIUS

// For temperature in Fahrenheit
// #define OPENWEATHERMAP_USE_FAHRENHEIT


#define NTP_SERVER1           "pool.ntp.org"
#define NTP_SERVER2           "time.nist.gov"
#define GMT_OFFSET_SEC        0
#define DAY_LIGHT_OFFSET_SEC  0
#define GET_TIMEZONE_API      "https://ipapi.co/timezone/"
#define COINMARKETCAP_HOST    "pro-api.coinmarketcap.com"
#define DEFAULT_TIMEZONE      "CST-8"         //When the time zone cannot be obtained, the default time zone is used

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(BOARD_PIXELS_NUM, BOARD_PIXELS_PIN, NEO_GRB + NEO_KHZ800);

void WiFiEvent(WiFiEvent_t event);
void datetimeSyncTask(void *ptr);
void updateCoin360Task(void *ptr);
WiFiClientSecure client ;
HTTPClient https;
AceButton *button = NULL;
LilyGo_Class amoled;

double latitude;
double longitude;
String timezone = "";

static OpenWeatherMapApi weatherApi;
static TaskHandle_t  vUpdateDateTimeTaskHandler = NULL;
static TaskHandle_t  vUpdateWeatherTaskHandler = NULL;
static TaskHandle_t  vUpdateCoin360TaskHandler = NULL;
static SemaphoreHandle_t xWiFiLock = NULL;
static CoinMarketCapApiDataStream coinData[4];
static String httpBody;
extern CoinMarketCapApiSubsribe coinSubsribe[4] ;


void buttonHandleEvent(AceButton *button,
                       uint8_t eventType,
                       uint8_t buttonState)
{
    uint8_t id ;
    // Print out a message for all events.
    // Serial.print(F("handleEvent(): eventType: "));
    // Serial.print(AceButton::eventName(eventType));
    // Serial.print(F("; buttonState: "));
    // Serial.println(buttonState);

    const  BoardsConfigure_t *boards = amoled.getBoarsdConfigure();
    id = amoled.getBoardID();

    switch (eventType) {
    case AceButton::kEventClicked:
        if (button->getId() == 0) {
            selectNextItem();
        } else {
            if (boards->pmu && id == LILYGO_AMOLED_147) {
                // Toggle CHG led
                amoled.setChargingLedMode(
                    amoled.getChargingLedMode() != XPOWERS_CHG_LED_OFF ?
                    XPOWERS_CHG_LED_OFF : XPOWERS_CHG_LED_ON);
            }
        }
        break;
    case AceButton::kEventLongPressed:

        Serial.println("Enter sleep !");

        WiFi.disconnect();
        WiFi.removeEvent(WiFiEvent);
        WiFi.mode(WIFI_OFF);

        Serial.println();

        amoled.sleep();

        if (boards->pixelsPins != -1) {
            pixels.clear();
            pixels.show();
        }

        if (boards->pmu && id == LILYGO_AMOLED_147) {

            // Set PMU Sleep mode
            amoled.enableSleep();
            amoled.clearPMU();
            amoled.enableWakeup();
            // The 1.47-inch screen does not have an external pull-up resistor, so it cannot be woken up by pressing the button.
            // Use Tiemr wakeup .
            esp_sleep_enable_timer_wakeup(60 * 1000000ULL);  //60S

        } else {
            uint64_t gpio0_mask = 0;
#if ESP_IDF_VERSION >=  ESP_IDF_VERSION_VAL(4,4,6)
            esp_sleep_enable_ext1_wakeup(gpio0_mask, ESP_EXT1_WAKEUP_ANY_LOW);
#else
            esp_sleep_enable_ext1_wakeup(gpio0_mask, ESP_EXT1_WAKEUP_ALL_LOW);
#endif
        }

        Wire.end();

        Serial.println("Sleep Start!");
        delay(5000);
        esp_deep_sleep_start();
        Serial.println("This place will never print!");
        break;
    default:
        break;
    }
}

void buttonHandlerTask(void *ptr)
{
    const  BoardsConfigure_t *boards = amoled.getBoarsdConfigure();
    while (1) {
        for (int i = 0; i < boards->buttonNum; ++i) {
            button[i].check();
        }
        delay(5);
    }
    vTaskDelete(NULL);
}


void setup()
{

    Serial.begin(115200);

    xWiFiLock =  xSemaphoreCreateBinary();
    xSemaphoreGive( xWiFiLock );

    // Register WiFi event
    WiFi.onEvent(WiFiEvent);


    bool rslt = false;

    // Begin LilyGo  1.47 Inch AMOLED board class
    //rslt = amoled.beginAMOLED_147();


    // Begin LilyGo  1.91 Inch AMOLED board class
    //rslt =  amoled.beginAMOLED_191();

    // Begin LilyGo  2.41 Inch AMOLED board class
    // rslt =  amoled.beginAMOLED_241();


    // Automatically determine the access device
    rslt = amoled.begin();
    Serial.println("============================================");
    Serial.print("    Board Nmae:LilyGo AMOLED "); Serial.println(amoled.getName());
    Serial.println("============================================");


    if (!rslt) {
        while (1) {
            Serial.println("The board model cannot be detected, please raise the Core Debug Level to an error");
            delay(1000);
        }
    }


    // Register lvgl helper
    beginLvglHelper(amoled);

    const  BoardsConfigure_t *boards = amoled.getBoarsdConfigure();

    //Set button on/off charge led , just for test button ,only 1.47 inch amoled available
    if (boards->buttonNum) {

        ButtonConfig *buttonConfig;
        button = new AceButton [boards->buttonNum];
        for (int i = 0; i < boards->buttonNum; ++i) {
            Serial.print("init button : "); Serial.println(i);
            pinMode(boards->pButtons[i], INPUT_PULLUP);
            button[i].init(boards->pButtons[i], HIGH, i);
            buttonConfig = button[i].getButtonConfig();
            buttonConfig->setFeature(ButtonConfig::kFeatureClick);
            buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
            buttonConfig->setEventHandler(buttonHandleEvent);
        }

        if (boards->pixelsPins != -1) {
            pixels.begin(); // This initializes the NeoPixel library.
            pixels.setBrightness(15);

            // Test pixels color
            pixels.setPixelColor(0, pixels.Color(255, 0, 0)); pixels.show(); delay(500);
            pixels.setPixelColor(0, pixels.Color(0, 255, 0)); pixels.show(); delay(500);
            pixels.setPixelColor(0, pixels.Color(0, 0, 255)); pixels.show(); delay(500);
            pixels.clear();
            pixels.show();
        }
    }



    // Draw Factory GUI
    factoryGUI();


    WiFi.mode(WIFI_STA);

    // Connect to the Internet after initializing the UI.

    // Uncomment will use WIFI configured with SmartConfig
    /*
    wifi_config_t current_conf;
    esp_wifi_get_config(WIFI_IF_STA, &current_conf);
    if (strlen((const char *)current_conf.sta.ssid) == 0) {
    */

    // Just for factory testing.
    Serial.print("Use default WiFi SSID & PASSWORD!!");
    Serial.print("SSID:"); Serial.println(WIFI_SSID);
    Serial.print("PASSWORD:"); Serial.println(WIFI_PASSWORD);
    if (String(WIFI_SSID) == "Your WiFi SSID" || String(WIFI_PASSWORD) == "Your WiFi PASSWORD" ) {
        Serial.println("[Error] : WiFi ssid and password are not configured correctly");
        Serial.println("[Error] : WiFi ssid and password are not configured correctly");
        Serial.println("[Error] : WiFi ssid and password are not configured correctly");
    }
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Uncomment will use WIFI configured with SmartConfig
    /*
    } else {
    Serial.println("Begin WiFi");
    WiFi.begin();
    }
    */

    // Enable Watchdog
    enableLoopWDT();

    if (boards->buttonNum && button) {
        xTaskCreate(buttonHandlerTask, "btn", 5 * 1024, NULL, 12, NULL);
    }

    amoled.setHomeButtonCallback([](void *ptr) {
        Serial.println("Home key pressed!");
        static uint32_t checkMs = 0;
        static uint8_t lastBri = 0;
        if (millis() > checkMs) {
            if (amoled.getBrightness()) {
                lastBri = amoled.getBrightness();
                amoled.setBrightness(0);
            } else {
                amoled.setBrightness(lastBri);
            }
        }
        checkMs = millis() + 200;
    }, NULL);

}

void loop()
{
    lv_task_handler();
    delay(1);
}


void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d\n", event);

    switch (event) {
    case ARDUINO_EVENT_WIFI_READY:
        Serial.println("WiFi interface ready");
        break;
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
        Serial.println("Completed scan for access points");
        break;
    case ARDUINO_EVENT_WIFI_STA_START:
        Serial.println("WiFi client started");
        break;
    case ARDUINO_EVENT_WIFI_STA_STOP:
        Serial.println("WiFi clients stopped");
        break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("Connected to access point");
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println("Disconnected from WiFi access point");
        lv_msg_send(WIFI_MSG_ID, NULL);
        break;
    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
        Serial.println("Authentication mode of access point has changed");
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.print("Obtained IP address: ");
        Serial.println(WiFi.localIP());
        configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);

        lv_msg_send(WIFI_MSG_ID, NULL);
        break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        Serial.println("Lost IP address and IP address is reset to 0");
        lv_msg_send(WIFI_MSG_ID, NULL);
        break;
    default: break;
    }
}
