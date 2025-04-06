#include <Arduino_LPS22HB.h>

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Testing LPS22HB Pressure Sensor...");

  if (!BARO.begin()) {
    Serial.println("LPS22HB sensor NOT detected! Possible HTS221-only failure.");
  } else {
    Serial.println("LPS22HB sensor detected! HTS221 is the only one broken.");
  }
}

void loop() {}
