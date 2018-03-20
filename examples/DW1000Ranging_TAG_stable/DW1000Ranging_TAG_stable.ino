// Code for TULIP ESP32
// Requires - ESP board integration with the Arduino IDE
// Requires - DWM1000 lib

/**
 * 
 * @todo
 *  - move strings to flash (less RAM consumption)
 *  - fix deprecated convertation form string to char* startAsTag
 *  - give example description
 */
#include <SPI.h>
#include "DW1000Ranging.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define MESSAGE_TYPE_RANGE               0x00;
#define MESSAGE_TYPE_ANCHOR_REGISTRATION 0x01;
#define MESSAGE_TYPE_ANCHOR_DELETION     0x02;

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;


// connection pins
const uint8_t PIN_RST = 21; // reset pin
const uint8_t PIN_IRQ = 15; // irq pin
const uint8_t PIN_SS = 32; // spi select pin

#define SERVICE_UUID           "B6B47992-21CD-11E8-B467-0ED5F89F718B" // Service UUID
#define CHARACTERISTIC_UUID_RX "B6B47CDA-21CD-11E8-B467-0ED5F89F718B"
#define CHARACTERISTIC_UUID_TX "B6B47FDC-21CD-11E8-B467-0ED5F89F718B" // Notify Characteristic UUID


class TulipBLEServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Mobile Device: CONNECTED.");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Mobile Device: DISCONNECTED.");
    }
};

class BLECallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {}
};


void setup() {
  Serial.begin(115200);
  delay(1000);

  //////////////////////////////
  // DWM1000 INITIALIZATION ////
  //////////////////////////////
  //init the configuration
  
  //init the configuration
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ); //Reset, CS, IRQ pin
  //define the sketch as anchor. It will be great to dynamically change the type of module
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);
  //Enable the filter to smooth the distance
  //DW1000Ranging.useRangeFilter(true);
  
  //we start the module as a tag
  DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);


  //////////////////////////////
  // BT INITIALIZATION /////////
  //////////////////////////////
  // Create the BLE Device
  BLEDevice::init("TULIP Tag"); // Give it a name

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new TulipBLEServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
                      
  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new BLECallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  DW1000Ranging.loop();
}

void newRange() {
  Serial.print("from: "); Serial.print(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
  Serial.print("\t Range: "); Serial.print(DW1000Ranging.getDistantDevice()->getRange()); Serial.print(" m");
  Serial.print("\t RX power: "); Serial.print(DW1000Ranging.getDistantDevice()->getRXPower()); Serial.println(" dBm");

  if (deviceConnected) {
    uint16_t address = DW1000Ranging.getDistantDevice()->getShortAddress();
    float range = DW1000Ranging.getDistantDevice()->getRange();
    uint8_t txArray[3 + sizeof(range)];
  
    txArray[0] = ((unsigned char *)(&address))[1];
    txArray[1] = ((unsigned char *)(&address))[0];
    txArray[2] = MESSAGE_TYPE_RANGE;
    memcpy(&(txArray[3]), &range, sizeof(range));
      
    pCharacteristic->setValue(txArray, (3 + sizeof(range))); 
    pCharacteristic->notify();
  }
}

void newDevice(DW1000Device* device) {
  Serial.print("ranging init; 1 device added ! -> ");
  Serial.print(" short:");
  Serial.println(device->getShortAddress(), HEX);

  if (deviceConnected) {
    uint16_t address = device->getShortAddress();
    uint8_t txArray[3];
  
    txArray[0] = ((unsigned char *)(&address))[1];
    txArray[1] = ((unsigned char *)(&address))[0];
    txArray[2] = MESSAGE_TYPE_ANCHOR_REGISTRATION;
      
    pCharacteristic->setValue(txArray, 3); 
    pCharacteristic->notify();
  }
}

void inactiveDevice(DW1000Device* device) {
  Serial.print("delete inactive device: ");
  Serial.println(device->getShortAddress(), HEX);
  if (deviceConnected) {
    uint16_t address = device->getShortAddress();
    uint8_t txArray[3];
  
    txArray[0] = ((unsigned char *)(&address))[1];
    txArray[1] = ((unsigned char *)(&address))[0];
    txArray[2] = MESSAGE_TYPE_ANCHOR_DELETION;
      
    pCharacteristic->setValue(txArray, 3); 
    pCharacteristic->notify();
  }
}

