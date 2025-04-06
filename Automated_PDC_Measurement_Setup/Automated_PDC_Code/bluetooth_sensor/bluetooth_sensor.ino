#include <ArduinoBLE.h>

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Initializing Bluetooth...");

  if (!BLE.begin()) {
    Serial.println("❌ Failed to initialize Bluetooth!");
    while (1);
  }

  Serial.println("✅ Bluetooth initialized! Scanning for nearby devices...");

  BLE.scan();
}

void loop() {
  BLEDevice peripheral = BLE.available();
  if (peripheral) {
    Serial.print("Found BLE Device: ");
    Serial.println(peripheral.address());
  }
}
