#include <PDM.h>

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Testing Microphone...");

  if (!PDM.begin(1, 16000)) {
    Serial.println("Microphone NOT detected! Possible board issue.");
  } else {
    Serial.println("Microphone detected! Your board is working.");
  }
}

void loop() {
}
