#include <ArduinoBLE.h>
#include <Arduino_LPS22HB.h>

BLEService temperatureService("1809");
BLEFloatCharacteristic tempChar("2A6E", BLERead | BLENotify);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Initializing BLE & Temperature Sensor...");

  if (!BLE.begin()) {
    Serial.println("❌ Failed to initialize Bluetooth!");
    while (1);
  }

  if (!BARO.begin()) {
    Serial.println("❌ LPS22HB sensor not detected!");
    while (1);
  }

  Serial.println("✅ BLE & LPS22HB initialized!");

  BLE.setLocalName("Nano33_Temperature");
  BLE.setAdvertisedService(temperatureService);
  temperatureService.addCharacteristic(tempChar);
  BLE.addService(temperatureService);

  tempChar.writeValue(0);
  BLE.advertise();
  Serial.println("🔄 Advertising BLE temperature data...");
}

void loop() {
  float temp = BARO.readTemperature();
  tempChar.writeValue(temp);
  Serial.print("Temperature Data Sent: ");
  Serial.print(temp);
  Serial.println("°C");

  delay(1000);
}
