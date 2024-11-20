
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <SPIFFS.h> // needed for async server (?)
#include <ArduinoJson.h>
#include <Preferences.h>

Preferences preferences;

AsyncWebServer server(80);

// Set these to your desired credentials.
const char *ssid = "ESP32-C3 AP";
const char *password = "password";

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");

  delay(3000);

  // Open NVS storage
  preferences.begin("my-app", true); // "my-app" is the namespace
  String username = preferences.getString("username", "");
  Serial.println("Username: '"+username+"'");
  preferences.end();
  // DONE


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

void loop() {

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
