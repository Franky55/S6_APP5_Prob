#include <Arduino.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>


// ================= WIFI =================

const char* ssid = "Gros_Charles";
const char* password = "VroomVroom";


// ================= BLE ==================

int scanTime = 5;
BLEScan *pBLEScan;


// ================= WEBSOCKET ============

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");


// ================= BEACONS ==============

struct BeaconInfo
{
    String uuid;
    unsigned long lastSeen;
};


std::vector<BeaconInfo> foundBeacons;


// Temps avant de considérer un beacon perdu
const unsigned long BEACON_TIMEOUT = 10000;



// ================= JSON SEND ============

void sendBeaconEvent(String uuid, String event)
{
    JsonDocument doc;

    doc["event"] = event;
    doc["uuid"] = uuid;

    String json;

    serializeJson(doc, json);

    Serial.println(json);

    ws.textAll(json);
}



// ================= BLE CALLBACK =========

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{

    void onResult(BLEAdvertisedDevice advertisedDevice)
    {

        if(!advertisedDevice.haveManufacturerData())
            return;


        String manufacturerData =
            advertisedDevice.getManufacturerData();


        uint8_t data[255];

        size_t dataLength =
            manufacturerData.length();


        if(dataLength > sizeof(data))
            return;


        memcpy(
            data,
            manufacturerData.c_str(),
            dataLength
        );



        // Vérification iBeacon Apple

        if(
            dataLength == 25 &&
            data[0] == 0x4C &&
            data[1] == 0x00
        )
        {

            BLEBeacon beacon;

            beacon.setData(manufacturerData);


            String uuid =
                beacon.getProximityUUID().toString();



            bool exists = false;



            // Cherche si le beacon existe déjà

            for(auto &b : foundBeacons)
            {

                if(b.uuid == uuid)
                {

                    b.lastSeen = millis();

                    exists = true;

                    break;
                }
            }



            // Nouveau beacon

            if(!exists)
            {

                BeaconInfo newBeacon;

                newBeacon.uuid = uuid;

                newBeacon.lastSeen = millis();


                foundBeacons.push_back(newBeacon);



                Serial.println("New beacon found");

                Serial.printf(
                    "UUID : %s\n",
                    uuid.c_str()
                );


                sendBeaconEvent(
                    uuid,
                    "found"
                );
            }
        }
    }
};




// ================= SETUP =================

void setup()
{

    Serial.begin(115200);



    // -------- WIFI --------

    WiFi.begin(
        ssid,
        password
    );


    Serial.print("Connecting WiFi");


    while(
        WiFi.status() != WL_CONNECTED
    )
    {
        delay(500);
        Serial.print(".");
    }


    Serial.println();

    Serial.println("WiFi connected");

    Serial.print("IP : ");

    Serial.println(
        WiFi.localIP()
    );



    // -------- WebSocket --------

    ws.onEvent(
        [](AsyncWebSocket *server,
           AsyncWebSocketClient *client,
           AwsEventType type,
           void *arg,
           uint8_t *data,
           size_t len)
        {

        }
    );


    server.addHandler(&ws);

    server.begin();




    // -------- BLE --------

    BLEDevice::init("");


    pBLEScan =
        BLEDevice::getScan();


    pBLEScan->setAdvertisedDeviceCallbacks(
        new MyAdvertisedDeviceCallbacks()
    );


    pBLEScan->setActiveScan(true);


    pBLEScan->setInterval(100);


    pBLEScan->setWindow(99);



    Serial.println("BLE ready");

}




// ================= LOOP ==================

void loop()
{

    // Scan BLE

    BLEScanResults *results =
        pBLEScan->start(
            scanTime,
            false
        );


    Serial.printf(
        "Devices found : %d\n",
        results->getCount()
    );



    // Vérification timeout

    for(
        int i = foundBeacons.size()-1;
        i >= 0;
        i--
    )
    {

        if(
            millis() - foundBeacons[i].lastSeen
            >
            BEACON_TIMEOUT
        )
        {

            String uuid =
                foundBeacons[i].uuid;



            Serial.printf(
                "Beacon lost : %s\n",
                uuid.c_str()
            );


            sendBeaconEvent(
                uuid,
                "lost"
            );



            foundBeacons.erase(
                foundBeacons.begin()+i
            );
        }
    }



    // Affichage état actuel

    Serial.println("\nCurrent beacons:");

    for(auto &b : foundBeacons)
    {

        Serial.printf(
            "%s  last seen %lu ms ago\n",
            b.uuid.c_str(),
            millis()-b.lastSeen
        );

    }



    pBLEScan->clearResults();


    ws.cleanupClients();


    delay(1000);
}