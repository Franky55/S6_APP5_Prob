#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>
#define ENDIAN_CHANGE_U16(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))

int scanTime = 5;
BLEScan *pBLEScan;

std::vector<String> foundBeacons;
std::vector<String> currentScanBeacons;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {

    if (!advertisedDevice.haveManufacturerData()) {
      return;
    }

    String strManufacturerData = advertisedDevice.getManufacturerData();

    uint8_t cManufacturerData[255];
    size_t dataLength = strManufacturerData.length();

    if (dataLength > sizeof(cManufacturerData)) {
      return;
    }

    memcpy(cManufacturerData, strManufacturerData.c_str(), dataLength);

    // Vérifie si c'est un iBeacon Apple
    if (dataLength == 25 &&
        cManufacturerData[0] == 0x4C &&
        cManufacturerData[1] == 0x00) {

      BLEBeacon oBeacon;
      oBeacon.setData(strManufacturerData);

      String uuid = oBeacon.getProximityUUID().toString();

      // Ajouter au scan courant si absent
      bool foundInCurrentScan = false;

      for (auto &beacon : currentScanBeacons) {
        if (beacon == uuid) {
          foundInCurrentScan = true;
          break;
        }
      }

      if (!foundInCurrentScan) {
        currentScanBeacons.push_back(uuid);
      }

      // Vérifier si c'est une nouvelle balise
      bool alreadyExists = false;

      for (auto &beacon : foundBeacons) {
        if (beacon == uuid) {
          alreadyExists = true;
          break;
        }
      }

      if (!alreadyExists) {
        foundBeacons.push_back(uuid);

        Serial.println("Found a new iBeacon!");
        Serial.printf(
          "ID: %04X Major: %u Minor: %u UUID: %s Power: %d\n",
          oBeacon.getManufacturerId(),
          ENDIAN_CHANGE_U16(oBeacon.getMajor()),
          ENDIAN_CHANGE_U16(oBeacon.getMinor()),
          uuid.c_str(),
          oBeacon.getSignalPower()
        );
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Scanning...");

  BLEDevice::init("");

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(
      new MyAdvertisedDeviceCallbacks());

  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop() {

  // Vider la liste des balises vues durant ce scan
  currentScanBeacons.clear();

  BLEScanResults *foundDevices =
      pBLEScan->start(scanTime, false);

  Serial.printf(
      "\nScan complete - %d devices found\n",
      foundDevices->getCount());

  // Supprimer les balises disparues
  for (int i = foundBeacons.size() - 1; i >= 0; i--) {

    bool seen = false;

    for (auto &uuid : currentScanBeacons) {
      if (foundBeacons[i] == uuid) {
        seen = true;
        break;
      }
    }

    if (!seen) {
      Serial.printf(
          "Beacon disappeared: %s\n",
          foundBeacons[i].c_str());

      foundBeacons.erase(foundBeacons.begin() + i);
    }
  }

  // Afficher les balises actuellement présentes
  Serial.println("\nCurrent beacons:");

  for (size_t i = 0; i < foundBeacons.size(); i++) {
    Serial.printf(
        "%d : %s\n",
        i,
        foundBeacons[i].c_str());
  }

  pBLEScan->clearResults();

  delay(2000);
}