#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <ArduinoJson.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>

// ================= WIFI =================

const char* ssid = "FrankLauHouse";
const char* password = "CrazyFrog69!";

// ================= BLE ==================

BLEScan *pBLEScan;
int scanTime = 1;
const unsigned long BEACON_TIMEOUT = 3000;

// ================= WEBSOCKET ============

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ================= COAP =================

WiFiUDP coapUDP;
#define COAP_PORT 5683
#define LED_PIN 32

// ================= BEACONS ==============

struct BeaconInfo {
    String uuid;
    unsigned long lastSeen;
};

std::vector<BeaconInfo> foundBeacons;

// =================================================
// ENVOI WEBSOCKET JSON
// =================================================

void sendBeaconEvent(String uuid, String event) {
    JsonDocument doc;
    doc["event"] = event;
    doc["uuid"] = uuid;

    String json;
    serializeJson(doc, json);

    Serial.println(json);
    ws.textAll(json);
}

// =================================================
// CALLBACK BLE
// =================================================

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (!advertisedDevice.haveManufacturerData())
            return;

        String manufacturerData = advertisedDevice.getManufacturerData();

        uint8_t data[255];
        size_t dataLength = manufacturerData.length();

        if (dataLength > sizeof(data))
            return;

        memcpy(data, manufacturerData.c_str(), dataLength);

        // iBeacon Apple
        if (dataLength == 25 && data[0] == 0x4C && data[1] == 0x00) {
            BLEBeacon beacon;
            beacon.setData(manufacturerData);

            String uuid = beacon.getProximityUUID().toString();

            bool exists = false;

            for (auto &b : foundBeacons) {
                if (b.uuid == uuid) {
                    b.lastSeen = millis();
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                BeaconInfo newBeacon;
                newBeacon.uuid = uuid;
                newBeacon.lastSeen = millis();

                foundBeacons.push_back(newBeacon);

                Serial.println("Beacon found");
                Serial.println(uuid);

                sendBeaconEvent(uuid, "found");
            }
        }
    }
};

// =================================================
// SERVEUR COAP SIMPLE
// =================================================

void handleCoAP() {
    int packetSize = coapUDP.parsePacket();

    if (packetSize == 0)
        return;

    uint8_t buffer[256];
    int len = coapUDP.read(buffer, sizeof(buffer));

    Serial.println("CoAP packet received:");
    for (int i = 0; i < len; i++) {
        Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();

    bool ledOn = false;
    bool ledOff = false;

    // Recherche des options URI Path
    for (int i = 0; i < len; i++) {
        // "on"
        if (buffer[i] == 'o' && i+1 < len && buffer[i+1] == 'n') {
            ledOn = true;
        }

        // "off"
        if (buffer[i] == 'o' && i+2 < len && buffer[i+1] == 'f' && buffer[i+2] == 'f') {
            ledOff = true;
        }
    }

    if (ledOn) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("LED ON");
    }

    if (ledOff) {
        digitalWrite(LED_PIN, LOW);
        Serial.println("LED OFF");
    }

    // Réponse UDP simple
    IPAddress client = coapUDP.remoteIP();
    uint16_t port = coapUDP.remotePort();

    coapUDP.beginPacket(client, port);

    const char response[] = "OK";
    coapUDP.write((const uint8_t*)response, strlen(response));

    coapUDP.endPacket();
}

// =================================================
// SETUP
// =================================================

void setup() {
    Serial.begin(115200);

    // LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    // WIFI
    WiFi.begin(ssid, password);

    Serial.print("Connexion WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connecté");
    Serial.print("IP : ");
    Serial.println(WiFi.localIP());

    // WebSocket
    server.addHandler(&ws);
    server.begin();

    // COAP
    coapUDP.begin(COAP_PORT);
    Serial.println("CoAP started UDP 5683");

    // BLE
    BLEDevice::init("");

    pBLEScan = BLEDevice::getScan();

    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(50);
    pBLEScan->setWindow(50);

    Serial.println("BLE ready");
}

void loop() {
    // BLE scan
    BLEScanResults *results = pBLEScan->start(scanTime, false);
    pBLEScan->clearResults();

    // Beacon timeout
    for (int i = foundBeacons.size()-1; i >= 0; i--) {
        if (millis() - foundBeacons[i].lastSeen > BEACON_TIMEOUT) {
            String uuid = foundBeacons[i].uuid;

            Serial.println("Beacon lost:");
            Serial.println(uuid);

            sendBeaconEvent(uuid, "lost");

            foundBeacons.erase(foundBeacons.begin()+i);
        }
    }

    // CoAP
    handleCoAP();

    // Websocket
    ws.cleanupClients();

    delay(100);
}
