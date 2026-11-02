#include "thingProperties.h"

#define BTN_ROW_1 2
#define BTN_ROW_2 3
#define BTN_ROW_3 7
#define BTN_COL_1 5
#define BTN_COL_2 4
#define BTN_COL_3 6

int rows[] = { BTN_ROW_1, BTN_ROW_2, BTN_ROW_3 };
int cols[] = { BTN_COL_1, BTN_COL_2, BTN_COL_3 };

#define colSize 3
#define rowSize 3

// Debouncing variables
bool buttonStates[colSize][rowSize] = {};  // Stable debounced state
bool lastReading[colSize][rowSize] = {};   // Last raw reading
unsigned long lastDebounceTime[colSize][rowSize] = {};
unsigned long debounceDelay = 50;

// Combination lock variables
int correctCombination[] = { 6, 9, 2, 1 };  // Change this to your desired combination
int currentInput[4] = { 0, 0, 0, 0 };
int inputIndex = 0;
bool isUnlocked = false;
unsigned long unlockTime = 0;
unsigned long lockTimeout = 10000;  // 10 seconds

void setup() {
  Serial.begin(9600);
  delay(1500);

  for (int i = 0; i < colSize; i++) {
    pinMode(cols[i], OUTPUT);
    digitalWrite(cols[i], HIGH);
  }
  for (int i = 0; i < rowSize; i++) {
    pinMode(rows[i], INPUT_PULLUP);
  }

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  button_matrix_state = "LOCKED";

  incorrectAttempts = 0;
  unlockCount = 0;
  isLocked = true;

  ArduinoCloud.update();  //pushar värden en gång
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
}

void loop() {
  ArduinoCloud.update();

  // Check if device should lock after 10 seconds
  if (isUnlocked && (millis() - unlockTime > lockTimeout)) {
    isUnlocked = false;
    button_matrix_state = "LOCKED";
    isLocked = true;
    inputIndex = 0;
    Serial.println("Device locked after timeout");
    ArduinoCloud.update();
  }

  // Scan button matrix with debouncing
  for (int i = 0; i < colSize; i++) {  // Loop through each column

    // ACTIVATE this column by setting it LOW (connecting it to ground)
    digitalWrite(cols[i], LOW);
    delay(1);  // Wait 1ms for the electrical signal to stabilize

    // Now check ALL rows to see if any button in this column is pressed
    for (int j = 0; j < rowSize; j++) {

      // READ the current state of this row
      // If button is pressed: row connects to the LOW column → reads LOW
      // If button is NOT pressed: row stays pulled HIGH → reads HIGH
      bool reading = (digitalRead(rows[j]) == LOW);

      // === DEBOUNCE LOGIC START ===
      // Physical buttons "bounce" - they flicker between pressed/unpressed for ~10-50ms
      // We need to wait until the reading is STABLE before accepting it

      // If the reading CHANGED from last time we checked...
      if (reading != lastReading[i][j]) {
        // ...restart the debounce timer (we need to wait for stability)
        lastDebounceTime[i][j] = millis();  // Record current time
        lastReading[i][j] = reading;        // Remember this new raw reading
      }

      // Check if the reading has been STABLE for at least 50ms (debounceDelay)
      if ((millis() - lastDebounceTime[i][j]) > debounceDelay) {

        // The reading is stable! Now check if the STABLE state is different
        // from our last confirmed button state
        if (reading != buttonStates[i][j]) {

          // The button state has ACTUALLY changed (not just bouncing)
          buttonStates[i][j] = reading;  // Update the confirmed state

          // Only trigger action when button goes from NOT pressed → PRESSED
          // (not when released, and not while held down)
          if (buttonStates[i][j] == true) {

            // Convert from array index (0,1) to human-readable coordinate (1,2,3,4)
            int col = i + 1;
            int row = j + 1;

            // Log to Serial Monitor for debugging
            Serial.print("Col " + String(col) + " Row " + String(row) + " PRESSED!");
            Serial.println();

            // Call the function that handles what happens when button is pressed
            handleButtonRead(col, row);
          }
        }
      }
      // === DEBOUNCE LOGIC END ===

    }  // End of row loop

    // DEACTIVATE this column by setting it back to HIGH
    // This ensures only ONE column is active at a time
    digitalWrite(cols[i], HIGH);

  }  // End of column loop - move to next column
}

void handleButtonRead(int col, int row) {
  String coordString = String(col) + "," + String(row);
  int buttonNumber = 0;

  if (coordString == "1,1") {
    buttonNumber = 1;
    Serial.println("Button 1");
  } else if (coordString == "2,1") {
    buttonNumber = 2;
    Serial.println("Button 2");
  } else if (coordString == "3,1") {
    buttonNumber = 3;
    Serial.println("Button 3");
  } else if (coordString == "1,2") {
    buttonNumber = 4;
    Serial.println("Button 4");
  } else if (coordString == "2,2") {
    buttonNumber = 5;
    Serial.println("Button 5");
  } else if (coordString == "3,2") {
    buttonNumber = 6;
    Serial.println("Button 6");
  } else if (coordString == "1,3") {
    buttonNumber = 7;
    Serial.println("Button 7");
  } else if (coordString == "2,3") {
    buttonNumber = 8;
    Serial.println("Button 8");
  } else if (coordString == "3,3") {
    buttonNumber = 9;
    Serial.println("Button 9");
  }
  // Handle combination lock logic
  if (!isUnlocked) {
    currentInput[inputIndex] = buttonNumber;
    inputIndex++;

    Serial.print("Input progress: ");
    Serial.print(inputIndex);
    Serial.println("/4");

    // Check if 4 buttons have been pressed
    if (inputIndex >= 4) {
      bool correct = true;
      for (int i = 0; i < 4; i++) {
        if (currentInput[i] != correctCombination[i]) {
          correct = false;
          break;
        }
      }

      if (correct) {
        isUnlocked = true;
        unlockTime = millis();
        button_matrix_state = "UNLOCKED";
        isLocked = false;
        unlockCount++;
        Serial.println("✓ Device UNLOCKED!");
        ArduinoCloud.update();
      } else {
        button_matrix_state = "WRONG CODE";
        incorrectAttempts++;
        isLocked = true;
        Serial.println("✗ Wrong combination!");
        ArduinoCloud.update();
        //Updating state in cloud to be handled on other systems.
        delay(2000);
        button_matrix_state = "LOCKED";
        ArduinoCloud.update();
      }

      // Reset input
      inputIndex = 0;
      for (int i = 0; i < 4; i++) {
        currentInput[i] = 0;
      }
    }
  } else {
    button_matrix_state = "Already unlocked - Button " + String(buttonNumber);
    Serial.println("Device already unlocked");
  }
}

void onButtonMatrixStateChange() {
  // Add your code here to act upon ButtonMatrixState change
}
