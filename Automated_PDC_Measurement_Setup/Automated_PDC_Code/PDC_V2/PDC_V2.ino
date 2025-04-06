// Global Variables
long tm;                  // Measurement time in seconds
float Vm;                 // Voltage in volts
int numMeasurements;      // Number of measurements
float maxOutputVoltagePlus = 0.0;  // Real-world max voltage for 5V from Arduino (H+).
float maxOutputVoltageMinus = 0.0; // Real-world max voltage for 5V from Arduino (H-).

// Pins for H+ and H- control
const int H_PIN = 9;// Toggle Switches 
const int H_Plus_Pin_PWM = 3; //PMW pin for HV+
const int H_Minus_Pin_PWM = 6; //PMW Pin for HV-
const int HVPlus = 7; // for HV+
const int HVMinus = 8; // for Hv-

// Control flags
bool isRunning = false;                
bool isPaused = false;                 
bool hasPrintedRunningMessage = false; 

// Function Prototypes
String waitForInput(const char* prompt);
void startMeasurement();
void handleSingleMeasurement();
void handleMultipleMeasurements(); // Updated function
long waitForLongInput(const char* prompt);
float waitForFloatInput(const char* prompt);
void clearSerialBuffer();
bool simulateMeasurement(long time, float voltage);
bool processSerialCommands();
void discharge();
bool askToContinue();
String getPolarityString();

// Setup function
void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  // Configure pins as output
  pinMode(H_PIN, OUTPUT);
  pinMode(H_Plus_Pin_PWM, OUTPUT);
  pinMode(H_Minus_Pin_PWM, OUTPUT);
  pinMode(HVPlus, OUTPUT);
  pinMode(HVMinus, OUTPUT);

  // Welcome message
  Serial.println(F("Welcome to the Measurement System"));
  Serial.println(F("Type 'start' to begin, 'pause' to pause, or 'stop' to stop."));
}

// Main loop
void loop() {
  // Process serial commands
  processSerialCommands();

  // Check if the measurement is running, paused, or stopped, and print appropriate messages
  if (isRunning && !hasPrintedRunningMessage) {
    Serial.println(F("A measurement process is running. Type 'pause' to pause or 'stop' to stop."));
    hasPrintedRunningMessage = true; // Ensure message is printed only once
  }

  if (isPaused && !hasPrintedRunningMessage) {
    Serial.println(F("Measurement is paused. Type 'start' to resume or 'stop' to terminate."));
    hasPrintedRunningMessage = true; // Ensure pause message is printed only once
  }

  if (!isRunning && !isPaused) {
    hasPrintedRunningMessage = false; // Reset when stopped
  }
  
}

// Process serial commands like start, pause, or stop
bool processSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove extra spaces or newlines

    if (command.equalsIgnoreCase("stop")) {
      Serial.println(F("System discharged and stopped. Type 'start' to begin again."));
      discharge(); 
      isRunning = false;
      isPaused = false;
      return false; 
    }

    if (command.equalsIgnoreCase("start")) {
      if (isPaused) {
        isPaused = false; // Resume if paused
        Serial.println(F("Resuming measurement..."));
      } else if (!isRunning) {
        isRunning = true;
        isPaused = false;
        hasPrintedRunningMessage = false; // Reset the flag for a new message
        startMeasurement();
      } else {
        Serial.println(F("Measurement already running. Type 'pause' or 'stop' if needed."));
      }
    } else if (command.equalsIgnoreCase("pause")) {
      if (isRunning && !isPaused) {
        isPaused = true; // Pause if running
        Serial.println(F("Measurement paused. Type 'start' to resume or 'stop' to terminate."));
      } else if (!isRunning) {
        Serial.println(F("No measurement is running to pause. Type 'start' to resume."));
      } else {
        Serial.println(F("Measurement is already paused."));
      }
    } else {
      Serial.println(F("Unknown command. Use 'start', 'pause', or 'stop'."));
    }
  }
  return true; // Indicates that the system is still running
}

// Start measurement function
void startMeasurement() {
  String input;
  while (true) {
    bool goBack = false; 

    // Input for H+ maximum voltage
    while (true) {
      Serial.println(F("Please enter the maximum output voltage for the H+ source (e.g., 100000 for 100kV):"));
      input = waitForInput("Max output voltage for H+ (in volts):");
      if (!isRunning) {
        return; 
      }
      if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to the previous menu...")); 
        hasPrintedRunningMessage = true; 
        goBack = true;
        break;
      }
      maxOutputVoltagePlus = input.toFloat();
      if (maxOutputVoltagePlus <= 0 || maxOutputVoltagePlus > 300000) {
        Serial.println(F("Invalid input. Please enter a number between 1 and 300000 for H+ maximum output voltage."));
        continue; 
    }
      Serial.println(String(F("You entered Max H+ = ")) + maxOutputVoltagePlus + F(" volts"));
      break; 
    }
    if (goBack) {
      continue;
    }

    // Input for H- maximum voltage
    while (true) {
      Serial.println(F("Please enter the maximum output voltage for the H- source (e.g., 50000 for 50kV):"));
      input = waitForInput("Max output voltage for H- (in volts):");
      if (!isRunning) {
        return; 
      }
      if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to the previous menu..."));
        hasPrintedRunningMessage = true;
        goBack = true;
        break;
      }
      maxOutputVoltageMinus = input.toFloat();
      if (maxOutputVoltageMinus <= 0 || maxOutputVoltageMinus > 300000) {
        Serial.println(F("Invalid input. Please enter a number between 1 and 300000 for H- maximum output voltage."));
        continue; 
    }
      Serial.println(String(F("You entered Max H- = ")) + maxOutputVoltageMinus + F(" volts"));
      break;
    }
    if (goBack) {
      continue;
    }

    // Ask for measurement type
    while (true) {
      Serial.println(F("Please select the type of measurement:"));
      Serial.println(F("1: Single Measurement"));
      Serial.println(F("2: Multiple Measurements"));
      Serial.println(F("Type 'back' at any point to return to this menu."));

      clearSerialBuffer(); 

      input = waitForInput("Enter your choice (1 or 2):");
      if (!isRunning) {
        Serial.println(F("System discharged. Type 'start' to begin again."));
        return;
      }
      if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to re-enter H+ and H- maximum voltages..."));
        break;
      }

      if (input == "1") {
        Serial.println(F("User selected Single Measurement."));
        handleSingleMeasurement();
        if (!isRunning) {
          return;
        }
      } 
      else if (input == "2") {
        Serial.println(F("User selected Multiple Measurements."));
        handleMultipleMeasurements(); // <-- Our new simplified approach
        if (!isRunning) {
          return;
        }
      } 
      else {
        Serial.println(F("Invalid input. Please enter 1 or 2."));
      }

      if (!isRunning) {
        Serial.println(F("System discharged and stopped. Exiting to main menu."));
        break;
      }
    }
  }
}
// Handle single measurement
void handleSingleMeasurement() {
  String input;
  while (isRunning) {
    Serial.println(F("Single Measurement selected."));
    Serial.println(F("Enter time in seconds or type 'back' to return to the previous menu."));

    input = waitForInput("Enter measurement time (seconds):");
    if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to the previous menu..."));
        return;
    }
    if (!isRunning) {  // Graceful exit if 'stop' was entered
        return;
    }
    tm = input.toInt();
    if (tm < 10) {
        Serial.println(F("Invalid input. Time must be at least 10 seconds."));
        continue;
    }

    // Calculate minimum increments locally
    float minIncrementPlus = maxOutputVoltagePlus / 255.0;
    float minIncrementMinus = maxOutputVoltageMinus / 255.0;

    while (true) {
      input = waitForInput("Enter voltage (V) or type 'back' to return to the previous menu:");
      if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to the previous menu..."));
        return;
      }
      if (!isRunning) {  // Graceful exit if 'stop' was entered
        return;
      }
      Vm = input.toFloat();
            
      // Determine which increment to compare against based on sign of Vm
      float minIncrement = (Vm > 0) ? minIncrementPlus : minIncrementMinus;

      if (Vm > 0 && Vm > maxOutputVoltagePlus) {
        Serial.println(F("Warning: Voltage exceeds the maximum output voltage for H+. Capping to max output voltage."));
        Vm = maxOutputVoltagePlus;
      } else if (Vm < 0 && fabs(Vm) > maxOutputVoltageMinus) {
        Serial.println(F("Warning: Voltage exceeds the maximum output voltage for H-. Capping to max output voltage."));
        Vm = -maxOutputVoltageMinus; 
      } else if (Vm == 0) {
        Serial.println(F("Invalid input. Voltage must be non-zero."));
        continue;
      }

      if (fabs(Vm) < minIncrement) {
        Serial.print(F("Invalid input. The smallest resolvable increment for this range is about "));
        Serial.print(minIncrement, 2);
        Serial.println(F(" V. Please enter a value at least this large."));
        continue;
      }
      break;
    }
    String polarity = getPolarityString(Vm);
    Serial.println();
    Serial.print(F("Time = "));
    Serial.print(tm);
    Serial.print(F(" s,  Voltage = "));
    Serial.print(Vm, 2);
    Serial.print(F(" V, Polarity = "));
    Serial.println(polarity);
    Serial.println(F("Parameters saved.\n"));

    if (Vm > 0) {
      Serial.println(F("Switched to HV+ voltage source."));
    } else {
      Serial.println(F("Switched to HV- voltage source."));
    }

    Serial.println(F("Starting measurement process..."));
    delay(1000);

    bool completed = simulateMeasurement(tm, Vm);

    if (completed) {
      Serial.println(F("Single measurement process completed."));
      discharge(); // Here, we discharge explicitly
      if (askToContinue()) {
        return; 
      } else {
        isRunning = false;
        isPaused = false;
        Serial.println(F("System discharged. Type 'start' to begin again."));
        return;
      }
    } else {
      return;
    }
  }
}
// Handle multiple measurements
void handleMultipleMeasurements() {
  String input;

  while (isRunning) {
    Serial.println(F("Multiple Measurements selected."));
    Serial.println(F("Type 'back' at any point to return to the previous menu."));

    // 1. Ask how many voltages
    input = waitForInput("Enter the number of voltages to measure:");
    if (!isRunning) return;
    if (input.equalsIgnoreCase("back")) {
      Serial.println(F("Returning to the previous menu..."));
      return;
    }
    numMeasurements = input.toInt();
    if (numMeasurements <= 0 || numMeasurements > 100) {
      Serial.println(F("Invalid input. Please enter a positive number between 1 to 100."));
      continue;
    }

    // Prepare arrays to store voltages & times
    long times[numMeasurements];
    float voltages[numMeasurements];

    // Calculate minimum increments locally
    float minIncrementPlus = maxOutputVoltagePlus / 255.0;
    float minIncrementMinus = maxOutputVoltageMinus / 255.0;

    // 2. For each measurement i, ask for voltage & time
    for (int i = 0; i < numMeasurements && isRunning; i++) {
      Serial.print(F("Configuring Measurement "));
      Serial.print(i + 1);
      Serial.println(F(":"));

      // Voltage
      while (true) {
        input = waitForInput("Enter voltage (V):");
        if (!isRunning) return;
        if (input.equalsIgnoreCase("back")) {
          Serial.println(F("Returning to the previous menu..."));
          return;
        }
        float voltage = input.toFloat();
        // Figure out minimum increment
        float minIncrement = (voltage > 0) ? minIncrementPlus : minIncrementMinus;

        if (voltage > 0 && voltage > maxOutputVoltagePlus) {
          Serial.println(F("Warning: Voltage exceeds the maximum for H+. Capping to H+ max."));
          voltage = maxOutputVoltagePlus;
        } else if (voltage < 0 && fabs(voltage) > maxOutputVoltageMinus) {
          Serial.println(F("Warning: Voltage exceeds the maximum for H-. Capping to H- max."));
          voltage = -maxOutputVoltageMinus;
        } else if (voltage == 0) {
          Serial.println(F("Invalid input. Voltage must be non-zero."));
          continue;
        }

        if (fabs(voltage) < minIncrement) {
          Serial.print(F("Invalid input. The smallest resolvable increment is about "));
          Serial.print(minIncrement, 6);
          Serial.println(F(" V. Please enter a value at least this large."));
          continue;
        }

        voltages[i] = voltage;
        break;
      }

      // Time
      while (true) {
        input = waitForInput("Enter measurement time (seconds):");
        if (!isRunning) return;
        if (input.equalsIgnoreCase("back")) {
          Serial.println(F("Returning to the previous menu..."));
          return;
        }

        long t = input.toInt();
        if (t < 10) {
          Serial.println(F("Invalid input. Time must be at least 10 seconds."));
          continue;
        }
        times[i] = t;
        break;
      }

      //Serial.println(F("Parameters saved."));
    }

    // If user typed stop in the middle
    if (!isRunning) return;

    // 3. Ask how many repetitions
    int repetitions = 0;
    while (true) {
      input = waitForInput("How many repetitions do you want for the entire sequence?");
      if (!isRunning) return;
      if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to the previous menu..."));
        return;
      }

      repetitions = input.toInt();
      if (repetitions <= 0) {
        Serial.println(F("Invalid input. Enter a positive integer."));
      } else {
        break;
      }
    }
    for (int i = 0; i < numMeasurements; i++) {
      String polarity = getPolarityString(voltages[i]);
      Serial.print(F("  Measurement "));
      Serial.print(i + 1);
      Serial.print(F(": Voltage= "));
      Serial.print(voltages[i]);
      Serial.print(F("V, Time= "));
      Serial.print(times[i]);
      Serial.print(F("s, Polarity= "));
      Serial.println(polarity);
    }
    // Print a single "Parameters saved" for the entire set
    Serial.println(F("Parameters saved.\n"));

    // 4. Execute the entire sequence for the given number of repetitions
    String lastPolarity = "OFF";
    for (int rep = 0; rep < repetitions && isRunning; rep++) {
      Serial.print(F("Starting repetition #"));
      Serial.println(rep + 1);

      for (int i = 0; i < numMeasurements && isRunning; i++) {
        Serial.print(F("  Starting measurement "));
        Serial.print(i + 1);
        Serial.print(F(" of "));
        Serial.print(numMeasurements);
        String currentPolarity = getPolarityString(voltages[i]);
        if (currentPolarity != lastPolarity) {
          if (currentPolarity == "HV+") {
            Serial.println(F("  Switched to HV+ voltage source."));
          } else if (currentPolarity == "HV-") {
            Serial.println(F("  Switched to HV- voltage source."));
          }
          // If it's "OFF", you can skip or handle differently
        }
        Serial.println(F("..."));
        delay(1000); // Just a small delay before each measurement
        bool completed = simulateMeasurement(times[i], voltages[i]);
        if (!completed || !isRunning) return;
        lastPolarity = currentPolarity;
        Serial.println(F("  Measurement complete."));
        if (i < (numMeasurements - 1)) {
            float current = voltages[i];
            float next    = voltages[i+1];

            // Polarity difference: one >= 0, the other < 0, or vice versa
            bool differentPolarity = ((current >= 0) && (next < 0)) 
                                  || ((current < 0) && (next >= 0));

            if (differentPolarity) {
                discharge();
                Serial.println(F("Discharged due to polarity change."));
            }
        } 
        else {
            discharge();
            Serial.println(F("Discharged at the end of the sequence."));
        }
      }
    }

    if (!isRunning) return;

    Serial.println(F("All multiple measurements completed."));
    if (!askToContinue()) return; 
  }
}
String getPolarityString(float voltage) {
  if (voltage > 0) return "HV+";
  else if (voltage < 0) return "HV-";
  else return "OFF"; // or "0 V"
}
// Helper function for input
String waitForInput(const char* prompt) {
  // Print the prompt once
  Serial.println(prompt);

  // Record the time at which we start waiting
  unsigned long startWaitTime = millis();
  const unsigned long TIMEOUT = 180000UL; // 3 minutes in ms

  // We'll read input inside this loop
  while (true) {
    // If we've been stopped or paused elsewhere, return immediately
    if (!isRunning || isPaused) {
      return "";
    }

    // If user typed something, read and process it
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();

      // Check special commands
      if (input.equalsIgnoreCase("back")) {
        return "back";
      }
      if (input.equalsIgnoreCase("stop")) {
        discharge(); 
        isRunning = false; 
        isPaused = false;
        Serial.println(F("System discharged and stopped. Type 'start' to begin again."));
        return "";
      }

      // Validate numeric input (digits, decimal, minus sign)
      bool isValid = true;
      for (int i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        if (!isDigit(c) && c != '.' && c != '-') {
          isValid = false;
          break;
        }
      }

      if (isValid && input.length() > 0) {
        return input;  // Return the valid input to caller
      }

      // If invalid input, notify and re-prompt
      Serial.println(F("Invalid input. Please enter a numerical value, 'back', or 'stop'."));
      Serial.println(prompt); // re-print the prompt
      // Reset the timer after re-prompt if desired (optional)
      startWaitTime = millis();
    }

    // Otherwise, user hasn't typed anything yet, so keep waiting
    processSerialCommands();
    delay(10);

    // Check if we've exceeded 3 minutes with no user input
    if (millis() - startWaitTime > TIMEOUT) {
      // Time out: discharge and reset the system
      discharge();
      isRunning = false;
      isPaused = false;

      Serial.println(F("No user input for 3 minutes. System timed out."));
      Serial.println(F("Type 'start' to begin again."));

      // Return an empty string so the caller knows we timed out
      return "";
    }
  } // end while
} // end function


float waitForFloatInput(const char* prompt) {
  float value = 0.0;
  while (true) {
    if (!isRunning || isPaused) return 0.0;
    Serial.println(prompt);

    while (Serial.available() == 0) {
      if (!isRunning || isPaused) return 0.0; 
      processSerialCommands(); 
      delay(10);
    }

    value = Serial.parseFloat();
    if (value != 0 || Serial.peek() == '\n') {
      clearSerialBuffer(); 
      return value;
    }
    Serial.println(F("Invalid input. Please enter a valid number."));
    clearSerialBuffer(); 
  }
}

void clearSerialBuffer() {
  while (Serial.available() > 0) {
    Serial.read(); 
  }
}

void discharge() {
  digitalWrite(H_PIN, LOW);
  analogWrite(H_Plus_Pin_PWM, 0);
  analogWrite(H_Minus_Pin_PWM, 0);
  digitalWrite(HVPlus, LOW);
  digitalWrite(HVMinus, LOW);
}

bool askToContinue() {
  while (true) {
    Serial.println(F("Do you want to continue measuring? (yes/no)"));

    String input = "";
    while (Serial.available() == 0) {
      delay(10);
    }
    input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("yes")) {
      return true; 
    } else if (input.equalsIgnoreCase("no")) {
      Serial.println(F("Exiting the measurement system..."));
      discharge();
      isRunning = false;
      isPaused = false;
      Serial.println(F("System stopped. Type 'start' to begin a new measurement."));
      return false;
    } else {
      Serial.println(F("Invalid input. Please type 'yes' or 'no'."));
    }
  }
}

//---------------------------------------------------------------------
// Simulation of the measurement
//---------------------------------------------------------------------
bool simulateMeasurement(long time, float voltage) {
  Serial.print(F("Simulating "));
  Serial.print(time);
  Serial.print(F(" s at "));
  Serial.print(voltage);
  Serial.println(F(" V(requested)."));

  // Determine scaling based on sign
  float scaledVoltage = 0.0;
  if (voltage > 0) {
      scaledVoltage = (voltage / maxOutputVoltagePlus) * 5.0;
  } else if (voltage < 0) {
      scaledVoltage = (voltage / maxOutputVoltageMinus) * 5.0;
  } else {
      scaledVoltage = 0.0;
  }

  // Limit scaledVoltage to ±5V
  if (scaledVoltage > 5.0)  scaledVoltage = 5.0;
  if (scaledVoltage < -5.0) scaledVoltage = -5.0;

  Serial.print(F("Scaled to "));
  Serial.print(scaledVoltage, 2);
  Serial.println(F(" V."));

  // Determine PWM value
  float absScaled = fabs(scaledVoltage); 
  int pwmValue = (int)((absScaled / 5.0) * 255.0); 
  if (pwmValue > 255) pwmValue = 255;  

  // Set voltage source pins based on polarity
  if (scaledVoltage > 0) {
      digitalWrite(H_PIN, HIGH);
      analogWrite(H_Plus_Pin_PWM, pwmValue);
      analogWrite(H_Minus_Pin_PWM, 0);
      digitalWrite(HVPlus, HIGH);
      digitalWrite(HVMinus, LOW);
  } else if (scaledVoltage < 0) {
      digitalWrite(H_PIN, LOW);
      analogWrite(H_Plus_Pin_PWM, 0);
      analogWrite(H_Minus_Pin_PWM, pwmValue);
      digitalWrite(HVPlus, LOW);
      digitalWrite(HVMinus, HIGH);
  } else {
      digitalWrite(H_PIN, LOW);
      digitalWrite(HVPlus, LOW);
      digitalWrite(HVMinus, LOW);
      analogWrite(H_Plus_Pin_PWM, 0);
      analogWrite(H_Minus_Pin_PWM, 0);
  }

  unsigned long durationInMillis = time * 1000UL;
  unsigned long startTime = millis();

  // Print the voltage only once
  bool hasPrinted = false;

  while (millis() - startTime < durationInMillis) {
      if (!isRunning) {
          discharge(); 
          return false; 
      }
      if (isPaused) {
          while (isPaused && isRunning) {
              processSerialCommands();
              delay(100);
          }
          if (!isRunning) {
              //Serial.println(F("System paused. Type 'start' to begin again."));
              return false;
          }
      }

      // Just printing the requested voltage periodically
      //Serial.println(voltage);
      // Print voltage only the first time
      if (!hasPrinted) {
          Serial.println(voltage);
          hasPrinted = true;
      }
      processSerialCommands();
      delay(100);
  }
  return true;
}
