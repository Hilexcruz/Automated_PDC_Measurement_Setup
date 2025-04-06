// ===== Global Variables =====

long tm;                            // Measurement time in seconds
float Vm;                           // Voltage in volts
float maxOutputVoltagePlus = 0.0;   // HV+ maximum voltage (real-world volts)
float maxOutputVoltageMinus = 0.0;  // HV- maximum voltage (real-world volts)
String user_Date = "";              // User-inputted date (DD-MM-YYYY)
float scaledVoltage = 0.0;
String currentTime = "";            // Current time (HH:MM:SS) – also updated on resume
String globalLastPolarity = "HV+";  // Global polarity variable: default to HV+
String final_date = "";             // End date for logging
String final_time = "";             // End time for logging

// Save original start time (before any resume)
String original_time = "";
String original_date = "";
String current_start_date = "";

// ===== Hardware Pins for HV+ and HV- controls =====
const int H_PIN = 9;            // Toggle switch/control pin
const int H_Plus_Pin_PWM = 3;   // PWM output for HV+
const int H_Minus_Pin_PWM = 6;  // PWM output for HV-
const int HVPlus = 7;           // Indicator LED for HV+
const int HVMinus = 8;          // Indicator LED for HV-

// ===== Control Flags =====
bool isRunning = false;
bool isPaused = false;
bool hasPrintedRunningMessage = false;

// ===== Function Prototypes =====

// -- Utility / Helper Functions --
String waitForInput(const char* prompt, bool ignorePause = false);
String waitForStringInput(const char* prompt);
void clearSerialBuffer();
void discharge();
bool isValidTime(String timeStr);
bool isValidDate(String dateStr);
bool isValidVoltage(String input);
bool isValidMeasurementTime(String input);
String addMinutes(String timeStr, float minutes);
int daysInMonth(int month, int year);
void setHVPlusOutput(float voltage);
void setHVMinusOutput(float voltage);

// -- Core Measurement Function --
bool takeMeasurement(float voltage);

// -- Ramp-Up Function --
void ramp_up(bool opposite, float nextV);

// -- High-Level Measurement Logic --
void handleSingleMeasurement();
void handleMultipleMeasurements();

// -- Top-Level Control Flow --
void startMeasurement();
bool processSerialCommands();

// ===== Setup & Loop =====
void setup() {
  Serial.begin(9600); // Initialize serial communication
  pinMode(H_PIN, OUTPUT);
  pinMode(H_Plus_Pin_PWM, OUTPUT);
  pinMode(H_Minus_Pin_PWM, OUTPUT);
  pinMode(HVPlus, OUTPUT);
  pinMode(HVMinus, OUTPUT);
  Serial.println(F("Welcome to the Polarization/Depolarization Current Measurement System"));
  Serial.println(F("Type 'start' to begin, 'pause' to pause, 'resume' to resume, or 'stop' to stop."));
}

void loop() {
  processSerialCommands(); // Process serial commands
  
  // Print running/paused messages only once
  if (isRunning && !hasPrintedRunningMessage) {
    Serial.println(F("Measurement is running. Type 'pause' to pause or 'stop' to stop."));
    hasPrintedRunningMessage = true;
  }
  if (isPaused && !hasPrintedRunningMessage) {
    Serial.println(F("Measurement is paused. Type 'resume' to resume or 'stop' to stop."));
    hasPrintedRunningMessage = true;
  }
  if (!isRunning && !isPaused) {
    hasPrintedRunningMessage = false;
  }
}

// ===== Process Serial Commands =====
bool processSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove extra spaces/newlines

    if (command.equalsIgnoreCase("stop")) {
      Serial.println(F("System discharged and stopped. Type 'start' to begin again."));
      discharge();
      isRunning = false;
      isPaused = false;
      return false;
    }
    else if (command.equalsIgnoreCase("start")) {
      if (!isRunning) {
        isRunning = true;
        isPaused = false;
        hasPrintedRunningMessage = false;
        startMeasurement();
      }
      else {
        Serial.println(F("Measurement already running. Type 'pause' or 'stop' if needed."));
      }
    }
    else if (command.equalsIgnoreCase("resume")) {
      if (isPaused) {
        Serial.println(F("Please enter a Date and time to resume."));

        // =============== Prompt for a valid Date ===============
        while (true) {
          // Wait even though isPaused is true by using `ignorePause = true`
          String resume_date = waitForInput("Enter new Date (DD-MM-YYYY) or 'stop':", /*ignorePause=*/true);
          if (!isRunning) {
            return false;
          }
          if (resume_date.equalsIgnoreCase("stop")) {
            discharge();
            isRunning = false;
            isPaused = false;
            return false;
          }
          // If user typed commands we don't allow here:
          if (resume_date.equalsIgnoreCase("start") ||
              resume_date.equalsIgnoreCase("pause") ||
              resume_date.equalsIgnoreCase("back"))
          {
            Serial.println(F("That command is not accepted during resume. Enter a valid date or 'stop'."));
            continue; // re-prompt
          }
          // Validate the date
          if (!isValidDate(resume_date)) {
            Serial.println(F("Invalid date. Please try again."));
            continue; // remain in loop
          }
          // Date is valid
          user_Date = resume_date;
          current_start_date = resume_date;
          Serial.print(F("Resume Date = "));
          Serial.println(user_Date);
          break; // exit date loop
        }

        // =============== Prompt for a valid Time ===============
        while (true) {
          String new_time = waitForInput("Enter new Time (HH:MM:SS) or 'stop':", true);

          // If user typed commands we don't allow here:
          if (new_time.equalsIgnoreCase("start") ||
              new_time.equalsIgnoreCase("pause") ||
              new_time.equalsIgnoreCase("back"))
          {
            Serial.println(F("That command is not accepted during resume. Enter a valid time or 'stop'."));
            continue; // re-prompt
          }
          // Validate time
          if (!isValidTime(new_time)) {
            Serial.println(F("Invalid time format. Please try again."));
            continue; // remain in loop
          }
          // Time is valid
          currentTime = new_time;
          Serial.print(F("Resume Time = "));
          Serial.println(currentTime);
          break; // exit time loop
        }

        // Done prompting
        isPaused = false;
        Serial.println(F("Resuming measurement with updated Date and Time..."));
        hasPrintedRunningMessage = false;
      }
      else {
        Serial.println(F("Cannot 'resume' because the system is not paused."));
      }
    }
    else if (command.equalsIgnoreCase("pause")) {
      if (isRunning && !isPaused) {
        isPaused = true;
        Serial.println(F("Measurement paused. Type 'resume' to resume or 'stop' to stop."));
      }
      else if (!isRunning) {
        Serial.println(F("No measurement is running to pause. Type 'start' to begin."));
      }
      else {
        Serial.println(F("Measurement is already paused."));
      }
    }
    else {
      Serial.println(F("Unknown command. Use 'start', 'pause', 'resume', or 'stop'."));
    }
  }
  return true; // System is still running
}


// ===== Updated waitForInput() =====
String waitForInput(const char* prompt, bool ignorePause) {
  Serial.println(prompt);
  unsigned long startWaitTime = millis();
  const unsigned long TIMEOUT = 180000UL; // 3 minutes timeout
  while (true) {
    // Only return an empty string if not ignoring pause
    if (!isRunning || (!ignorePause && isPaused)) return "";
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      if (input.equalsIgnoreCase("back")) return "back";
      if (input.equalsIgnoreCase("stop")) {
        discharge();
        isRunning = false;
        isPaused = false;
        Serial.println("System discharged and stopped. Type 'start' to begin again.");
        return "";
      }
      startWaitTime = millis();
      return input;
    }
    processSerialCommands();
    delay(10);
    if (millis() - startWaitTime > TIMEOUT) {
      discharge();
      isRunning = false;
      isPaused = false;
      Serial.println(F("No user input for 3 minutes. System timed out."));
      Serial.println(F("Type 'start' to begin again."));
      return "";
    }
  }
}

String waitForStringInput(const char* prompt) {
  // (This helper remains unchanged for now.)
  Serial.println(prompt);
  unsigned long startWaitTime = millis();
  const unsigned long TIMEOUT = 180000UL; // 3 minutes
  while (true) {
    if (!isRunning || isPaused) return "";
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      if (input.equalsIgnoreCase("back")) return "back";
      if (input.equalsIgnoreCase("stop")) {
        discharge();
        isRunning = false;
        isPaused = false;
        Serial.println(F("System discharged and stopped. Type 'start' to begin again."));
        return "";
      }
      return input;
    }
    processSerialCommands();
    delay(10);
    if (millis() - startWaitTime > TIMEOUT) {
      discharge();
      isRunning = false;
      isPaused = false;
      Serial.println(F("No user input for 3 minutes. System timed out."));
      Serial.println(F("Type 'start' to begin again."));
      return "";
    }
  }
}

void clearSerialBuffer() {
  while (Serial.available() > 0) {
    Serial.read();
  }
}

void discharge() {
  // Set all outputs to low (or zero) so that the system is "discharged."
  digitalWrite(H_PIN, LOW);
  analogWrite(H_Plus_Pin_PWM, 0);
  analogWrite(H_Minus_Pin_PWM, 0);
  digitalWrite(HVPlus, LOW);
  digitalWrite(HVMinus, LOW);
}

// ===== Start Measurement =====
void startMeasurement() {
  String input;
  while (isRunning) {
    bool goBack = false;
    // Get date with validation
    while (isRunning) {
      input = waitForInput("Enter Date (DD-MM-YYYY): ", true);
      if (!isRunning) return;
      if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to previous menu..."));
        return;
      }
      if (!isValidDate(input)) {
        Serial.println(F("Invalid date format or invalid day. Please use DD-MM-YYYY (e.g., 01-12-2025) starting from 2025"));
        continue;
      }
      user_Date = input;
      if (original_date.length() == 0) {
        original_date = user_Date;
      }
      if (current_start_date.length() == 0) {
        current_start_date = user_Date;
      }
      Serial.print(F("Date Set: "));
      Serial.println(user_Date);

      // Input for H+ maximum voltage
      while (isRunning) {
        Serial.println(F("Please enter the maximum output voltage for the H+ source (e.g., 100000 for 100kV):"));
        input = waitForInput("Max output voltage for H+ (in volts): ", true);
        if (!isRunning) return;
        if (input.equalsIgnoreCase("back")) {
          Serial.println(F("Returning to re-enter date"));
          goBack = true;
          break;
        }
        maxOutputVoltagePlus = input.toFloat();
        if (maxOutputVoltagePlus <= 0 || maxOutputVoltagePlus > 300000) {
          Serial.println(F("Invalid input. Please enter a number between 1 and 300000 for H+ maximum output voltage."));
          continue;
        }
        Serial.println(String(F("You entered Max H+ = ")) + maxOutputVoltagePlus + F(" volts"));

        // Input for H- maximum voltage
        bool voltageBack = false;
        while (isRunning) {
          Serial.println(F("Please enter the maximum output voltage for the H- source (e.g., 50000 for 50kV):"));
          input = waitForInput("Max output voltage for H- (in volts): ", true);
          if (!isRunning) return;
          if (input.equalsIgnoreCase("back")) {
            Serial.println(F("Returning to re-enter H+ maximum voltage..."));
            voltageBack = true;
            break;
          }
          maxOutputVoltageMinus = input.toFloat();
          if (maxOutputVoltageMinus <= 0 || maxOutputVoltageMinus > 300000) {
            Serial.println(F("Invalid input. Please enter a number between 1 and 300000 for H- maximum output voltage."));
            continue;
          }
          Serial.println(String(F("You entered Max H- = ")) + maxOutputVoltageMinus + F(" volts"));

          // Choose measurement mode
          while (isRunning) {
            Serial.println(F("Select measurement type:"));
            Serial.println(F("1: Single Measurement"));
            Serial.println(F("2: Multiple Measurements"));
            Serial.println(F("Type 'back' to re-enter voltage settings."));
            clearSerialBuffer();
            input = waitForInput("Enter choice (1 or 2): ", true);
            if (!isRunning) return;
            if (input.equalsIgnoreCase("back")) {
              Serial.println(F("Returning to re-enter H+ and H- maximum voltages..."));
              goBack = true;
              break; // Exit measurement loop
            }
            if (input == "1") {
              handleSingleMeasurement();
            } else if (input == "2") {
              handleMultipleMeasurements();
            } else {
              Serial.println(F("Invalid input. Please enter 1 or 2."));
              continue;
            }
            if (!isRunning) {
              break;
            }
          }
          if (goBack) { goBack = false; break; }
        }
        if (voltageBack) break;
        if (goBack) break;
      }
      if (goBack) break;
    }
  }
}

// ===== Single Measurement Mode =====
void handleSingleMeasurement() {
  String input;
  Serial.println(F("Single Measurement selected"));
  while (isRunning) {
    // Get Measurement Time
    while (isRunning) {
      input = waitForInput("Enter measurement time in seconds (≥10) or 'back' to exit:", true);
      if (!isRunning || input.equalsIgnoreCase("back")) return;
      int tmInput = input.toInt();
      if (!isValidMeasurementTime(input) || tmInput < 10) {
        Serial.println(F("Invalid time! Enter positive integers (e.g., 300)"));
        continue;
      }
      tm = tmInput;
      Serial.print(F("Measurement Time is set to: "));
      Serial.println(tm);
      
      // Calculate smallest resolvable increments
      float minIncrementPlus = maxOutputVoltagePlus / 255.0;
      float minIncrementMinus = maxOutputVoltageMinus / 255.0;
      
      // Get Measurement Voltage
      while (isRunning) {
        input = waitForInput("Enter voltage (V) or 'back' to go back:", true);
        if (!isRunning) return;
        if (input.equalsIgnoreCase("back")) break;
        float voltageInput = input.toFloat();
        if (!isValidVoltage(input)) {
          Serial.println(F("Invalid voltage! Enter a number (e.g., 150.5 or -50)"));
          continue;
        }
        // Ensure voltage is within range
        if (voltageInput > 0 && voltageInput > maxOutputVoltagePlus) {
          Serial.println(F("Voltage exceeds HV+ maximum. Capping to max."));
          voltageInput = maxOutputVoltagePlus;
        }
        if (voltageInput > 0 && voltageInput < minIncrementPlus) {
          Serial.print(F("Voltage below resolution for HV+ (min ~"));
          Serial.print(minIncrementPlus, 2);
          Serial.println(F(" V). Enter a larger value."));
          continue;
        }
        if (voltageInput < 0 && fabs(voltageInput) > maxOutputVoltageMinus) {
          Serial.println(F("Voltage exceeds HV- maximum. Capping to max."));
          voltageInput = -maxOutputVoltageMinus;
        }
        if (voltageInput < 0 && fabs(voltageInput) < minIncrementMinus) {
          Serial.print(F("Voltage below resolution for HV- (min ~"));
          Serial.print(minIncrementMinus, 2);
          Serial.println(F(" V). Enter a larger value."));
          continue;
        }
        Vm = voltageInput;
        Serial.print(F("Voltage is set to: "));
        Serial.println(Vm);
        
        // Get Current Time (for measurement start)
        while (isRunning) {
          input = waitForInput("Enter current time (HH:MM:SS) (24 hour format) or 'back' to go back:", true);
          if (!isRunning) return;
          Serial.print("Current time is: ");
          Serial.println(input);
          if (input.equalsIgnoreCase("back")) break;
          if (!isValidTime(input)) {
            Serial.println(F("Invalid time format. Please use HH:MM:SS (e.g., 14:30:50)"));
            continue;
          }
          
          // Save the original start time (only once)
          if (original_time.length() == 0) {
            original_time = input;
          }
          currentTime = input;
          // Log the scheduled end time based on tm (converted to minutes)
          String scheduledEnd = addMinutes(currentTime, tm / 60.0);
          // Determine polarity
          String currentPolarity = (Vm > 0) ? "HV+" : (Vm < 0 ? "HV-" : globalLastPolarity);
          if (Vm != 0) globalLastPolarity = currentPolarity;
          Serial.println();
          Serial.print(F("Measurement: Time = "));
          Serial.print(tm);
          Serial.print(F(" s, Voltage = "));
          Serial.print(Vm, 2);
          Serial.print(F(" V, Polarity = "));
          Serial.println(currentPolarity);
          Serial.println(F("Parameters saved."));
          
          // Start Measurement Process
          Serial.println(F("Starting measurement process..."));
          delay(1000);
          int measuringTime = tm;
          original_time = currentTime;
          bool completed = takeMeasurement(Vm);
          // Determine final end time and end date If the measurement was never paused
          String measurement_end_time;
          measurement_end_time = addMinutes(currentTime, measuringTime/60.0);
          final_date = user_Date;
          // if resumed, currentTime should be updated.
          if (!completed) {
            // A pause occurred mid-measurement.
            // When resuming, the resume command should update 'currentTime'.
            // Use currentTime as the new start for the remaining measurement time (tm holds remaining seconds).
            if (!isRunning) {
              // Bail out immediately — do NOT call takeMeasurement again
              return;
            }
            String resumedEnd = addMinutes(currentTime, tm/ 60.0);
            // Complete the remaining measurement time:
            completed = takeMeasurement(Vm);
            final_date = user_Date;
            final_time = resumedEnd;
            if (!completed) return; // if paused again, handle similarly
          }else {final_time = measurement_end_time;}
          if (!completed || !isRunning) return;
          Serial.println(F("Measurement complete."));
          Serial.println(F("#DATA: Single Measurement"));
          Serial.print(F("#DATA: "));
          Serial.print(Vm, 2);
          Serial.print(F("  "));
          Serial.print(measuringTime);
          Serial.print(F("  "));
          Serial.print(original_date);
          Serial.print(F("  "));
          Serial.print(original_time);
          Serial.print(F("  "));
          Serial.print(final_date);
          Serial.print(F("  "));
          Serial.println(final_time);
          
          // Ask if user wants to continue or return to the main menu
          if (!askToContinue()) {
            isRunning = false;
            isPaused = false;
            Serial.println(F("System discharged. Type 'start' to begin again."));
            return;
          } else {
            Serial.println(F("Returning to main menu..."));
            return;
          }
        } // End current time loop
      } // End voltage loop
    } // End measurement time loop
  } // End outer while
}


struct MeasurementSet {
  int nMeas;       // Number of parameters in this set
  int reps;        // Number of repetitions for this set
  float* voltages; // Dynamically allocated array for voltages
  long* times;     // Dynamically allocated array for measurement times

  //ARRAYS for storing each measurement's date
  String* measurementStartTime;
  String* measurementEndTime;
  String* measurementStartDate;
  String* measurementEndDate;
};

void handleMultipleMeasurements() {
  String input;
  
  String measurementStartTime;
  String measurementEndTime;
  String baselineTime;               // Local working variable for the current schedule
  float voltage = 0.0;
  long timeSec = 0;
  int reps = 0;
  Serial.println(F("Multiple Measurements selected."));
  // Enter the number of sets
  while (isRunning) {
    int numSets = 0;
    while (true) {
      input = waitForInput("Enter number of sets (up to 70):", true);
      if (!isRunning) return;
      if (input.equalsIgnoreCase("back")) return;
      numSets = input.toInt();
      if (numSets > 0 && numSets <= 70) break;
      Serial.println(F("Invalid input. Enter a positive number up to 70."));
    }
    MeasurementSet* sets = new MeasurementSet[numSets];
    float minIncrementPlus = maxOutputVoltagePlus / 255.0;
    float minIncrementMinus = maxOutputVoltageMinus / 255.0;

    // Gather data for each set
    for (int s = 0; s < numSets && isRunning;) {
      Serial.print(F("Defining Set #"));
      Serial.println(s + 1);
      int nMeas = 0;
      while (true) {
        input = waitForInput("Enter number of parameters in this measurement set:", true);
        if (!isRunning) return;
        if (input.equalsIgnoreCase("back")) {
          if (s > 0) s--; else return;
        }
        nMeas = input.toInt();
        if (nMeas > 0) break;
        Serial.println(F("Invalid input. Enter a positive number."));
      }
      sets[s].nMeas = nMeas;
      sets[s].voltages = new float[nMeas];
      sets[s].times = new long[nMeas];

      for (int i = 0; i < nMeas && isRunning;) {
        if (input.equalsIgnoreCase("back")) return;
        if (!isRunning) return;
        Serial.print(F("  Configuring measurement "));
        Serial.print(i + 1);
        Serial.print(F(" of "));
        Serial.println(sets[s].nMeas);
        while (true) {
          input = waitForInput("Enter voltage (V):", true);
          if (!isRunning) return;
          if (input.equalsIgnoreCase("back")) {
            if (i > 0) i--; else if (s > 0) s--; else return;
          }
          Serial.print(F("You entered: "));
          Serial.print(input);
          Serial.println(F(" V"));
          if (!isValidVoltage(input)) {
            Serial.println(F("Invalid voltage. Enter a number (e.g., 150.5, -50, or 0)."));
            continue;
          }
          voltage = input.toFloat();
          if (voltage > 0) {
            if (voltage > maxOutputVoltagePlus) {
              Serial.println(F("Voltage exceeds HV+ maximum. Capping to max."));
              voltage = maxOutputVoltagePlus;
            }
            if (voltage < minIncrementPlus) {
              Serial.print(F("Voltage below resolution for HV+ (min ~"));
              Serial.print(minIncrementPlus, 2);
              Serial.println(F(" V). Enter a larger value."));
              continue;
            }
          } else if (voltage < 0) {
            if (fabs(voltage) > maxOutputVoltageMinus) {
              Serial.println(F("Voltage exceeds HV- maximum. Capping to max."));
              voltage = -maxOutputVoltageMinus;
            }
            if (fabs(voltage) < minIncrementMinus) {
              Serial.print(F("Voltage below resolution for HV- (min ~"));
              Serial.print(minIncrementMinus, 2);
              Serial.println(F(" V). Enter a larger value."));
              continue;
            }
          } else if (voltage == 0) {
            if (globalLastPolarity == "HV+" || globalLastPolarity == "")
              setHVPlusOutput(0);
            else if (globalLastPolarity == "HV-")
              setHVMinusOutput(0);
          }
          break;
        }
        sets[s].voltages[i] = voltage;
        // ----- Get measurement time -----
        while (true) {
          input = waitForInput("Enter measurement time in seconds (≥10):", true);
          if (!isRunning) return;
          if (input.equalsIgnoreCase("back")) {
            if (i > 0) i--; else if (s > 0) s--; else return;
          }
          Serial.print(F("You entered: "));
          Serial.print(input);
          Serial.println(F(" s"));
          timeSec = input.toInt();
          if (timeSec >= 10) break;
          Serial.println(F("Time must be at least 10 seconds."));
        }
        sets[s].times[i] = timeSec;
        i++;
      }
      // ----- Get repetitions for the set -----
      while (true) {
        input = waitForInput("Enter number of repetitions for this set:", true);
        if (!isRunning) return;
        if (input.equalsIgnoreCase("back")) {
          if (nMeas > 0) nMeas--; else if (s > 0) s--; else return;
        }
        reps = input.toInt();
        if (reps > 0) break;
        Serial.println(F("Invalid input. Must be a positive integer."));
      }
      sets[s].reps = reps;

      // Allocate memory for time arrays 
      sets[s].measurementStartTime = new String[sets[s].nMeas * sets[s].reps];
      sets[s].measurementEndTime   = new String[sets[s].nMeas * sets[s].reps];
      // NEW: Allocate memory for date arrays 
      sets[s].measurementStartDate = new String[sets[s].nMeas * sets[s].reps];
      sets[s].measurementEndDate   = new String[sets[s].nMeas * sets[s].reps];
      s++;
    }

    // ----- Review entered sets -----
    Serial.println(F("\nYou have entered the following sets:"));
    for (int s = 0; s < numSets; s++) {
      Serial.print(F("Set #"));
      Serial.println(s + 1);
      Serial.print(F("  Repetitions: "));
      Serial.println(sets[s].reps);
      Serial.print(F("  Number of measurements: "));
      Serial.println(sets[s].nMeas);
      Serial.print(F("  Total measurement runs: "));
      Serial.println(sets[s].reps * sets[s].nMeas);
      for (int i = 0; i < sets[s].nMeas; i++) {
        Serial.print(F("    M"));
        Serial.print(i + 1);
        Serial.print(F(": Voltage = "));
        Serial.print(sets[s].voltages[i]);
        Serial.print(F(" V, Time = "));
        Serial.print(sets[s].times[i]);
        Serial.println(F(" s"));
      }
      Serial.println();
    }
    while (true) {
      input = waitForStringInput("Proceed with these measurements? (yes/no):");
      if (!isRunning) return;
      if (input.equalsIgnoreCase("yes")) break;
      if (input.equalsIgnoreCase("no")) return;
      Serial.println(F("Invalid input. Type yes or no."));
    }
    while (true) {
      baselineTime = waitForInput("Enter current time (HH:MM:SS) for starting measurements:", true);
      if (!isRunning) return;
      Serial.print(F("Current Time is: "));
      Serial.println(input);
      if (isValidTime(baselineTime)) break;
      Serial.println(F("Invalid time format. Use HH:MM:SS."));
    }

    // ----- Execute the measurement sets -----
    for (int s = 0; s < numSets && isRunning; s++) {
      Serial.print(F("\n*** Executing Set #"));
      Serial.println(s + 1);
      Serial.print(F(" with "));
      Serial.print(sets[s].nMeas);
      Serial.print(F(" measurements, repeated "));
      Serial.print(sets[s].reps);
      Serial.println(F(" times ***"));
      int j = 0;  // Global measurement counter
      String lastPolarity = "OFF";
      String runningTime = baselineTime;

      for (int rep = 0; rep < sets[s].reps && isRunning; rep++) {
        Serial.print(F("  Starting repetition #"));
        Serial.println(rep + 1);
        for (int i = 0; i < sets[s].nMeas && isRunning; i++, j++) {
          voltage = sets[s].voltages[i];
          timeSec = sets[s].times[i];

          // Determine next voltage for ramp logic
          float nextV;
          if (i < sets[s].nMeas - 1) {
            nextV = sets[s].voltages[i + 1];
          } else {
            // i is last measurement of this repetition
            if (rep < sets[s].reps - 1) {
              // Not the last repetition: restart the same set
              nextV = sets[s].voltages[0];
            } else if (s < numSets - 1) {
              // Last repetition of this set, but there's a next set
              nextV = sets[s+1].voltages[0];
            } else {
              // Last measurement of the last set
              nextV = voltage; // No preramp needed
            }
          }
          // Only ramp if next voltage is opposite polarity

          // Use the previous non-zero polarity when the current measurement is 0
          String previousPolarity = (voltage != 0) ? ((voltage > 0) ? "HV+" : "HV-") : globalLastPolarity;

          // Determine if the next voltage is of opposite polarity relative to the last active polarity
          bool opposite = ((previousPolarity == "HV+" && nextV < 0) || (previousPolarity == "HV-" && nextV > 0));

          if (nextV != voltage && opposite) {
            ramp_up(opposite, nextV);
          }

          String currentPolarity = (voltage > 0) ? "HV+" : (voltage < 0 ? "HV-" : globalLastPolarity);
          if (voltage != 0) globalLastPolarity = currentPolarity;
          if (currentPolarity == "HV+")
            setHVPlusOutput(voltage);
          else
            setHVMinusOutput(voltage);

          if (input.equalsIgnoreCase("back")) return;
          if (!isRunning) return;

          if (measurementStartTime.length() == 0) {
            measurementStartTime = baselineTime;
          }

          // Compute the "scheduled" times for full measurement
          String scheduledStart = runningTime;
          String scheduledEnd   = addMinutes(scheduledStart, timeSec / 60.0);
          // Each measurement's start date is user_Date 
          sets[s].measurementStartDate[j] = user_Date;
          sets[s].measurementEndDate[j]   = user_Date; // default unless pause changes it
          // Store times
          sets[s].measurementStartTime[j] = scheduledStart;
          sets[s].measurementEndTime[j]   = scheduledEnd;

          Serial.println(F("----------------------------------------------------------------------------------------"));
          lastPolarity = currentPolarity;

          // Set global tm to the measurement duration
          tm = timeSec;
          int measuring_duration = timeSec; // Store the original measurement duration
          bool completed = takeMeasurement(voltage);
          if (!isRunning) return;

          // If a pause occurred (takeMeasurement returned false):
          if (!completed) {
            // currentTime holds the new resume time (HH:MM:SS)
            String resumedEnd = addMinutes(currentTime, tm / 60.0);
            // measurement time before pause
            int measuredBeforePause = measuring_duration - tm;
            Serial.print(F("Measurement resumed: "));
            Serial.print(measuredBeforePause);
            Serial.println(F(" s completed before pause."));

            // Update just the END time + date, since we paused mid‐measurement
            sets[s].measurementEndTime[j]   = resumedEnd;
            sets[s].measurementEndDate[j]   = user_Date; 

            // Resume leftover portion
            completed = takeMeasurement(voltage);
            if (!completed) return;

            // Move baseline so subsequent measurements start after resumedEnd
            baselineTime = resumedEnd;
            runningTime  = resumedEnd;
            // Also update date globals so next measurement uses new date
            current_start_date = user_Date;
            final_date         = user_Date;
          }
          else {
            // Measurement completed normally
            runningTime = scheduledEnd;
          }
          Serial.println(F("    Measurement complete."));
        }
      }
      // After the entire set completes, move baselineTime to where we left off
      baselineTime = runningTime;
      Serial.println("****************************************************************************");
    }

    Serial.println("\nAll sets completed. Discharging...");
    int globalMeasurementIndex = 1;
    
    // FINAL PRINT: Print the date/time 
    for (int s = 0; s < numSets; s++) {
      int index = 0;
      for (int rep = 0; rep < sets[s].reps; rep++) {
        Serial.print(F("#DATA: Set "));
        Serial.print(s + 1);
        Serial.print(F(", Repetition "));
        Serial.println(rep + 1);
        for (int j = 0; j < sets[s].nMeas; j++, index++) {
          Serial.print(F("#DATA: M"));
          Serial.print(globalMeasurementIndex);
          Serial.print(F(": "));
          Serial.print(sets[s].voltages[j], 2);
          Serial.print(F(" "));
          Serial.print(sets[s].times[j]);
          Serial.print(F(" "));
          Serial.print(sets[s].measurementStartDate[index]); 
          Serial.print(F(" "));
          Serial.print(sets[s].measurementStartTime[index]); 
          Serial.print(F(" "));
          Serial.print(sets[s].measurementEndDate[index]);   
          Serial.print(F(" "));
          Serial.print(sets[s].measurementEndTime[index]);   
          Serial.println();
          globalMeasurementIndex++;
        }
      }
    }

    discharge();
    if (!askToContinue()) {
      isRunning = false;
      isPaused = false;
      Serial.println(F("System discharged. Type 'start' to begin again."));
      return;
    }
    else {
      return;
    }
  }
}


// ===== Helper Functions =====
void setHVPlusOutput(float voltage) {
  scaledVoltage = (voltage / maxOutputVoltagePlus) * 3.3;
  if (scaledVoltage > 3.3) scaledVoltage = 3.3;
  if (scaledVoltage < 0.0) scaledVoltage = 0.0;
  int pwm = (int)((scaledVoltage / 3.3) * 255.0);
  if (pwm > 255) pwm = 255;
  if (voltage == 0) pwm = 0;
  analogWrite(H_Plus_Pin_PWM, pwm);
  digitalWrite(H_PIN, HIGH);
  digitalWrite(HVPlus, HIGH);
  digitalWrite(HVMinus, LOW);
}

void setHVMinusOutput(float voltage) {
  scaledVoltage = (voltage / maxOutputVoltageMinus) * 3.3;
  if (scaledVoltage < -3.3) scaledVoltage = -3.3;
  if (scaledVoltage > 0.0) scaledVoltage = 0.0;
  int pwm = (int)((fabs(scaledVoltage) / 3.3) * 255.0);
  if (pwm > 255) pwm = 255;
  if (voltage == 0) pwm = 0;
  analogWrite(H_Minus_Pin_PWM, pwm);
  digitalWrite(H_PIN, LOW);
  digitalWrite(HVPlus, LOW);
  digitalWrite(HVMinus, HIGH);
}

void ramp_up(bool opposite, float nextV) {
  if (opposite) {
    if (nextV > 0) {
      setHVPlusOutput(nextV);
      Serial.print(F("    (Pre-ramping HV+ to "));
      Serial.print(nextV);
      Serial.println(F(" V)"));
    } else {
      setHVMinusOutput(nextV);
      Serial.print(F("    (Pre-ramping HV- to "));
      Serial.print(nextV);
      Serial.println(F(" V)"));
    }
  }
}

bool isValidDate(String dateStr) {
  if (dateStr.length() != 10) return false;
  if (dateStr.charAt(2) != '-' || dateStr.charAt(5) != '-') return false;
  String dayStr = dateStr.substring(0, 2);
  String monthStr = dateStr.substring(3, 5);
  String yearStr = dateStr.substring(6, 10);
  for (int i = 0; i < dayStr.length(); i++) {
    if (!isDigit(dayStr.charAt(i))) return false;
  }
  for (int i = 0; i < monthStr.length(); i++) {
    if (!isDigit(monthStr.charAt(i))) return false;
  }
  for (int i = 0; i < yearStr.length(); i++) {
    if (!isDigit(yearStr.charAt(i))) return false;
  }
  int day = dayStr.toInt();
  int month = monthStr.toInt();
  int year = yearStr.toInt();
  if (month < 1 || month > 12) return false;
  int maxDay = daysInMonth(month, year);
  if (day < 1 || day > maxDay) return false;
  if (year < 2025 || year > 2100) return false;
  return true;
}

int daysInMonth(int month, int year) {
  switch (month) {
    case 1: case 3: case 5: case 7: case 8: case 10: case 12: 
      return 31;
    case 4: case 6: case 9: case 11: 
      return 30;
    case 2:
      if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
        return 29;
      else
        return 28;
    default: 
      return 0;
  }
}

bool isValidTime(String timeStr) {
  if (timeStr.length() != 8) return false;
  if (timeStr.charAt(2) != ':' || timeStr.charAt(5) != ':') return false;
  String hourStr = timeStr.substring(0, 2);
  String minuteStr = timeStr.substring(3, 5);
  String secondStr = timeStr.substring(6, 8);
  for (int i = 0; i < hourStr.length(); i++) {
    if (!isDigit(hourStr.charAt(i))) return false;
  }
  for (int i = 0; i < minuteStr.length(); i++) {
    if (!isDigit(minuteStr.charAt(i))) return false;
  }
  for (int i = 0; i < secondStr.length(); i++){
    if(!isDigit(secondStr.charAt(i))) return false;
  }
  int hour = hourStr.toInt();
  int minute = minuteStr.toInt();
  int second = secondStr.toInt();
  if (hour < 0 || hour > 23) return false; 
  if (minute < 0 || minute > 59) return false;
  if (second < 0 || second > 60) return false;
  return true;
}

bool isValidVoltage(String input) {
  bool hasDecimal = false;
  for (int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (i == 0 && c == '-') continue;
    if (c == '.' && !hasDecimal) {
      hasDecimal = true;
      continue;
    }
    if (!isDigit(c)) return false;
  }
  return true;
}

bool isValidMeasurementTime(String input) {
  for (int i = 0; i < input.length(); i++) {
    if (!isDigit(input.charAt(i))) return false;
  }
  return true;
}

String addMinutes(String timeStr, float minutes) {
  int hour = timeStr.substring(0, 2).toInt();
  int minute = timeStr.substring(3, 5).toInt();
  int second = timeStr.substring(6, 8).toInt();
  int extraSeconds = (minutes - int(minutes)) * 60;
  minute += int(minutes);
  second += extraSeconds;
  hour += minute / 60;
  minute = minute % 60;
  hour = hour % 24;
  if (second >= 60) {
    minute += second / 60;
    second = second % 60;
  }
  return (hour < 10 ? "0" : "") + String(hour) + ":" +
         (minute < 10 ? "0" : "") + String(minute) + ":" +
         (second < 10 ? "0" :"") + String(second);
}

String getPolarityString(float voltage) {
  if (voltage > 0) return "HV+";
  else if (voltage < 0) return "HV-";
  else return globalLastPolarity;
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
    if (input.equalsIgnoreCase("yes")) return true;
    else if (input.equalsIgnoreCase("no")) {
      Serial.println(F("Exiting the measurement system..."));
      discharge();
      isRunning = false;
      isPaused = false;
      Serial.println(F("System stopped. Type 'start' to begin a new measurement."));
      return false;
    } else {
      Serial.println(F("Invalid input. Type 'yes' or 'no'."));
    }
  }
}

void addMinutesToDateTime(const String &dateStr, const String &timeStr, float minutesToAdd, String &resultDate, String &resultTime) {
  int day = dateStr.substring(0, 2).toInt();
  int month = dateStr.substring(3, 5).toInt();
  int year = dateStr.substring(6, 10).toInt();
  int hour = timeStr.substring(0, 2).toInt();
  int minute = timeStr.substring(3, 5).toInt();
  int second = timeStr.substring(6, 8).toInt();
  int totalMinutes = hour * 60 + minute;
  totalMinutes += (int)minutesToAdd;
  int newHour = totalMinutes / 60;
  int newMinute = totalMinutes % 60;
  int extraDays = newHour / 24;
  newHour = newHour % 24;
  while (extraDays > 0) {
    int daysThisMonth = daysInMonth(month, year);
    if (day + extraDays > daysThisMonth) {
      extraDays -= (daysThisMonth - day + 1);
      day = 1;
      month++;
      if (month > 12) { month = 1; year++; }
    } else {
      day += extraDays;
      extraDays = 0;
    }
  }
  resultDate = (day < 10 ? "0" : "") + String(day) + "-" +
               (month < 10 ? "0" : "") + String(month) + "-" +
               String(year);
  resultTime = (newHour < 10 ? "0" : "") + String(newHour) + ":" +
               (newMinute < 10 ? "0" : "") + String(newMinute) + ":" +
               (second < 10 ? "0" : "") + String(second);
}

bool takeMeasurement(float voltage) {
  Serial.print(F("Measuring "));
  Serial.print(tm);
  Serial.print(F(" s at "));
  Serial.print(voltage);
  Serial.println(F(" V (requested)."));
  if (voltage > 0) {
    setHVPlusOutput(voltage);
  } else if (voltage < 0) {
    setHVMinusOutput(voltage);
  } 
  Serial.print(F("Scaled to "));
  Serial.print(scaledVoltage, 2);
  Serial.println(F(" V ."));
  unsigned long durationInMillis = tm * 1000UL;
  unsigned long startTime = millis();
  bool hasPrinted = false;
  while (millis() - startTime < durationInMillis) {
    if (!isRunning) {
      discharge();
      return false;
    }
    if (isPaused) {
      unsigned long pauseStart = millis();
      while (isPaused && isRunning) {
        processSerialCommands();
        delay(100);
      }
      unsigned long pauseDuration = millis() - pauseStart;
      startTime += pauseDuration; // Adjust startTime so that the pause period is not counted
      if (!isRunning) return false;
      // Calculate remaining measurement time (in seconds) and update global tm
      unsigned long elapsed = millis() - startTime;
      if (elapsed < durationInMillis) {
        tm = (durationInMillis - elapsed) / 1000;
        } else {
        tm = 0;
      }
      // Return false to indicate that the measurement was interrupted
      return false;
    }
    if (!hasPrinted) {
      Serial.println(voltage);
      hasPrinted = true;
    }
    processSerialCommands();
    delay(100);
  }
  tm = 0;  // measurement fully completed
  return true;
}
