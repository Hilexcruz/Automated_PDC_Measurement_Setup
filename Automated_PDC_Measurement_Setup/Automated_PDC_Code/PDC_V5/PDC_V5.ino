// Include the Wire library for I2C communication with the RTC
#include <Wire.h>

// Include the RTClib library to interface with the DS3231 RTC
#include <RTClib.h>

// Create an RTC object to interact with the DS3231 RTC module
RTC_DS3231 rtc;      

// ===== Global Variables =====

// Variable for measurement time (in seconds) that determines how long a measurement lasts.
long tm;                            

// Voltage variable (in volts) that holds the requested voltage level for measurement.
float Vm;                           

// Maximum voltage for the HV+ (High Voltage positive) source in real-world volts.
float maxOutputVoltagePlus = 0.0;   

// Maximum voltage for the HV- (High Voltage negative) source in real-world volts.
float maxOutputVoltageMinus = 0.0;  

// Date string (format: DD-MM-YYYY) fetched from the RTC
String user_Date = "";              

// Scaled voltage after converting the real voltage to a 0-3.251V analog output range.
float scaledVoltage = 0.0;

// Current time string (format: HH:MM:SS) fetched from the RTC
String currentTime = "";            

// Global variable to store the polarity from the last non-zero voltage measurement.
// Defaults to "HV+" if no measurement has been made yet.
String globalLastPolarity = "HV+";  

// Variables to store the final date and time of the measurement when it ends.
String final_date = "";             
String final_time = "";             

// Feedback check interval (in milliseconds). This value is set to 1 hour.
static const unsigned long FEEDBACK_INTERVAL = 3600000UL; 

// Threshold fraction for voltage drift detection. If the measured voltage differs by more than 0.1% (0.001)
static const float feedbackThreshold = 0.001; // from the setpoint, the system will adjust the voltage.

// Scale factors to convert the high voltage (0-10 V range) to the 0-3.251 V range expected by the Arduino.
static const float hvPlusScaleFactor  =  (10.0f / 3.251f); // These are based on the resistor divider design used.
static const float hvMinusScaleFactor =  (10.0f / 3.251f); // These are based on the resistor divider design used.

// Variables to store the original start time and date (before any resume)
String original_time = "";
String original_date = "";
String current_start_date = "";

// ===== Hardware Pin Definitions =====

// H_PIN is a control pin (e.g., toggle switch) to select between HV+ and HV- outputs.
const int H_PIN = 9;            

// PWM output pin for the HV+ output (controls voltage via PWM).
const int H_Plus_Pin_PWM = 3;   

// PWM output pin for the HV- output (controls voltage via PWM).
const int H_Minus_Pin_PWM = 6;  

// Indicator LED for HV+ state.
const int HVPlus = 7;           

// Indicator LED for HV- state.
const int HVMinus = 8;           

// ===== Control Flags =====

// Boolean flag indicating if the measurement process is currently running.
bool isRunning = false;

// Boolean flag indicating if the measurement process is paused.
bool isPaused = false;

// Boolean flag used to ensure that messages about running/paused states are printed only once.
bool hasPrintedRunningMessage = false;

// ===== Function Prototypes =====

// Utility / Helper function prototypes (used throughout the code)
String waitForInput(const char* prompt, bool ignorePause = false);   // Wait for user input from serial.
String waitForStringInput(const char* prompt);                       // Wait for a string input.
void clearSerialBuffer();                                            // Clear any pending serial data.
void discharge();                                                    // Turn off all outputs to "discharge" the system.
bool isValidTime(String timeStr);                                    // Validate a time string in HH:MM:SS.
bool isValidDate(String dateStr);                                    // Validate a date string in DD-MM-YYYY.
bool isValidVoltage(String input);                                   // Validate a measurement voltage input string.
bool isValidPositiveVoltage(String input);                           // Validate the max-voltages input 
bool isValidMeasurementTime(String input);                           // Validate that the measurement time is a number.
String addMinutes(String timeStr, float minutes);                    // Add minutes to a given time string (still used in some places).
int daysInMonth(int month, int year);                                // Get the number of days in a given month/year.
void setHVPlusOutput(float voltage);                                 // Set the HV+ output voltage using PWM.
void setHVMinusOutput(float voltage);                                // Set the HV- output voltage using PWM.
void checkFeedback();                                                // Check the measured voltage against the desired voltage and adjust if needed.
float readHVplus();                                                  // Read the actual HV+ voltage using the ADC.
float readHVminus();                                                 // Read the actual HV- voltage using the ADC.
void setRTCFromSerial();                                             // Set the RTC time based on serial input.
void feedbackloop();                                                 // feedback function to check feedback periodically.

// Core measurement function that runs a single measurement cycle.
bool takeMeasurement(float voltage);

// Ramp-up function to pre-adjust voltage when switching polarity.
void ramp_up(bool opposite, float nextV);

// High-level measurement logic functions for single and multiple measurement modes.
void handleSingleMeasurement();
void handleMultipleMeasurements();

// Top-level control flow functions.
void startMeasurement();
bool processSerialCommands();

// --- Helper Functions for RTC ---
// Get the current date from the RTC (format: DD-MM-YYYY)
String getCurrentDate() {
  DateTime now = rtc.now(); // Retrieve the current date and time from the RTC module
  char buf[11]; // 11 characters: "DD-MM-YYYY" (10 chars) + 1 for the null terminator (`\0`)
  // Format the date into the buffer using `sprintf`:
  sprintf(buf, "%02d-%02d-%04d", now.day(), now.month(), now.year()); // - `%02d` ensures two-digit formatting for day and month (e.g., "01" instead of "1") - `%04d` ensures four-digit formatting for the year (e.g., "2025" instead of "25")
  return String(buf);
}

// Get the current time from the RTC (format: HH:MM:SS)
String getCurrentTime() {
  DateTime now = rtc.now(); // Retrieve the current date and time from the RTC module
  char buf[9]; // 11 characters: "DD-MM-YYYY" (10 chars) + 1 for the null terminator (`\0`)
  // Format the date into the buffer using `sprintf`:
  sprintf(buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second()); // - `%02d` ensures two-digit formatting for day and month (e.g., "01" instead of "1") - `%04d` ensures four-digit formatting for the year (e.g., "2025" instead of "25")
  return String(buf);
}

// ===== Setup Function =====
void setup() {
  // Initialize serial communication at 9600 baud rate
  Serial.begin(9600);
  
  // Wait until the Serial port is ready (only needed on some boards)
  while(!Serial);
  
  // Set the analog write resolution to 12 bits (0-4095)
  analogWriteResolution(12);
  
  // Set the hardware pins to OUTPUT mode as needed
  pinMode(H_PIN, OUTPUT);
  pinMode(H_Plus_Pin_PWM, OUTPUT);
  pinMode(H_Minus_Pin_PWM, OUTPUT);
  pinMode(HVPlus, OUTPUT);
  pinMode(HVMinus, OUTPUT);
  
  // Initialize the I2C bus for the RTC module
  Wire.begin();
  
  // Try to initialize the RTC; if not found, print an error and halt.
  if (!rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
    while (1);
  }
  
  // If the RTC lost power or time is invalid, prompt the user to set the current time.
  if (rtc.lostPower()) {
    Serial.println(F("RTC has lost power. Please set the current time."));
    setRTCFromSerial();
  }
  // Uncomment the next line to automatically adjust the RTC to compile time (only if needed)
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // Fetch the current date and time from RTC in a single line.
  String currentDate = getCurrentDate();
  String currentTime = getCurrentTime();
  // Welcome message and instructions for starting the measurement system.
  Serial.println(F("Welcome to the Polarization/Depolarization Current Measurement System"));
  Serial.print(F("Current RTC Date & Time: "));
  Serial.print(currentDate);
  Serial.print(F(" "));
  Serial.println(currentTime);
  Serial.println(F("Type 'start' to begin, 'pause' to pause, 'resume' to resume, or 'stop' to stop."));
}

// ===== Main Loop Function =====
void loop() {
  // Always check for serial commands from the user (start, stop, pause, resume)
  processSerialCommands();
  
  // Print messages indicating the system's running state only once to avoid duplicate prints
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
  
  // Call the feedback loop function to check and adjust voltage at defined intervals.
  feedbackloop(); // Run feedback check every hour
}

// ===== Process Serial Commands =====
// This function continuously checks if there is serial input and processes commands accordingly.
bool processSerialCommands() {
  if (Serial.available() > 0) {
    // Read the command until newline and trim whitespace.
    String command = Serial.readStringUntil('\n');
    command.trim();

    // Check for various commands:
    if (command.equalsIgnoreCase("stop")) {
      Serial.println(F("System discharged and stopped. Type 'start' to begin again."));
      discharge();          // Turn off all outputs.
      isRunning = false;    // Stop measurement.
      isPaused = false;
      return false;
    }
    else if (command.equalsIgnoreCase("start")) {
      if (isPaused) {
        Serial.println(F("Measurement is paused. Type 'resume' to continue or 'stop' to discharge."));
    }
      else if (!isRunning) {
        isRunning = true;
        isPaused = false;
        hasPrintedRunningMessage = false;
        startMeasurement();  // Begin the measurement process.
      }
      else {
        Serial.println(F("Measurement already running. Type 'pause' or 'stop' if needed."));
      }
    }
    else if (command.equalsIgnoreCase("resume")) {
      if (isPaused) {
        // Fetch the current RTC date/time.
        user_Date = getCurrentDate();
        currentTime = getCurrentTime();
        current_start_date = user_Date;
        
        Serial.print(F("Resume Date = "));
        Serial.println(user_Date);
        Serial.print(F("Resume Time = "));
        Serial.println(currentTime);

        // Prompt the user to either synchronize the RTC or continue with current time.
        while (true) {
          Serial.println(F("Type 'sync' to set RTC or 'go' to proceed with the measurement."));
          String choice = waitForInput("Enter choice:", true);
          
          if (!isRunning) return false;  // If the user types “stop” or the system is halted.

          if (choice.equalsIgnoreCase("sync")) {
            setRTCFromSerial(); // Adjust the RTC time based on user input.
            // Re-fetch date/time after sync.
            user_Date = getCurrentDate();
            currentTime = getCurrentTime();
            current_start_date = user_Date;

            Serial.print(F("New RTC date = "));
            Serial.println(user_Date);
            Serial.print(F("New RTC time = "));
            Serial.println(currentTime);
            break;  // Exit the loop.
          }
          else if (choice.equalsIgnoreCase("go")) {
            // If the user trusts the RTC time, continue without syncing.
            break;
          }
          else {
            Serial.println(F("Invalid choice. Type 'sync' or 'go'."));
          }
        }

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
  return true; // Return true if the system is still running.
}

// ===== Set RTC from Serial =====
// This function prompts the user to input the current date and time in a specific format,
// then sets the RTC accordingly.
void setRTCFromSerial() {
  Serial.println(F("Please enter the current date and time (DD-MM-YYYY HH:MM:SS):"));
  // Wait until data is available on the serial port.
  while (!Serial.available());
  String datetime = Serial.readStringUntil('\n');
  datetime.trim();
  
  // Basic parsing assuming the input is exactly in the format: DD-MM-YYYY HH:MM:SS
  int day = datetime.substring(0, 2).toInt();
  int month = datetime.substring(3, 5).toInt();
  int year = datetime.substring(6, 10).toInt();
  int hour = datetime.substring(11, 13).toInt();
  int minute = datetime.substring(14, 16).toInt();
  int second = datetime.substring(17, 19).toInt();
  
  // Set the RTC to the provided date and time.
  rtc.adjust(DateTime(year, month, day, hour, minute, second));
  Serial.println(F("RTC updated successfully!"));
}

// ===== waitForInput Function =====
// Waits for user input from Serial after prompting. It supports a timeout and commands to go back or stop.
String waitForInput(const char* prompt, bool ignorePause) {
  Serial.println(prompt);
  unsigned long startWaitTime = millis();
  const unsigned long TIMEOUT = 180000UL; // Timeout set to 3 minutes.
  while (true) {
    // If the system is not running or is paused (unless ignorePause is true), return empty string.
    if (!isRunning || (!ignorePause && isPaused)) return "";
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      // Check if user wants to go back.
      if (input.equalsIgnoreCase("back")) return "back";
      // Check if user wants to stop.
      if (input.equalsIgnoreCase("stop")) {
        discharge();
        isRunning = false;
        isPaused = false;
        Serial.println("System discharged and stopped. Type 'start' to begin again.");
        return "";
      }
      // Reset wait timer and return input.
      startWaitTime = millis();
      return input;
    }
    // Process any other serial commands.
    processSerialCommands();
    delay(10);
    // Check for timeout.
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

// ===== waitForStringInput Function =====
// Similar to waitForInput but dedicated to handling string inputs.
String waitForStringInput(const char* prompt) {
  Serial.println(prompt);
  unsigned long startWaitTime = millis();
  const unsigned long TIMEOUT = 180000UL; // 3 minutes timeout.
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

// ===== clearSerialBuffer Function =====
// Clears out any remaining data in the serial buffer.
void clearSerialBuffer() {
  while (Serial.available() > 0) {
    Serial.read();
  }
}

// ===== discharge Function =====
// This function "discharges" the system by turning off all output pins.
// It is used to safely stop measurements.
void discharge() {
  digitalWrite(H_PIN, LOW);
  analogWrite(H_Plus_Pin_PWM, 0);
  analogWrite(H_Minus_Pin_PWM, 0);
  digitalWrite(HVPlus, LOW);
  digitalWrite(HVMinus, LOW);
}

// ===== Start Measurement =====
// This function handles the process of starting a measurement session.
// It gathers user input for voltage parameters and measurement modes.
void startMeasurement() {
  String input;
  while (isRunning) {
    bool goBack = false;
    // Fetch and display the current date from RTC.
    user_Date = getCurrentDate();
    if (original_date.length() == 0) {
      original_date = user_Date;
    }
    if (current_start_date.length() == 0) {
      current_start_date = user_Date;
    }

    // --- Input for HV+ Maximum Voltage ---
    while (isRunning) {
      Serial.println(F("Please enter the maximum output voltage for the H+ source (e.g., 100000 for 100kV):"));
      input = waitForInput("Max output voltage for H+ (in volts): ", true);
      if (!isRunning) return;
      if (input.equalsIgnoreCase("back")) {
        Serial.println(F("Returning to re-enter date"));
        goBack = true;
        break;
      }
      
      // Validate the input is within a reasonable range.
      if (!isValidPositiveVoltage(input)) {
        Serial.println(F("Invalid input. Please enter a positive numeric number"));
        continue;
      }
      maxOutputVoltagePlus = input.toFloat();
      Serial.println(String(F("You entered Max H+ = ")) + maxOutputVoltagePlus + F(" volts"));

      // --- Input for HV- Maximum Voltage ---
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
        
        // Validate HV- maximum voltage.
        if (!isValidPositiveVoltage(input)) {
          Serial.println(F("Invalid input. Please enter a positive numeric number"));
          continue;
        }
        maxOutputVoltageMinus = input.toFloat();
        Serial.println(String(F("You entered Max H- = ")) + maxOutputVoltageMinus + F(" volts"));

        // --- Choose Measurement Mode (Single or Multiple) ---
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
            break; // Exit measurement loop to allow re-entry.
          }
          if (input == "1") {
            handleSingleMeasurement(); // Call single measurement mode.
          } else if (input == "2") {
            handleMultipleMeasurements(); // Call multiple measurements mode.
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
  }
}

// ===== Single Measurement Mode =====
// This function handles the user interface and logic for a single measurement.
void handleSingleMeasurement() {
  String input;
  Serial.println(F("Single Measurement selected"));
  while (isRunning) {
    // Get the measurement duration (in seconds)
    while (isRunning) {
      input = waitForInput("Enter measurement time in seconds (≥10) or 'back' to exit:", true);
      if (!isRunning || input.equalsIgnoreCase("back")) return;
      int tmInput = input.toInt();
      if (!isValidMeasurementTime(input) || tmInput < 10) {
        Serial.println(F("Invalid time! Enter only positive integers greater than 10 (e.g., 300)"));
        continue;
      }
      tm = tmInput;
      Serial.print(F("Measurement Time is set to: "));
      Serial.println(tm);
      
      // Calculate the smallest resolvable voltage increments based on the ADC resolution.
      float minIncrementPlus = maxOutputVoltagePlus / 4095.0;
      float minIncrementMinus = maxOutputVoltageMinus / 4095.0;
      
      // Get the measurement voltage from the user.
      while (isRunning) {
        input = waitForInput("Enter voltage (V) or 'back' to go back:", true);
        if (!isRunning) return;
        if (input.equalsIgnoreCase("back")) break;
        float voltageInput = input.toFloat();
        if (!isValidVoltage(input)) {
          Serial.println(F("Invalid voltage! Enter a number (e.g., 150.5 or -50)"));
          continue;
        }
        // For positive voltages, ensure it does not exceed the maximum allowed.
        if (voltageInput > 0 && voltageInput > maxOutputVoltagePlus) {
          Serial.println(F("Voltage exceeds HV+ maximum. Capping to max."));
          voltageInput = maxOutputVoltagePlus;
        }
        // Check if the voltage is above the smallest measurable increment.
        if (voltageInput > 0 && voltageInput < minIncrementPlus) {
          Serial.print(F("Voltage below resolution for HV+ (min ~"));
          Serial.print(minIncrementPlus, 2);
          Serial.println(F(" V). Enter a larger value."));
          continue;
        }
        // For negative voltages, similar checks are applied.
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
        user_Date = getCurrentDate();
        currentTime = getCurrentTime();
        Serial.print("Current date and time is: ");
        Serial.print(user_Date);
        Serial.print("  ");
        Serial.println(currentTime);
        
        // Prompt the user to either sync the RTC or proceed with the current time.
        while (true) {
          Serial.println(F("Type 'sync' to set RTC or 'go' to proceed with the measurement."));
          String choice = waitForInput("Enter choice:", true);
          if (!isRunning) return;
          if (choice.equalsIgnoreCase("sync")) {
            setRTCFromSerial(); // Adjust the RTC.
            Serial.println(F("Time updated. Now proceeding..."));
            // Update date and time from the RTC.
            user_Date = getCurrentDate();
            currentTime = getCurrentTime();
            break;
          }
          else if (choice.equalsIgnoreCase("go")) {
            break;
          }
          else {
            Serial.println(F("Invalid choice. Type 'sync' or 'go'."));
            continue;
          }
        }

        // Get the current time from the RTC for record keeping.
        currentTime = getCurrentTime();
        Serial.print("Current time is: ");
        Serial.println(currentTime);
          
        // Save the original measurement start time if not already set.
        if (original_time.length() == 0) {
          original_time = currentTime;
        }
        
        // Compute the scheduled end date and time by adding the measurement duration.
        String scheduledEndDate, scheduledEndTime;
        addMinutesToDateTime(user_Date, currentTime, (float)tm / 60.0, scheduledEndDate, scheduledEndTime);

        // Determine the polarity based on the voltage.
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
          
        // Start the measurement process after a short delay.
        Serial.println(F("Starting measurement process..."));
        delay(1000);
        int measuringTime = tm;
        original_time = currentTime;
        bool completed = takeMeasurement(Vm);

        // If measurement was paused, adjust scheduled end time.
        String measurement_end_date = scheduledEndDate;
        String measurement_end_time = scheduledEndTime;
        if (!completed) {
          // If paused mid-measurement, resume measurement.
          if (!isRunning) {
            return;
          }
          String resumedDate, resumedTime;
          addMinutesToDateTime(user_Date, currentTime, (float)tm / 60.0, resumedDate, resumedTime);

          completed = takeMeasurement(Vm);
          final_date = resumedDate;
          final_time = resumedTime;
          if (!completed) return;
        } else {
          final_date = measurement_end_date;
          final_time = measurement_end_time;
        }

        if (!completed || !isRunning) return;
        Serial.println(F("Measurement complete."));
        // Print the measurement data to the serial monitor.
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
          
        // Ask the user if they want to continue measuring.
        if (!askToContinue()) {
          isRunning = false;
          isPaused = false;
          Serial.println(F("System discharged. Type 'start' to begin again."));
          return;
        } else {
          Serial.println(F("Returning to main menu..."));
          return;
        }
      }
    }
  }
}

// ===== Structure to Hold Multiple Measurement Sets =====
struct MeasurementSet {
  int nMeas;       // Number of different measurement parameters in this set.
  int reps;        // Number of repetitions for this set.
  float* voltages; // Dynamically allocated array holding voltage values for each measurement.
  long* times;     // Dynamically allocated array holding measurement durations for each parameter.

  // Arrays for storing the start and end dates/times for each measurement.
  String* measurementStartTime;
  String* measurementEndTime;
  String* measurementStartDate;
  String* measurementEndDate;
};

// ===== Multiple Measurements Mode =====
// This function handles the case where the user wants to perform multiple measurement sets.
void handleMultipleMeasurements() {
  String input;
  String measurementStartTime;
  String measurementEndTime;

  // Fetch the baseline time from the RTC.
  String baselineTime = getCurrentTime();
  Serial.print("Current Time is: ");
  Serial.println(baselineTime);

  float voltage = 0.0;
  long timeSec = 0;
  int reps = 0;

  Serial.println(F("Multiple Measurements selected."));
  while (isRunning) {
    int numSets = 0;

    // ──────────────────────────────────────────────────────────
    // 1) Ask for the number of sets
    // ──────────────────────────────────────────────────────────
RE_ASK_NUM_SETS:
    while (true) {
      input = waitForInput("Enter number of sets (up to 200):", true);
      if (!isRunning) return;

      // If user presses "back," exit this function (go up to measurement-type menu).
      if (input.equalsIgnoreCase("back")) {
        return;  
      }

      numSets = input.toInt();
      if (numSets > 0 && numSets <= 200) {
        break;
      }
      Serial.println(F("Invalid input. Enter a positive number up to 200."));
    }

    // Dynamically allocate the array of MeasurementSet objects
    MeasurementSet* sets = new MeasurementSet[numSets];
    float minIncrementPlus  = maxOutputVoltagePlus  / 4095.0;
    float minIncrementMinus = maxOutputVoltageMinus / 4095.0;

    // ──────────────────────────────────────────────────────────
    // 2) Loop over each set
    // ──────────────────────────────────────────────────────────
    for (int s = 0; s < numSets && isRunning; ) {
      Serial.print(F("Defining Set #"));
      Serial.println(s + 1);

      int nMeas = 0;

      // ──────────────────────────────────────────────────────────
      // 2a) Ask for the number of parameters in this set
      // ──────────────────────────────────────────────────────────
RE_ASK_NMEAS:
      while (true) {
        input = waitForInput("Enter number of parameters in this measurement set:", true);
        if (!isRunning) return;

        // If "back," go back to re-ask “Enter number of sets”
        if (input.equalsIgnoreCase("back")) {
          goto RE_ASK_NUM_SETS;
        }

        nMeas = input.toInt();
        if (nMeas > 0) {
          break;
        }
        Serial.println(F("Invalid input. Enter a positive number."));
      }

      // Allocate arrays for voltages and times
      sets[s].nMeas    = nMeas;
      sets[s].voltages = new float[nMeas];
      sets[s].times    = new long[nMeas];

      // ──────────────────────────────────────────────────────────
      // 2b) For each measurement: Enter voltage, then time
      // ──────────────────────────────────────────────────────────
      for (int i = 0; i < nMeas && isRunning; ) {
        if (!isRunning) return;

        Serial.print(F("  Configuring measurement "));
        Serial.print(i + 1);
        Serial.print(F(" of "));
        Serial.println(sets[s].nMeas);

        // ----- Enter voltage -----
RE_ASK_VOLTAGE:
        while (true) {
          input = waitForInput("Enter voltage (V):", true);
          if (!isRunning) return;

          // If "back":
          if (input.equalsIgnoreCase("back")) {
            // If i>0, decrement i to re‐configure the previous measurement
            if (i > 0) {
              i--;
              break;  // break so the for loop sees i-- and re-asks that measurement
            } else {
              // i==0 => go back up to “Enter number of parameters”
              goto RE_ASK_NMEAS;
            }
          }

          // Otherwise, validate the voltage
          Serial.print(F("You entered: "));
          Serial.print(input);
          Serial.println(F(" V"));

          if (!isValidVoltage(input)) {
            Serial.println(F("Invalid voltage. Enter a number (e.g., 150.5, -50, or 0)."));
            continue;
          }
          voltage = input.toFloat();

          // Check constraints
          if (voltage > 0) {
            if (voltage > maxOutputVoltagePlus) {
              Serial.println(F("Voltage exceeds HV+ maximum. Capping to max."));
              voltage = maxOutputVoltagePlus;
            } else if (voltage < minIncrementPlus) {
              Serial.print(F("Voltage below resolution for HV+ (min ~"));
              Serial.print(minIncrementPlus, 2);
              Serial.println(F(" V). Enter a larger value."));
              continue;
            }
          } else if (voltage < 0) {
            if (fabs(voltage) > maxOutputVoltageMinus) {
              Serial.println(F("Voltage exceeds HV- maximum. Capping to max."));
              voltage = -maxOutputVoltageMinus;
            } else if (fabs(voltage) < minIncrementMinus) {
              Serial.print(F("Voltage below resolution for HV- (min ~"));
              Serial.print(minIncrementMinus, 2);
              Serial.println(F(" V). Enter a larger value."));
              continue;
            }
          } else if (voltage == 0) {
            // zero => set whichever output was last active to 0
            if (globalLastPolarity == "HV+" || globalLastPolarity == "") {
              setHVPlusOutput(0);
            } else {
              setHVMinusOutput(0);
            }
          }
          // Valid voltage => break
          break;
        }

        // If we just broke out of voltage due to i--, skip time and let the for loop re-ask
        if (input.equalsIgnoreCase("back") && i >= 0) {
          continue;
        }
        sets[s].voltages[i] = voltage;

        // ----- Enter time -----
RE_ASK_TIME:
        while (true) {
          input = waitForInput("Enter measurement time in seconds (≥10):", true);
          if (!isRunning) return;

          // If "back," go up one level => re-enter voltage for the same i
          if (input.equalsIgnoreCase("back")) {
            goto RE_ASK_VOLTAGE;
          }

          Serial.print(F("You entered: "));
          Serial.print(input);
          Serial.println(F(" s"));

          timeSec = input.toInt();
          if (timeSec >= 10) {
            break;
          }
          Serial.println(F("Time must be at least 10 seconds."));
        }

        sets[s].times[i] = timeSec;
        i++;
      }

      // ──────────────────────────────────────────────────────────
      // 2c) Ask for number of repetitions
      // ──────────────────────────────────────────────────────────
      while (true) {
        input = waitForInput("Enter number of repetitions for this set:", true);
        if (!isRunning) return;

        // If "back," go back up one prompt => re-ask "number of parameters"
        if (input.equalsIgnoreCase("back")) {
          goto RE_ASK_NMEAS;
        }

        reps = input.toInt();
        if (reps > 0) {
          break;
        }
        Serial.println(F("Invalid input. Must be a positive integer."));
      }
      sets[s].reps = reps;

      // Allocate arrays for storing start/end date/time
      sets[s].measurementStartTime = new String[nMeas * reps];
      sets[s].measurementEndTime   = new String[nMeas * reps];
      sets[s].measurementStartDate = new String[nMeas * reps];
      sets[s].measurementEndDate   = new String[nMeas * reps];

      // Move on to the next set
      s++;
    }

    // Everything below (review sets, confirm yes/no, sync or go, execution, data logging) is unchanged
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
      if (input.equalsIgnoreCase("yes")) {
        user_Date = getCurrentDate();
        currentTime = getCurrentTime();
        Serial.print("Current date and time is: ");
        Serial.print(user_Date);
        Serial.print("  ");
        Serial.println(currentTime);
        break; 
      }
      if (input.equalsIgnoreCase("no")) return;
      Serial.println(F("Invalid input. Type yes or no."));
    }

    while (true) {
      Serial.println(F("Type 'sync' to set RTC or 'go' to proceed with multiple measurements."));
      String choice = waitForInput("Enter choice:", true);
      if (!isRunning) return;
      if (choice.equalsIgnoreCase("sync")) {
        setRTCFromSerial();
        Serial.println(F("Time updated. Now proceeding..."));
        user_Date    = getCurrentDate();
        baselineTime = getCurrentTime();
        break;
      }
      else if (choice.equalsIgnoreCase("go")) {
        break;
      }
      else {
        Serial.println(F("Invalid choice. Type 'sync' or 'go'."));
      }
    }

    baselineTime = getCurrentTime();
    Serial.print(F("Current Time is: "));
    Serial.println(baselineTime);

    // Execute each measurement set (unchanged)
    for (int s = 0; s < numSets && isRunning; s++) {
      Serial.print(F("\n*** Executing Set #"));
      Serial.println(s + 1);
      Serial.print(F(" with "));
      Serial.print(sets[s].nMeas);
      Serial.print(F(" measurements, repeated "));
      Serial.print(sets[s].reps);
      Serial.println(F(" times ***"));
      int j = 0;  
      String lastPolarity = "OFF";
      String runningTime = baselineTime;

      // Loop through reps
      for (int rep = 0; rep < sets[s].reps && isRunning; rep++) {
        Serial.print(F("  Starting repetition #"));
        Serial.println(rep + 1);

        // Loop through the measurements
        for (int i = 0; i < sets[s].nMeas && isRunning; i++, j++) {
          Vm = sets[s].voltages[i];
          voltage = Vm;
          timeSec = sets[s].times[i];

          float nextV;
          if (i < sets[s].nMeas - 1) {
            nextV = sets[s].voltages[i + 1];
          } else {
            if (rep < sets[s].reps - 1) {
              nextV = sets[s].voltages[0];
            } else if (s < numSets - 1) {
              nextV = sets[s+1].voltages[0];
            } else {
              nextV = voltage;
            }
          }

          String previousPolarity = (voltage != 0) 
              ? ((voltage > 0) ? "HV+" : "HV-") 
              : globalLastPolarity;

          // If next voltage is opposite polarity, do ramp_up
          if (nextV != voltage) {
            bool opposite = ((voltage >= 0 && nextV < 0) || (voltage < 0 && nextV > 0));
            if (opposite) {
              ramp_up(opposite, nextV);
            }
          }

          // Set HV+ or HV-
          String currentPolarity = (voltage > 0) ? "HV+" : 
                                  ((voltage < 0) ? "HV-" : globalLastPolarity);
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

          // Compute new end date/time
          String newEndDate, newEndTime;
          addMinutesToDateTime(user_Date, runningTime, (float)timeSec / 60.0,
                               newEndDate, newEndTime);

          // Record start/end
          sets[s].measurementStartDate[j] = user_Date;
          sets[s].measurementStartTime[j] = runningTime;
          sets[s].measurementEndDate[j]   = newEndDate;
          sets[s].measurementEndTime[j]   = newEndTime;

          Serial.println(F("----------------------------------------------------------------------------------------"));
          lastPolarity = currentPolarity;

          tm = timeSec;
          int measuring_duration = timeSec;
          bool completed = takeMeasurement(voltage);
          if (!isRunning) return;

          if (!completed) {
            // If paused
            String resumedEndDate, resumedEndTime;
            addMinutesToDateTime(user_Date, currentTime, (float)tm / 60.0,
                                 resumedEndDate, resumedEndTime);

            int measuredBeforePause = measuring_duration - tm;
            Serial.print(F("Measurement resumed: "));
            Serial.print(measuredBeforePause);
            Serial.println(F(" s completed before pause."));

            sets[s].measurementEndDate[j] = resumedEndDate;
            sets[s].measurementEndTime[j] = resumedEndTime;

            completed = takeMeasurement(voltage);
            if (!completed) return;

            baselineTime       = resumedEndTime;
            runningTime        = resumedEndTime;
            current_start_date = resumedEndDate;
            final_date         = resumedEndDate;
            final_time         = resumedEndTime;
          }
          else {
            baselineTime = newEndTime;
            runningTime  = newEndTime;
            user_Date    = newEndDate;
            final_date   = newEndDate;
            final_time   = newEndTime;
          }
          Serial.println(F("    Measurement complete."));
        }
      }
      Serial.println("****************************************************************************");
    }

    // After all sets, discharge
    Serial.println("\nAll sets completed. Discharging...");
    int globalMeasurementIndex = 1;

    // Print measurement data
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
      isPaused  = false;
      Serial.println(F("System discharged. Type 'start' to begin a new measurement."));
      return;
    }
    else {
      return;
    }
  }
}





// ===== Helper Functions to Set HV+ and HV- Outputs =====

// Function to set the HV+ output voltage. It scales the requested voltage to the 0-3.251 V range for PWM.
void setHVPlusOutput(float voltage) {
  scaledVoltage = (voltage / maxOutputVoltagePlus) * 3.251;
  if (scaledVoltage > 3.251) scaledVoltage = 3.251;
  if (scaledVoltage < 0.0) scaledVoltage = 0.0;
  int pwm = (int)((scaledVoltage / 3.251) * 4095.0);
  if (pwm > 4095) pwm = 4095;
  if (voltage == 0) pwm = 0;
  analogWrite(H_Plus_Pin_PWM, pwm);
  digitalWrite(H_PIN, HIGH);      // Set control pin to indicate HV+ is active.
  digitalWrite(HVPlus, HIGH);       // Turn on HV+ indicator LED.
  digitalWrite(HVMinus, LOW);       // Turn off HV- indicator LED.
}

// Function to set the HV- output voltage. It scales the voltage to the correct PWM value for the negative side.
void setHVMinusOutput(float voltage) {
  scaledVoltage = (voltage / maxOutputVoltageMinus) * 3.251;
  if (scaledVoltage < -3.251) scaledVoltage = -3.251;
  if (scaledVoltage > 0.0) scaledVoltage = 0.0;
  int pwm = (int)((fabs(scaledVoltage) / 3.251) * 4095.0);
  if (pwm > 4095) pwm = 4095;
  if (voltage == 0) pwm = 0;
  analogWrite(H_Minus_Pin_PWM, pwm);
  digitalWrite(H_PIN, LOW);       // Set control pin to indicate HV- is active.
  digitalWrite(HVPlus, LOW);        // Turn off HV+ indicator LED.
  digitalWrite(HVMinus, HIGH);      // Turn on HV- indicator LED.
}

// ===== Ramp-up Function =====
// This function pre-adjusts the voltage when a change in polarity is about to occur.
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

// ===== Validation Functions =====

// Validate that a date string is in the format DD-MM-YYYY and represents a valid date.
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

// Return the number of days in a given month for a specified year.
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

// Validate that a time string is in the format HH:MM:SS and represents a valid time.
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

// Validate that the voltage input string represents a valid number.
bool isValidVoltage(String input) {
  bool hasDecimal = false;
  if (input.length() == 0) return false; // If the input is empty, reject it
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

bool isValidPositiveVoltage(String input) {
    // Ensure input is not empty
    if (input.length() == 0) return false;
    // Check if input contains only digits and at most one decimal point
    bool hasDecimal = false;
    for (int i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        // Reject negative values
        if (c == '-') return false;
        // Allow one decimal point
        if (c == '.' && !hasDecimal) {
            hasDecimal = true;
            continue;
        }
        // If any character is not a digit (except one decimal), reject it
        if (!isDigit(c)) return false;
    }
    // Check if the input is purely numeric
    for (int i = 0; i < input.length(); i++) {
        if (!isDigit(input.charAt(i)) && input.charAt(i) != '.') {
            Serial.println(F("Invalid input! Only numeric values are allowed."));
            return false;
        }
    }
    // Convert to float and check range
    float value = input.toFloat();
    if (value < 1 ) {
      Serial.println(F("Invalid input. Only positive number is allowed."));
      return false;
    }

    return true;
}

// Validate that the measurement time input consists solely of digits.
bool isValidMeasurementTime(String input) {
  for (int i = 0; i < input.length(); i++) {
    if (!isDigit(input.charAt(i))) return false;
  }
  return true;
}

// ===== addMinutes Function =====
// Adds a given number of minutes (can be fractional) to a time string and returns the new time.
// (This function is kept for backward compatibility.)
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

// ===== askToContinue Function =====
// Asks the user if they wish to continue with additional measurements.
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

// ===== addMinutesToDateTime Function =====
// Adds minutes (which can be fractional) to a given date and time, and calculates the resulting new date and time.
// This function handles overflow into new hours and even new days/months/years.
void addMinutesToDateTime(const String &dateStr, const String &timeStr, float minutesToAdd, 
                          String &resultDate, String &resultTime) {
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

  float fractional = minutesToAdd - (int)minutesToAdd;
  int extraSeconds = (int)(fractional * 60.0f);
  second += extraSeconds;
  if (second >= 60) {
    second -= 60;
    newMinute += 1;
    if (newMinute >= 60) {
      newMinute -= 60;
      newHour += 1;
      if (newHour >= 24) {
        newHour -= 24;
        extraDays += 1;
      }
    }
  }

  // Handle extra days by checking month lengths.
  while (extraDays > 0) {
    int daysThisMonth = daysInMonth(month, year);
    if (day + extraDays > daysThisMonth) {
      extraDays -= (daysThisMonth - day + 1);
      day = 1;
      month++;
      if (month > 12) {
        month = 1;
        year++;
      }
    } else {
      day += extraDays;
      extraDays = 0;
    }
  }

  // If hours overflow beyond 24, adjust the day accordingly.
  if (newHour >= 24) {
    int dayIncrement = newHour / 24;
    newHour = newHour % 24;
    day += dayIncrement;
    int daysThisMonth = daysInMonth(month, year);
    if (day > daysThisMonth) {
      day -= daysThisMonth;
      month++;
      if (month > 12) {
        month = 1;
        year++;
      }
    }
  }

  // Format the resulting date as DD-MM-YYYY.
  resultDate = (day < 10 ? "0" : "") + String(day) + "-" +
               (month < 10 ? "0" : "") + String(month) + "-" +
               String(year);
  // Format the resulting time as HH:MM:SS.
  resultTime = (newHour < 10 ? "0" : "") + String(newHour) + ":" +
               (newMinute < 10 ? "0" : "") + String(newMinute) + ":" +
               (second < 10 ? "0" : "") + String(second);
}

// ===== takeMeasurement Function =====
// This function carries out the measurement for a given voltage and duration.
// It also checks for pauses and adjusts the timing accordingly.
bool takeMeasurement(float voltage) {
  Serial.print(F("Measuring "));
  Serial.print(tm);
  Serial.print(F(" s at "));
  Serial.print(voltage);
  Serial.println(F(" V (requested)."));
  
  // Set the appropriate voltage output based on the sign of the voltage.
  if (voltage > 0) {
    setHVPlusOutput(voltage);
  } else if (voltage < 0) {
    setHVMinusOutput(voltage);
  } 
  Serial.print(F("Scaled to "));
  Serial.print(scaledVoltage, 2);
  Serial.println(F(" V ."));
  
  // Calculate the measurement duration in milliseconds.
  unsigned long durationInMillis = tm * 1000UL;
  unsigned long startTime = millis();
  bool hasPrinted = false;
  
  // Loop until the measurement duration has elapsed.
  while (millis() - startTime < durationInMillis) {
    if (!isRunning) {
      discharge();
      return false;
    }
    // Handle pausing: if paused, wait until resumed.
    if (isPaused) {
      unsigned long pauseStart = millis();
      while (isPaused && isRunning) {
        processSerialCommands();
        delay(100);
      }
      unsigned long pauseDuration = millis() - pauseStart;
      startTime += pauseDuration; // Adjust the start time to account for the pause.
      if (!isRunning) return false;
      unsigned long elapsed = millis() - startTime;
      if (elapsed < durationInMillis) {
        tm = (durationInMillis - elapsed) / 1000;
      } else {
        tm = 0;
      }
      return false; // Indicate that the measurement did not complete in one continuous run.
    }
    if (!hasPrinted) {
      Serial.println(voltage);
      hasPrinted = true;
    }
    processSerialCommands();
    feedbackloop();
    delay(100);
  }
  tm = 0;  
  return true; // Measurement completed successfully.
}

// ===== feedbackloop Function =====
// This function is called in the main loop to periodically check for voltage drift.
void feedbackloop() {
  if (isRunning && !isPaused) { // Ensure measurement is active
    static unsigned long lastFeedbackCheck = 0;
    unsigned long now = millis();
    if (now - lastFeedbackCheck >= FEEDBACK_INTERVAL) { // Check every hour (3600000ms)
      lastFeedbackCheck = now;
      checkFeedback(); // Check and adjust the voltage if necessary.
    }
  }
}

// ===== checkFeedback Function =====
// Compares the desired voltage to the actual measured voltage and adjusts if the difference exceeds the threshold.
void checkFeedback() {

  if (Vm == 0.0f) return; // No voltage set, so no adjustment needed
  if (!isRunning || isPaused) return;  // Skip feedback if paused/stopped
  float measuredHV = (Vm > 0.0f) ? readHVplus() : readHVminus();
  if (Vm == 0.0f && measuredHV == 0.0f) return;  // No active voltage to check
  float diff = fabs(fabs(Vm) - fabs(measuredHV));
  float fraction = diff / fabs(Vm); // Calculate error percentage

  if (fraction > feedbackThreshold) { // Check if drift exceeds threshold
    Serial.print(F("[Feedback] Voltage drift detected. Measured="));
    Serial.print(measuredHV, 2);
    Serial.print(F(" V, Setpoint="));
    Serial.print(Vm, 2);
    Serial.print(F(" V, Diff="));
    Serial.print(diff, 2);
    Serial.println(F(" => Re-adjusting..."));

    // Re-adjust the voltage based on polarity.
    if (Vm > 0.0f)
      setHVPlusOutput(Vm);
    else
      setHVMinusOutput(Vm);
  }
}

// ===== readHVplus Function =====
// Reads the voltage on the HV+ side using an analog input (assumed to be on pin A0).
float readHVplus() {
  int raw = analogRead(A0); // Read ADC value from HV+ sensor
  float adcVolts = (float)raw / 4095.0f * 3.251f; // Convert ADC value to voltage
  float realVoltage =  adcVolts * hvPlusScaleFactor; // Scale to real-world voltage
  // --- Add this debug line ---
  Serial.print("[DEBUG] HV+ ADC raw = ");
  Serial.print(raw);
  Serial.print(", HV+ ADC volts = ");
  Serial.print(adcVolts, 3);
  Serial.print(", HV+ real volts = ");
  Serial.println(realVoltage, 3);

  return realVoltage;
}

// ===== readHVminus Function =====
// Reads the voltage on the HV- side using an analog input (assumed to be on pin A0).
float readHVminus() {
  int raw = analogRead(A0);  // Read ADC value from HV- sensor
  float adcVolts = (float)raw / 4095.0f * 3.251f; // Convert ADC value to voltage
  float realVoltage = adcVolts * hvMinusScaleFactor; // Scale to real-world voltage
  // --- Add this debug line ---
  Serial.print("[DEBUG] HV- ADC raw = ");
  Serial.print(raw);
  Serial.print(", HV- ADC volts = ");
  Serial.print(adcVolts, 3);
  Serial.print(", HV- real volts = ");
  Serial.println(realVoltage, 3);

  return realVoltage;
}
