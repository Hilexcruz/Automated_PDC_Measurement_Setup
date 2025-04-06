#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>

BLEService motionService("180D");
BLEFloatCharacteristic xChar("2A37", BLERead | BLENotify);
BLEFloatCharacteristic yChar("2A38", BLERead | BLENotify);
BLEFloatCharacteristic zChar("2A39", BLERead | BLENotify);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Initializing Bluetooth & Motion Sensor...");

  if (!BLE.begin()) {
    Serial.println("❌ Failed to initialize Bluetooth!");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("❌ Motion sensor not detected!");
    while (1);
  }

  Serial.println("✅ Bluetooth & IMU initialized!");

  BLE.setLocalName("Nano33_Motion");
  BLE.setAdvertisedService(motionService);
  motionService.addCharacteristic(xChar);
  motionService.addCharacteristic(yChar);
  motionService.addCharacteristic(zChar);
  BLE.addService(motionService);

  xChar.writeValue(0);
  yChar.writeValue(0);
  zChar.writeValue(0);

  BLE.advertise();
  Serial.println("🔄 Advertising BLE motion data...");
}

void loop() {
  float x, y, z;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    xChar.writeValue(x);
    yChar.writeValue(y);
    zChar.writeValue(z);
    Serial.print("Motion Data Sent: X=");
    Serial.print(x);
    Serial.print(", Y=");
    Serial.print(y);
    Serial.print(", Z=");
    Serial.println(z);
  }
  delay(500);
}
