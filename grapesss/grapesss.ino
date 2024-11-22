
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <SPIFFS.h> // needed for async server (?)
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ezButton.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// 
#define STATE_IDLE 0
#define STATE_WIFI 1
#define STATE_BLE  2

// BLE stuff
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// SETUP
Preferences preferences;
AsyncWebServer server(80);
ezButton button(0, EXTERNAL_PULLDOWN);

// Set these to your desired credentials.
const char *ssid = "ESP32-C3 AP";
const char *password = "password";

const int SHORT_PRESS_TIME = 1000;
const int LONG_PRESS_TIME = 1000;

unsigned long pressedTime  = 0;
bool isPressing = false;
bool isLongDetected = false;

int currentAppState;
unsigned long lastAppStateChange = 0;

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("Starting up!");

  //Print the wakeup reason for ESP32
  print_wakeup_reason();


  button.setDebounceTime(50);
  currentAppState = STATE_IDLE;
  lastAppStateChange = millis();

  
  // Open NVS storage
  preferences.begin("my-app", true); // "my-app" is the namespace
  String username = preferences.getString("username", "");
  Serial.println("Username: '"+username+"'");
  preferences.end();
  // DONE


  // setup deep sleep pin
  esp_deep_sleep_enable_gpio_wakeup(1 << 0, ESP_GPIO_WAKEUP_GPIO_HIGH);  // 1 = High, 0 = Low
}

void loop() {
  button.loop();

  if(button.isPressed()) {
    pressedTime = millis();
    isPressing = true;
    isLongDetected = false;
  }

  if(button.isReleased()) {
    isPressing = false;
    long pressDuration = millis() - pressedTime;

    if(pressDuration < SHORT_PRESS_TIME) {
      Serial.println("A short press is detected");
      currentAppState = STATE_BLE;
      lastAppStateChange = millis();

      startBLE();
    }
  }

  // if we are still pressing and not yet longDetected check the pressDuration
  if(isPressing && isLongDetected == false) {
    long pressDuration = millis() - pressedTime;

    if(pressDuration > LONG_PRESS_TIME) {
      isLongDetected = true;

      Serial.println("A long loooong press is detected");
      currentAppState = STATE_WIFI;
      lastAppStateChange = millis();

      startWiFIAndServer();
    }
  }

  // if not IDLE check how long we were in BLE or WIFI
  if (currentAppState != STATE_IDLE) {
    if (millis() - lastAppStateChange > 10000) {
      Serial.println("More than 10s in current state, switching off to IDLE mode");
      currentAppState = STATE_IDLE;
      lastAppStateChange = millis();

      stopWiFIAndServer();
      stopBLE();

      Serial.println("Entering deep sleep state");
      esp_deep_sleep_start();
    }
  }
}

void startWiFIAndServer() {
  Serial.println("Configuring access point...");

  // You can remove the password parameter if you want the AP to be open.
  // a valid password must have more than 7 characters
  if (!WiFi.softAP(ssid, password)) {
    log_e("Soft AP creation failed.");
    while (1);
  }
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server.on("/api/sysinfo", HTTP_GET, handleSysInfo);
  server.on("/api/config", HTTP_POST, handleConfig);
  
  server.begin();

  Serial.println("Server started");
}

void stopWiFIAndServer() {
  server.end();
  
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// This function is called when the sysInfo service was requested.
void handleSysInfo(AsyncWebServerRequest *request) {
  Serial.println("handleSysInfo");

  JsonDocument doc;
  doc["chip_model"] = ESP.getChipModel();
  doc["chip_cores"] = ESP.getChipCores();
  doc["chip_revision"] = ESP.getChipRevision();
  doc["flash_size"] = ESP.getFlashChipSize();
  doc["free_heap"] = ESP.getFreeHeap();

  String result;
  serializeJson(doc, result);

  request->send(200, "application/json; charset=utf-8", result);
}  // handleSysInfo()


// This function is called when the sysInfo service was requested.
void handleConfig(AsyncWebServerRequest *request) {
  Serial.println("handleConfig");

  // Process body content if available
  String bodyContent;
  if (request->hasParam("body", true)) {
    bodyContent = request->getParam("body", true)->value();
  } else {
    bodyContent = "No body content!";
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, bodyContent);
  if (error) {
    request->send(400, "text/plain", "Invalid JSON");
    return;
  }

  String received_username = doc["username"];
  Serial.println("received_username: '"+received_username+"'");

  // Open NVS storage
  preferences.begin("my-app", false); // "my-app" is the namespace
  String old_username = preferences.getString("username", "");
  if (received_username != "" && received_username != old_username) {
    Serial.println("received_username is not empty and is different from old_username '"+old_username+"'");
    preferences.putString("username", received_username);
  } else {
    Serial.println("received_username is empty, or the same old_username '"+old_username+"'");
  }
  preferences.end();
  // DONE

  request->send(200, "text/plain", "username updated");
}  // handleSysInfo()


void startBLE() {
  Serial.println("Starting BLE");

  BLEDevice::init("Long name works now");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic =
    pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pCharacteristic->setValue("Hello World says Neil");
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("Characteristic defined! Now you can read it in your phone!");

  Serial.println("BLE Started");
}


void stopBLE() {
  Serial.println("Stopping BLE");
  BLEDevice::deinit();
  Serial.println("BLE stopped");
}

/*
  Method to print the reason by which ESP32
  has been awaken from sleep
*/
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP:      Serial.println("Wakeup caused by ULP program"); break;
    case ESP_SLEEP_WAKEUP_GPIO:     Serial.println("Wakeup caused by GPIO"); break;
    default:                        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}