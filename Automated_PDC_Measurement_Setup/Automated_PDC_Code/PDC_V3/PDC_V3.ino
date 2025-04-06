// Global Variables
long tm;                  // Measurement time in seconds
float Vm;                 // Voltage in volts
int numMeasurements;      // Number of measurements
float maxOutputVoltagePlus = 0.0;  // This will represent the real-world max voltage corresponding to 5V from Arduino.
float maxOutputVoltageMinus = 0.0;

// Pins for H+ and H- control
const int H_PLUS_PIN = 9;   // pin for H+ voltage source
const int H_MINUS_PIN = 10; // pin for H- voltage source
const int H_Plus_Pin_PWM = 5; // pin for H- voltage source
const int H_Minus_Pin_PWM = 6;// pin for H- voltage source

// Control flags
bool isRunning = false;                // To track if measurement is active
bool isPaused = false;                 // To track if measurement is paused
bool hasPrintedRunningMessage = false; // Prevent repetitive messages

// Function Prototypes
String waitForInput(const char* prompt);
void startMeasurement();
void handleSingleMeasurement();
void handleMultipleMeasurements();
void handleSameParameters();
void handleDifferentParameters();
long waitForLongInput(const char* prompt);
float waitForFloatInput(const char* prompt);
void clearSerialBuffer();
//void initializeMeasurement();
bool simulateMeasurement(long time, float voltage);
void processSerialCommands();
void discharge();
bool askToContinue();

// Setup function
void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  // Configure pins as output
  pinMode(H_PLUS_PIN, OUTPUT);
  pinMode(H_MINUS_PIN, OUTPUT);
  pinMode(H_Plus_Pin_PWM, OUTPUT);
  pinMode(H_Minus_Pin_PWM, OUTPUT);

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
void processSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove extra spaces or newlines

    if (command.equalsIgnoreCase("stop")) {
      Serial.println(F("Exiting the measurement system..."));
      discharge(); // Ensure system is powered down
      isRunning = false;
      isPaused = false;
      return; // Just return, don't halt with while(true).
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
        Serial.println(F("No measurement is running to pause."));
      } else {
        Serial.println(F("Measurement is already paused."));
      }
    } else {
      Serial.println(F("Unknown command. Use 'start', 'pause', or 'stop'."));
    }
  }
}

//start measurement function
void startMeasurement() {
  String input;
  while (true){
    // Input for H+ maximum voltage
    while (true) {
      Serial.println(F("Please enter the maximum output voltage for the H+ source (e.g., 100000 for 100kV):"));
      input = waitForInput("Max output voltage for H+ (in volts):");
      if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to the previous menu..."));
        return;
      }
      if (!isRunning) {
        Serial.println(F("System discharged. Type 'start' to begin again."));
        return;
      }
      maxOutputVoltagePlus = input.toFloat();
      if (maxOutputVoltagePlus <= 0) {
        Serial.println(F("Invalid input. Please enter a number for the H+ maximum output voltage."));
        continue;
      }
      break;
    }
  
    // Ask the user for the maximum output voltage for the H- source
    while (true) {
      Serial.println(F("Please enter the maximum output voltage for the H- source (e.g., 50000 for 50kV):"));
      input = waitForInput("Max output voltage for H- (in volts):");
      if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to the previous menu..."));
        return;
      }
      if (!isRunning) {
        Serial.println(F("System discharged. Type 'start' to begin again."));
        return;
      }
      maxOutputVoltageMinus = input.toFloat();
      if (maxOutputVoltageMinus <= 0) {
        Serial.println(F("Invalid input. Please enter a number for the H- maximum output voltage."));
        continue;
      }
      break;
    }
    // we need to check it here. If the H- loop was broken due to "back", 
    // just continue the outer loop from the top so H+ is re-entered again.
    if (input.equalsIgnoreCase("back")) {
      continue;  // This jumps back to the outer while(true), re‑asking H+ from scratch
    }
    // Menu for selecting measurement type
    while (true) {
      Serial.println(F("Please select the type of measurement:"));
      Serial.println(F("1: Single Measurement"));
      Serial.println(F("2: Multiple Measurements"));
      Serial.println(F("Type 'back' at any point to return to this menu."));
  
      clearSerialBuffer(); // Clear any leftover input
  
      input = waitForInput("Enter your choice (1 or 2):");
      if (!isRunning) {
        // If 'stop' was entered inside waitForInput
        Serial.println(F("System discharged. Type 'start' to begin again."));
        return;
      }
      // If user types 'back' here, jump out to the outer loop, which re-asks for H+ / H- again
      if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to re-enter H+ and H- maximum voltages..."));
        break;
      }
  
      if (input == "1") {
        Serial.println(F("DEBUG: User selected Single Measurement."));
        handleSingleMeasurement();
      } else if (input == "2") {
        Serial.println(F("DEBUG: User selected Multiple Measurements."));
        handleMultipleMeasurements();
      } else {
        Serial.println(F("Invalid input. Please enter 1 or 2."));
      }
      // Break the loop if system is no longer running
      if (!isRunning) {
          Serial.println(F("System stopped. Exiting to main menu."));
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
        Serial.println(F("Enter time in milliseconds or type 'back' to return to the previous menu."));

        input = waitForInput("Enter measurement time (seconds):");
        if (input.equalsIgnoreCase("back")) {
            Serial.println(F("Returning to the previous menu..."));
            return;
        }

        tm = input.toInt();
        if (tm < 10) {
            Serial.println(F("Invalid input. Please enter a positive number for time. Time must be at least 10 seconds."));
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
            Vm = input.toFloat();
            
            // Determine which increment to compare against based on sign of Vm
            float minIncrement = (Vm > 0) ? minIncrementPlus : minIncrementMinus;

            if (Vm > 0 && Vm > maxOutputVoltagePlus) {
                Serial.println(F("Warning: Voltage exceeds the maximum output voltage for H+. Capping to max output voltage."));
                Vm = maxOutputVoltagePlus;
            } else if (Vm < 0 && fabs(Vm) > maxOutputVoltageMinus) {
                Serial.println(F("Warning: Voltage exceeds the maximum output voltage for H-. Capping to max output voltage."));
                Vm = -maxOutputVoltageMinus; // Maintain negative sign
            } else if (Vm == 0) {
                Serial.println(F("Invalid input. Voltage must be non-zero."));// Check if user entered a voltage that is too small to distinguish
                continue;
            }

            if (fabs(Vm) < minIncrement) {
                Serial.print(F("Invalid input. The smallest resolvable increment for this range is about "));
                Serial.print(minIncrement, 2);
                Serial.println(F(" V. Please enter a value at least this large."));
                continue;
            }

            // If we reach here, the voltage is acceptable
            break;
        }

        //initializeMeasurement();
        Serial.println(F("Starting measurement process..."));
        delay(1000);

        // Call simulateMeasurement and check its return value
        bool completed = simulateMeasurement(tm, Vm);

        if (completed) {
            Serial.println(F("Single measurement process completed."));
            if (askToContinue()) {
                return; // Return to the main menu
            } else {
                isRunning = false;
                isPaused = false;
                Serial.println(F("System discharged. Type 'start' to begin again."));
                return;
            }
        } else {
            // No additional stop message here, as it is already handled in simulateMeasurement
            return;
        }
    }
}

// Handle multiple measurements
void handleMultipleMeasurements() {
  String input;
  while (true) {
    // Before asking, check if still running
    if (!isRunning) return;

    Serial.println(F("Multiple Measurements selected."));
    Serial.println(F("1: Same Parameters"));
    Serial.println(F("2: Different Parameters"));
    Serial.println(F("Type 'back' to return to the previous menu."));

    input = waitForInput("Enter your choice (1 or 2):");
    if (!isRunning) {
      // If stop was entered inside waitForInput
      Serial.println(F("System discharged. Type 'start' to begin again."));
      return; 
    }

    if (input.equalsIgnoreCase("back")) {
      Serial.println(F("Returning to the previous menu..."));
      return; 
    }

    if (input == "1") {
      Serial.println(F("DEBUG: User selected Same Parameters."));
      handleSameParameters();
      // After returning from handleSameParameters, check if still running
      if (!isRunning) return;
    } else if (input == "2") {
      Serial.println(F("DEBUG: User selected Different Parameters."));
      handleDifferentParameters();
      if (!isRunning) return;
    } else {
      Serial.println(F("Invalid input. Please enter 1 or 2."));
    }
  }
}
// Handle same parameters
void handleSameParameters() {
    String input;
    while (isRunning) {
        Serial.println(F("Same Parameters Measurement with Sets and Polarization selected."));
        Serial.println(F("Type 'back' at any point to return to the previous menu."));

        input = waitForInput("Enter the number of sets of measurements:");
        if (!isRunning) {
            // If 'stop' was entered, isRunning is false here
            Serial.println(F("System discharged. Type 'start' to begin again."));
            return;
        }
        if (input.equalsIgnoreCase("back")) {
            Serial.println(F("Returning to the previous menu..."));
            return;
        }

        int numSets = input.toInt();
        if (numSets <= 0) {
            Serial.println(F("Invalid input. Please enter a positive number."));
            continue; // re-ask for the number of sets
        }

        long times[numSets];
        float voltages[numSets];
        int repetitions[numSets];
        bool polarizations[numSets];

        // Compute increments locally here, after user-defined maxOutputVoltagePlus/Minus are known
        float minIncrementPlus = maxOutputVoltagePlus / 255.0;
        float minIncrementMinus = maxOutputVoltageMinus / 255.0;

        // Collect parameters for each set
        for (int i = 0; i < numSets; i++) {
            if (!isRunning) return;
            
            Serial.print(F("Configuring Set "));
            Serial.print(i + 1);
            Serial.println(F(":"));

            // Time
            while (true) {
                input = waitForInput("Enter measurement time (seconds):");
                if (!isRunning) return;
                if (input.equalsIgnoreCase("back")) {
                    Serial.println(F("Returning to the previous menu..."));
                    return;
                }
                long time = input.toInt();
                if (time > 10) {
                    times[i] = time;
                    break;
                } else {
                    Serial.println(F("Invalid input. Time must be greater than 10s."));
                }
            }

            // Voltage
            while (true) {
                input = waitForInput("Enter voltage (V):");
                if (!isRunning) return;
                if (input.equalsIgnoreCase("back")) {
                    Serial.println(F("Returning to the previous menu..."));
                    return;
                }
                // Determine min increment based on sign
                float voltage = input.toFloat();
                float minIncrement = (voltage > 0) ? minIncrementPlus : minIncrementMinus;
                if (voltage > 0 && voltage > maxOutputVoltagePlus) {
                    Serial.println(F("Warning: Voltage exceeds the maximum output voltage for H+. Capping to max output voltage."));
                    voltages[i] = maxOutputVoltagePlus;
                } else if (voltage < 0 && fabs(voltage) > maxOutputVoltageMinus) {
                    Serial.println(F("Warning: Voltage exceeds the maximum output voltage for H-. Capping to max output voltage."));
                    voltages[i] = -maxOutputVoltageMinus;
                } else if (fabs(voltage) == 0) {
                    Serial.println(F("Invalid input. Voltage must be non-zero."));
                    continue;
                } else {
                    voltages[i] = voltage;
                }
                //float voltage = input.toFloat();
                if (fabs(voltage) < minIncrement) {
                    Serial.print(F("Invalid input. The smallest resolvable increment is about "));
                    Serial.print(minIncrement, 6);
                    Serial.println(F(" V. Please enter a value at least this large."));
                    continue;
                }
                voltages[i] = voltage;
                break;
            }

            // Repetitions
            while (true) {
                input = waitForInput("Enter the number of repetitions:");
                if (!isRunning) return;
                if (input.equalsIgnoreCase("back")) {
                    Serial.println(F("Returning to the previous menu..."));
                    return;
                }
                int reps = input.toInt();
                if (reps > 0) {
                    repetitions[i] = reps;
                    break;
                } else {
                    Serial.println(F("Invalid input. Repetitions must be greater than 0."));
                }
            }

            // Polarization
            while (true) {
                if (!isRunning) return;
                Serial.println(F("Enable polarity Reversal? (yes/no):"));

                while (Serial.available() == 0) {
                    if (!isRunning) return;
                    delay(10);
                }
                input = Serial.readStringUntil('\n');
                input.trim();

                // If 'stop' is typed here directly
                if (input.equalsIgnoreCase("stop")) {
                    discharge();
                    isRunning = false;
                    isPaused = false;
                    Serial.println("System discharged. Type 'start' to begin again.");
                    return;
                }

                if (!isRunning) return;

                if (input.equalsIgnoreCase("yes")) {
                    polarizations[i] = true;
                    break;
                } else if (input.equalsIgnoreCase("no")) {
                    polarizations[i] = false;
                    break;
                } else if (input.equalsIgnoreCase("back")) {
                    Serial.println(F("Returning to the previous menu..."));
                    return;
                } else {
                    Serial.println(F("Invalid input. Please type 'yes' or 'no'."));
                }
            }

            Serial.println(F("Set parameters saved successfully."));
        }

        if (!isRunning) return;

        // Execute the sets
        for (int i = 0; i < numSets && isRunning; i++) {
            Serial.print(F("Starting Set "));
            Serial.print(i + 1);
            Serial.println(F(":"));

            for (int j = 0; j < repetitions[i] && isRunning; j++) {
                Serial.print(F("  Starting measurement "));
                Serial.print(j + 1);
                Serial.print(F(" of "));
                Serial.print(repetitions[i]);
                Serial.println(F("..."));

                Serial.println(F("Starting measurement process..."));
                delay(1000);

                if (polarizations[i]) {
                    bool completed = simulateMeasurement(times[i] / 2, voltages[i]);
                    if (!completed || !isRunning) return;

                    completed = simulateMeasurement(times[i] / 2, -voltages[i]);
                    if (!completed || !isRunning) return;
                } else {
                    bool completed = simulateMeasurement(times[i], voltages[i]);
                    if (!completed || !isRunning) return;
                }

                Serial.print(F("  Measurement "));
                Serial.print(j + 1);
                Serial.println(F(" complete."));
            }

            if (!isRunning) return;
            Serial.print(F("Set "));
            Serial.print(i + 1);
            Serial.println(F(" completed."));
        }

        if (!isRunning) return;
        Serial.println(F("All sets of measurements completed."));
        if (!askToContinue()) return;
    }
}

// Handle different parameters
void handleDifferentParameters() {
    String input;
    while (isRunning) {
        Serial.println(F("Different Parameters selected."));
        Serial.println(F("Type 'back' at any point to return to the previous menu."));

        input = waitForInput("Enter number of measurements:");
        if (!isRunning) {
            Serial.println(F("System discharged. Type 'start' to begin again."));
            return;
        }
        if (input.equalsIgnoreCase("back")) {
            Serial.println(F("Returning to the previous menu..."));
            return;
        }

        numMeasurements = input.toInt();
        if (numMeasurements <= 0) {
            Serial.println(F("Invalid input. Please enter a positive number."));
            continue;
        }

        long times[numMeasurements];
        float voltages[numMeasurements];
        // Calculate minimum increments locally
        float minIncrementPlus = maxOutputVoltagePlus / 255.0;
        float minIncrementMinus = maxOutputVoltageMinus / 255.0;

        for (int i = 0; i < numMeasurements && isRunning; i++) {
            Serial.print(F("Configuring Measurement "));
            Serial.print(i + 1);
            Serial.println(F(":"));

            while (true) {
                input = waitForInput("Enter measurement time (seconds):");
                if (!isRunning) return;
                if (input.equalsIgnoreCase("back")) {
                    Serial.println(F("Returning to the previous menu..."));
                    return;
                }
                times[i] = input.toInt();
                if (times[i] >= 10) break;
                Serial.println(F("Invalid input. Time must be greater than 10s."));
            }

            while (true) {
                input = waitForInput("Enter voltage (V):");
                if (!isRunning) return;
                if (input.equalsIgnoreCase("back")) {
                    Serial.println(F("Returning to the previous menu..."));
                    return;
                }
                float voltage = input.toFloat();
                // Determine min increment based on sign
                float minIncrement = (voltage > 0) ? minIncrementPlus : minIncrementMinus;

                if (voltage > 0 && voltage > maxOutputVoltagePlus) {
                    Serial.println(F("Warning: Voltage exceeds the maximum output voltage for H+. Capping to max output voltage."));
                    voltages[i] = maxOutputVoltagePlus;
                } else if (voltage < 0 && fabs(voltage) > maxOutputVoltageMinus) {
                    Serial.println(F("Warning: Voltage exceeds the maximum output voltage for H-. Capping to max output voltage."));
                    voltages[i] = -maxOutputVoltageMinus;
                } else if (fabs(voltage) == 0) {
                    Serial.println(F("Invalid input. Voltage must be non-zero."));
                    continue;
                } else {
                    voltages[i] = voltage;
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

            Serial.println(F("Parameters saved."));
        }

        if (!isRunning) return;

        // Execute measurements
        for (int i = 0; i < numMeasurements && isRunning; i++) {
            Serial.print(F("Starting Measurement "));
            Serial.print(i + 1);
            Serial.println(F(":"));

            Serial.println(F("Starting measurement process..."));
            delay(1000);
            bool completed = simulateMeasurement(times[i], voltages[i]);
            if (!completed || !isRunning) return;

            Serial.print(F("Measurement "));
            Serial.print(i + 1);
            Serial.println(F(" complete."));
        }

        if (!isRunning) return;
        Serial.println(F("All different-parameter measurements completed."));
        if (!askToContinue()) return;
    }
}


// Helper function for input
String waitForInput(const char* prompt) {
  String input;
  while (true) {
    if (!isRunning || isPaused) return ""; // Exit on pause or stop
    Serial.println(prompt);

    while (Serial.available() == 0) {
      processSerialCommands(); // Check for stop command during waiting
      delay(10);
      // If stop was processed, isRunning might be false now
      if (!isRunning) return "";
    }

    input = Serial.readStringUntil('\n');
    input.trim(); // Remove spaces/newlines

    if (input.equalsIgnoreCase("back")) {
      return "back";
    }

    if (input.equalsIgnoreCase("stop")) {
      Serial.println(F("Exiting the measurement system..."));
      discharge(); // Ensure system is powered down
      isRunning = false; 
      isPaused = false;
      // IMPORTANT: Return immediately after stop
      return "";
    }

    // Validate numeric input
    bool isValid = true;
    for (int i = 0; i < input.length(); i++) {
      char c = input.charAt(i);
      if (!isDigit(c) && c != '.' && c != '-') { // Allow digits, decimal, negative
        isValid = false;
        break;
      }
    }

    if (isValid && input.length() > 0) {
      return input;
    }

    // If invalid, prompt again
    Serial.println(F("Invalid input. Please enter a numerical value, 'back', or 'stop'."));
  }
}

float waitForFloatInput(const char* prompt) {
  float value = 0.0;
  while (true) {
    if (!isRunning || isPaused) return 0.0; // Exit on pause or stop
    Serial.println(prompt);

    // Wait for user input with periodic command processing
    while (Serial.available() == 0) {
      if (!isRunning || isPaused) return 0.0; // Exit if stopped or paused
      processSerialCommands(); // Allow processing of pause/stop commands
      delay(10);
    }

    value = Serial.parseFloat();
    if (value != 0 || Serial.peek() == '\n') {
      clearSerialBuffer(); // Clear remaining input
      return value;
    }
    Serial.println(F("Invalid input. Please enter a valid number."));
    clearSerialBuffer(); // Clear any junk input
  }
}

void clearSerialBuffer() {
  while (Serial.available() > 0) {
    Serial.read(); // Discard all unread input
  }
}

void discharge() {
  digitalWrite(H_PLUS_PIN, LOW);
  digitalWrite(H_MINUS_PIN, LOW);
  analogWrite(H_Plus_Pin_PWM, LOW);
  analogWrite(H_Minus_Pin_PWM, LOW);
  //Serial.println(F("System discharged. LEDs are off."));
}

bool askToContinue() {
  while (true) {
    Serial.println(F("Do you want to continue measuring? (yes/no)"));

    // Wait for user input
    String input = "";
    while (Serial.available() == 0) {
      delay(10); // Wait for input
    }
    input = Serial.readStringUntil('\n');
    input.trim(); // Remove any extra whitespace

    // Validate the input
    if (input.equalsIgnoreCase("yes")) {
      return true; // User wants to continue
    } else if (input.equalsIgnoreCase("no")) {
      Serial.println(F("Exiting the measurement system..."));
      discharge(); // Power down the system
      isRunning = false;
      isPaused = false;
      Serial.println(F("System stopped. Type 'start' to begin a new measurement."));
      return false; // User wants to stop
    } else {
      Serial.println(F("Invalid input. Please type 'yes' or 'no'."));
    }
  }
}



bool simulateMeasurement(long time, float voltage) {
  Serial.print(F("Simulating "));
  Serial.print(time);
  Serial.print(F(" s at "));
  Serial.print(voltage);
  Serial.println(F(" V(requested)."));

  // Determine scaling based on sign of requestedVoltage
  float scaledVoltage = 0.0;
  if (voltage > 0) {
      // Positive voltage uses maxOutputVoltagePlus
      scaledVoltage = (voltage / maxOutputVoltagePlus) * 5.0;
  } else if (voltage < 0) {
      // Negative voltage uses maxOutputVoltageMinus
      // For a negative requested voltage, say -20000, and maxOutputVoltageMinus = 50000,
      // scaled voltage = (-20000/50000)*5 = -2.0 V
      scaledVoltage = (voltage / maxOutputVoltageMinus) * 5.0;
  } else {
      // Zero voltage is just zero
      scaledVoltage = 0.0;
  }
  // Limit scaledVoltage to ±5V
  if (scaledVoltage > 5.0) scaledVoltage = 5.0;
  if (scaledVoltage < -5.0) scaledVoltage = -5.0;
  Serial.print(scaledVoltage, 2);
  Serial.println(F(" V."));

  // Set voltage source pins based on polarity
  if (scaledVoltage > 0) {
      digitalWrite(H_PLUS_PIN, HIGH);
      digitalWrite(H_MINUS_PIN, LOW);
      analogWrite(H_Plus_Pin_PWM, 64);
      analogWrite(H_Minus_Pin_PWM, 0);
      Serial.println(F("Switched to H+ voltage source."));
  } else if (scaledVoltage < 0) {
      digitalWrite(H_PLUS_PIN, LOW);
      digitalWrite(H_MINUS_PIN, HIGH);
      analogWrite(H_Plus_Pin_PWM, 0);
      analogWrite(H_Minus_Pin_PWM, 64);
      Serial.println(F("Switched to H- voltage source."));
  } else {
      digitalWrite(H_PLUS_PIN, LOW);
      digitalWrite(H_MINUS_PIN, LOW);
      analogWrite(H_Plus_Pin_PWM, 0);
      analogWrite(H_Minus_Pin_PWM, 0);
      Serial.println(F("No voltage source selected (0V)."));
  }

  unsigned long durationInMillis = time * 1000;
  unsigned long startTime = millis();

  while (millis() - startTime < durationInMillis) {
      if (!isRunning) {
          // Centralize "stop" behavior here
          discharge(); // Turn off LEDs
          Serial.println(F("System stopped mid-process. Type 'start' to begin again."));
          return false; // Indicate measurement was stopped
      }

      if (isPaused) {
          while (isPaused && isRunning) {
              processSerialCommands();
              delay(100);
          }
          if (!isRunning) {
              //discharge();
              Serial.println(F("System paused. Type 'start' to begin again."));
              return false;
          }
      }

      Serial.println(voltage);
      processSerialCommands();
      delay(100);
  }

  Serial.println(F("Measurement complete."));
  discharge();
  return true; // Indicate successful completion
    
}