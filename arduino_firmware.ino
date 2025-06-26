
/* 
Arduino firmware for light pipetting guide v2 (EXPANDED for Protocols & Experiments) 
Author: Rodrigo Bonfim Aragao
Date: 5/4/2025
*/

#include <FastLED.h>
#include <SPI.h>

// Pins
#define LED_PIN 6
#define FAN_PIN 7
#define THERMISTOR_PIN A0

#define SERIES_RESISTOR 10000  // 10k resistor
#define NOMINAL_RESISTANCE 10000  // resistance at 25 °C
#define NOMINAL_TEMPERATURE 25    // °C
#define B_COEFFICIENT 3950        // B value from datasheet
#define ADC_MAX 1023
#define TEMP_THRESHOLD 25.0  // Celsius

// Boolean to track incoming serial data
boolean newData = false;
const byte numChars = 64;
char receivedCharArray[numChars];
String fullCommand = "";  // Will store the clean incoming serial command

unsigned long lastTempReading = 0;
const unsigned long tempReadInterval = 60000; // Read temp every minute

/* Components of command received over serial port */
char rowLetter[numChars] = {0};
int rowNumber;
int columnNumber = 0;
char illuminationCommand[numChars] = {0};
char colorCode[numChars] = {0};
int pixelNumber;

/* Definition of plate layout */
const int numColumns = 12;
const int numRows = 8;
const int numWells = numRows * numColumns;
CRGB leds[numWells];

// Protocol definition
struct Protocol {
  String name;
  CRGB color;
  uint8_t intensity; // 0-255
  unsigned long activePeriod;   // milliseconds
  unsigned long silentPeriod;   // milliseconds
  unsigned long pulseOn;        // milliseconds
  unsigned long pulseOff;       // milliseconds
  unsigned long totalDuration;  // milliseconds
  
  // Runtime variables
  unsigned long startTime;      // when protocol started
  unsigned long lastUpdate;     // last time protocol was updated
  bool isActive;                // currently in active period?
  bool isOn;                    // light currently on during pulse?
  bool isRunning;               // protocol is currently running
};

// Experiment definition
struct Experiment {
  String name;
  //DateTime startTime;
  bool isRunning;
  String metadata;
};

// Max protocols allowed
const int maxProtocols = 10;
Protocol protocols[maxProtocols];
int protocolCount = 0;

// Current experiment
Experiment currentExperiment;

// Well assignment: which protocol is assigned to each well
int wellToProtocol[numWells]; // -1 = no protocol assigned

// Function declarations
void clearDisplay();
void recvWithStartEndMarkers();
// void parseData();
void displayParsedCommand();
int convertRowLetterToNumber(char rowLetterReceived[]);
void parseIlluminationCommand(String illuminationCommand);
void clearRow(int row);
void clearColumn(int column);
void updateDisplay();
CRGB getColorFromCode(const char *code);
void createProtocol(String name, const char* color, uint8_t intensity, unsigned long activeP, unsigned long silentP, unsigned long pulseOn, unsigned long pulseOff, unsigned long totalDur);
void assignProtocolToWell(int row, int column, int protocolIndex);
void assignProtocolToRange(int startRow, int startCol, int endRow, int endCol, int protocolIndex);
void updateProtocols();
void startExperiment(String name);
void stopExperiment();
void saveExperimentMetadata();
void loadExperiment(String filename);
void readTemperature();
// void printHelp();
void listProtocols();
void processSerialCommand1(String fullCommand);

void setup() {
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, numWells);
  Serial.begin(115200);
  Serial.println(F("Light System for Optogenetics"));
  clearDisplay();
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);

  // Initialize well assignments
  for (int i = 0; i < numWells; i++) {
    wellToProtocol[i] = -1; // No protocol assigned
  }
  
  currentExperiment.isRunning = false;
}

unsigned long lastProtocolUpdate = 0;
const unsigned long protocolUpdateInterval = 50; // ms
void loop() {
  recvWithStartEndMarkers();

  if (newData) {
    processSerialCommand1(String(receivedCharArray));  // full command parsing
    newData = false;
  }
  
  // Update running protocols
  if (currentExperiment.isRunning) {
  unsigned long now = millis();

  if (now - lastProtocolUpdate >= protocolUpdateInterval) {
    updateProtocols();
    lastProtocolUpdate = now;
  }

  if (now - lastTempReading >= 1800000UL) {  // 30 mins
    readTemperature();  // Simulated
    lastTempReading = now;
  }
}
}

void processSerialCommand1(String fullCommand) {
  fullCommand.trim();  // Remove trailing newline or spaces
  char tempArray[numChars];
  fullCommand.toCharArray(tempArray, numChars);

  char* token = strtok(tempArray, ",");
  if (!token) return;
  String field1 = String(token);

  token = strtok(NULL, ",");
  if (!token) return;
  int field2 = atoi(token);

  token = strtok(NULL, ",");
  if (!token) return;
  String command = String(token);

  if (command == "X" || command == "C" || command == "R" || command == "S" ||
      command == "CR" || command == "CC" || command == "U") {
    // Use only field1 and field2
    rowNumber = convertRowLetterToNumber((char*)field1.c_str());
    columnNumber = field2;
    strcpy(colorCode, "W");  // Default
    parseIlluminationCommand(command);
    return;
  }

  if (command == "HELP") {
    //printHelp();
  }
  else if (command == "PROTOCOL") {
    // Format: <name,intensity,PROTOCOL,activePeriod,silentPeriod,pulseOn,pulseOff,totalDuration,color>
    String name = field1;
    int intensity = field2;

    token = strtok(NULL, ",");
    if (!token) { Serial.println(F("ERROR: Missing activePeriod.")); return; }
    float activeP = atof(token) * 1000;
  
    token = strtok(NULL, ",");
    if (!token) { Serial.println(F("ERROR: Missing silentPeriod.")); return; }
    float silentP = atof(token) * 1000;

    token = strtok(NULL, ",");
    if (!token) { Serial.println(F("ERROR: Missing pulseOn.")); return; }
    float pulseOn = atof(token) * 1000;

    token = strtok(NULL, ",");
    if (!token) { Serial.println(F("ERROR: Missing pulseOff.")); return; }
    float pulseOff = atof(token) * 1000;

    token = strtok(NULL, ",");
    if (!token) { Serial.println(F("ERROR: Missing totalDuration.")); return; }
    float totalDur = atof(token) * 1000;

    token = strtok(NULL, ",");
    if (!token) { Serial.println(F("ERROR: Missing color.")); return; }
    char protocolColor[2];
    strncpy(protocolColor, token, 1);
    protocolColor[1] = '\0';

    createProtocol(name, protocolColor, intensity, activeP, silentP, pulseOn, pulseOff, totalDur);
  }
  else if (command == "ASSIGN") {
    // Format: <row,column,ASSIGN,protocolIndex>
    token = strtok(NULL, ",");
    if (!token) { Serial.println(F("ERROR: Missing protocol index.")); return; }
    int protocolIndex = atoi(token);

    int row = convertRowLetterToNumber((char*)field1.c_str());
    int col = field2;
    assignProtocolToWell(row, col - 1, protocolIndex);
  }
  else if (command == "RANGE") {
    // Format: <startRow,startCol,RANGE,endRow,endCol,protocolIndex>
    token = strtok(NULL, ",");
    if (!token) { Serial.println(F("ERROR: Missing endRow.")); return; }
    int endRow = convertRowLetterToNumber(token);

    token = strtok(NULL, ",");
    if (!token) { Serial.println(F("ERROR: Missing endCol.")); return; }
    int endCol = atoi(token);

    token = strtok(NULL, ",");
    if (!token) { Serial.println(F("ERROR: Missing protocol index.")); return; }
    int protocolIndex = atoi(token);

    int startRow = convertRowLetterToNumber((char*)field1.c_str());
    int startCol = field2;

    assignProtocolToRange(startRow, startCol - 1, endRow, endCol - 1, protocolIndex);
  }
  else if (command == "LIST") {
    listProtocols();
  }
  else if (command == "START") {
    startExperiment(field1);
  }
  else if (command == "STOP") {
    stopExperiment();
  }
  else if (command == "SAVE") {
    saveExperimentMetadata();
  }
  else if (command == "LOAD") {
    loadExperiment(field1);
  }
  else if (command == "TEMP") {
  readTemperature();  // Call your existing temp function
  }
  else {
    Serial.println(F("Unknown command. Type <0,0,HELP> for available commands."));
  }
}

void listProtocols() {
  Serial.println(F("\n--- Defined Protocols ---"));
  for (int i = 0; i < protocolCount; i++) {
    Serial.print(F("Index: "));
    Serial.print(i);
    Serial.print(F(", Name: "));
    Serial.print(protocols[i].name);
    Serial.print(", Intensity: ");
    Serial.print(protocols[i].intensity);
    Serial.print(F(", Color: "));
    
    // Identify color code from CRGB
    if (protocols[i].color == CRGB::Red) Serial.print("R");
    // else if (protocols[i].color == CRGB::Green) Serial.print("G");
    else if (protocols[i].color == CRGB::Blue) Serial.print("B");
    else Serial.print("W");  // fallback for White or unknown
    
    Serial.print(F(", Active: "));
    Serial.print(protocols[i].activePeriod / 1000);
    Serial.print(F("s, Silent: "));
    Serial.print(protocols[i].silentPeriod / 1000);
    Serial.print(F("s, Pulse ON: "));
    Serial.print(protocols[i].pulseOn / 1000);
    Serial.print(F("s, Pulse OFF: "));
    Serial.print(protocols[i].pulseOff / 1000);
    Serial.print(F("s, Duration: "));
    Serial.print(protocols[i].totalDuration / 1000);
    Serial.println(F("s"));
  }
}

void createProtocol(String name, const char* color, uint8_t intensity, unsigned long activeP, unsigned long silentP, unsigned long pulseOn, unsigned long pulseOff, unsigned long totalDur) {
    Serial.print(F("Color code received: "));
    Serial.println(color);
    if (protocolCount < maxProtocols) {
    protocols[protocolCount].name = name;
    protocols[protocolCount].color = getColorFromCode(color);
    protocols[protocolCount].intensity = intensity;
    protocols[protocolCount].activePeriod = activeP;
    protocols[protocolCount].silentPeriod = silentP;
    protocols[protocolCount].pulseOn = pulseOn;
    protocols[protocolCount].pulseOff = pulseOff;
    protocols[protocolCount].totalDuration = totalDur;
    protocols[protocolCount].isRunning = false;
    protocols[protocolCount].isActive = false;
    protocols[protocolCount].isOn = false;
    
    Serial.print(F("Protocol created: "));
    Serial.print(name);
    Serial.print(F(" (Index: "));
    Serial.print(protocolCount);
    Serial.println(F(")"));
    
    protocolCount++;
  } else {
    Serial.println(F("ERROR: Maximum number of protocols reached."));
  }
}

void assignProtocolToWell(int row, int column, int protocolIndex) {
  if (row >= 0 && row < numRows && column >= 0 && column < numColumns) {
    int index = row * numColumns + (column);
    if (index < numWells && protocolIndex < protocolCount && protocolIndex >= 0) {
      wellToProtocol[index] = protocolIndex;
      Serial.print(F("Assigned protocol "));
      Serial.print(protocolIndex);
      Serial.print(F(" ("));
      Serial.print(protocols[protocolIndex].name);
      Serial.print(F(") to well "));
      Serial.print(char('A' + row));
      Serial.println(column+1);
    } else {
      Serial.println(F("ERROR: Invalid protocol index or well position."));
    }
  } else {
    Serial.println(F("ERROR: Well position out of range."));
  }
}

void assignProtocolToRange(int startRow, int startCol, int endRow, int endCol, int protocolIndex) {
    if (protocolIndex < 0 || protocolIndex >= protocolCount) {
        Serial.println(F("ERROR: Invalid protocol index."));
        return;
    }

    // Make sure ranges are valid
    if (startRow > endRow) {
        int temp = startRow;
        startRow = endRow;
        endRow = temp;
    }
    if (startCol > endCol) {
        int temp = startCol;
        startCol = endCol;
        endCol = temp;
    }

    Serial.print(F("Assigning protocol "));
    Serial.print(protocolIndex);
    Serial.print(F(" ("));
    Serial.print(protocols[protocolIndex].name);
    Serial.print(F(") to wells "));
    Serial.print(char('A' + startRow));
    Serial.print(startCol + 1);
    Serial.print(F(" through "));
    Serial.print(char('A' + endRow));
    Serial.println(endCol + 1);

    for (int r = startRow; r <= endRow; r++) {
        for (int c = startCol; c <= endCol; c++) {
            assignProtocolToWell(r, c, protocolIndex);
        }
    }
}


void updateProtocols() {
  if (!currentExperiment.isRunning) return;
  
  unsigned long currentTime = millis();
  // unsigned long experimentElapsed = currentTime - currentExperiment.startTime.unixtime() * 1000;
  
  // Update each well based on its assigned protocol
  for (int i = 0; i < numWells; i++) {
    int protocolIndex = wellToProtocol[i];
    
    // Skip wells with no assigned protocol
    if (protocolIndex < 0 || protocolIndex >= protocolCount) {
      leds[i] = CRGB::Black;
      continue;
    }
    
    Protocol *p = &protocols[protocolIndex];
    
    // Check if protocol should be running
    if (!p->isRunning) {
      // Start protocol if not running
      p->startTime = currentTime;
      p->lastUpdate = currentTime;
      p->isRunning = true;
      p->isActive = true;  // Start in active phase
      p->isOn = true;      // Start with light on
    }
    
    // Check if protocol has exceeded total duration
    if (p->totalDuration > 0 && currentTime - p->startTime > p->totalDuration) {
      leds[i] = CRGB::Black;
      continue;  // Protocol completed
    }
    
    // Calculate where we are in the cycle
    unsigned long elapsed = currentTime - p->lastUpdate;
    
    // Handle active/silent cycling
    if (p->isActive) {
      // Currently in active period
      if (p->activePeriod > 0 && elapsed >= p->activePeriod) {
        // Switch to silent period
        p->isActive = false;
        p->lastUpdate = currentTime;
        leds[i] = CRGB::Black;
      } else {
        // Still in active period, handle pulsing
        if (p->pulseOn > 0 && p->pulseOff > 0) {
          // Calculate pulse phase
          unsigned long pulseElapsed = elapsed % (p->pulseOn + p->pulseOff);
          
          if (pulseElapsed < p->pulseOn) {
            // Light should be on
            if (!p->isOn) {
              p->isOn = true;
            }
          } else {
            // Light should be off
            if (p->isOn) {
              p->isOn = false;
            }
          }
        } else {
          // No pulsing, light stays on during active period
          p->isOn = true;
        }
        
        // Set LED state
        if (p->isOn) {
          CRGB color = p->color;
          // Apply intensity
          // color.nscale8_video(p->intensity); 
          
          /*  Check if this scaling is appropriate for Optogenetics
              Maybe use manual scaling (below) instead: 
          */  
          color.r = (color.r * p->intensity) / 255;
          color.g = (color.g * p->intensity) / 255;
          color.b = (color.b * p->intensity) / 255;
          leds[i] = color;
        } else {
          leds[i] = CRGB::Black;
        }
      }
    } else {
      // Currently in silent period
      if (p->silentPeriod > 0 && elapsed >= p->silentPeriod) {
        // Switch back to active period
        p->isActive = true;
        p->isOn = true;
        p->lastUpdate = currentTime;
        
        // Set color with intensity
        CRGB color = p->color;
        // color.nscale8_video(p->intensity);
        color.r = (color.r * p->intensity) / 255;
        color.g = (color.g * p->intensity) / 255;
        color.b = (color.b * p->intensity) / 255;
        leds[i] = color;
      } else {
        // Still in silent period
        leds[i] = CRGB::Black;
      }
    }
  }
  
  // Update the display
  FastLED.show();
}

void startExperiment(String name) {
  // Stop any running experiment
  if (currentExperiment.isRunning) {
    stopExperiment();
  }
  
  // Reset all protocols
  for (int i = 0; i < protocolCount; i++) {
    protocols[i].isRunning = false;
    protocols[i].isActive = false;
    protocols[i].isOn = false;
  }
  
  // Set up new experiment
  currentExperiment.name = name;
  //currentExperiment.startTime = rtc.now();
  currentExperiment.isRunning = true;
  
  Serial.print(F("Starting experiment: "));
  Serial.println(name);

  // Take initial temperature reading
  readTemperature();
  //lastTempReading = millis();
}

void stopExperiment() {
  if (!currentExperiment.isRunning) {
    Serial.println(F("No experiment is running."));
    return;
  }
  
  Serial.print(F("Stopping experiment: "));
  Serial.println(currentExperiment.name);
  
  currentExperiment.isRunning = false;
  
  // Reset all protocol states
  for (int i = 0; i < protocolCount; i++) {
    protocols[i].isRunning = false;
    protocols[i].isActive = false;
    protocols[i].isOn = false;
  }

  // Turn off all LEDs
  clearDisplay();
  
  // Take final temperature reading
  readTemperature();
}

void saveExperimentMetadata() {
  if (currentExperiment.name.length() == 0) {
    Serial.println(F("No experiment to save."));
    return;
  }
  
  String filename = currentExperiment.name + ".txt";
  //File dataFile = SD.open(filename, FILE_WRITE);
}

void loadExperiment(String filename) {
  if (!filename.endsWith(".txt")) {
    filename += ".txt";
  }
}

void readTemperature() {
  // Simulate temperature (for now)
  //float tempC = random(220, 260) / 10.0;  // Generates values like 22.0 to 26.0
  int adc = analogRead(THERMISTOR_PIN);
  float resistance = SERIES_RESISTOR / (1023.0 / adc - 1);
  float steinhart;
  steinhart = resistance / NOMINAL_RESISTANCE;     // (R/Ro)
  steinhart = log(steinhart);                      // ln(R/Ro)
  steinhart /= B_COEFFICIENT;                      // 1/B * ln(R/Ro)
  steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                     // Invert
  float tempC = steinhart - 273.15 - 9.0;                // Convert to °C
  
  Serial.print("TEMP:");
  Serial.println(tempC, 2);
  
  // Fan control logic
  if (tempC >= TEMP_THRESHOLD) {
    digitalWrite(FAN_PIN, HIGH);
    Serial.println("Fan ON");
  } else {
    digitalWrite(FAN_PIN, LOW);
    Serial.println("Fan OFF");
  }
}

void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte indexListCounter = 0;
    const char startMarker = '<';
    const char endMarker = '>';
    char receivedCharacter;
  
    while (Serial.available() > 0 && newData == false) {
      receivedCharacter = Serial.read();
  
      if (recvInProgress) {
        if (receivedCharacter != endMarker) {
          receivedCharArray[indexListCounter++] = receivedCharacter;
          if (indexListCounter >= numChars) {
            indexListCounter = numChars - 1; // avoid overflow
          }
        } else {
          receivedCharArray[indexListCounter] = '\0'; // null-terminate
          recvInProgress = false;
          indexListCounter = 0;
          newData = true; // complete message received

          fullCommand = String(receivedCharArray);  // Save the complete incoming command here
        }
      } 
      else if (receivedCharacter == startMarker) {
        recvInProgress = true;
      }
    }
  }
  

void displayParsedCommand() {
  Serial.print(F("Parsed: Row="));
  Serial.print(rowLetter);
  Serial.print(F(", Column="));
  Serial.print(columnNumber);
  Serial.print(F(", Command="));
  Serial.print(illuminationCommand);
//   Serial.print(F(", Barcode="));
//   Serial.print(plateBarcode);
  Serial.print(F(", Color="));
  Serial.println(colorCode);
}

void clearDisplay() {
  FastLED.clear();
  FastLED.show();
  Serial.println(F("Display cleared."));
}

void illuminateRow(int row) {
  CRGB color = getColorFromCode(colorCode);
  //row =- 1; // maybe delete
  if (row < 0 || row >= numRows) return;
  Serial.print(F("Row: ")); Serial.println(row);
  for (int column = 0; column < numColumns; column++) {
    pixelNumber = numColumns * row + column;
    if (pixelNumber < numWells) leds[pixelNumber] = color;
  }
  FastLED.show();
}

void illuminateColumn(int column) {
  CRGB color = getColorFromCode(colorCode);
  column -= 1;
  if (column < 0 || column >= numColumns) return;
  Serial.print(F("Column: ")); Serial.println(column + 1);
  for (int row = 0; row < numRows; row++) {
    pixelNumber = row * numColumns + column;
    if (pixelNumber < numWells) leds[pixelNumber] = color;
  }
  FastLED.show();
}

void illuminateWell(int c, int r) {
  CRGB color = getColorFromCode(colorCode);
  pixelNumber = r * numColumns + (c - 1);
  if (pixelNumber < 0 || pixelNumber >= numWells) {
    Serial.println(F("ERROR: Pixel out of range!"));
    return;
  }
  Serial.print(F("Pixel #: ")); Serial.println(pixelNumber);
  leds[pixelNumber] = color;
  FastLED.show();
}

void parseIlluminationCommand(String illuminationCommand) {
  if (illuminationCommand == "X") {
    clearDisplay();
  } else if (illuminationCommand == "C") {
    illuminateColumn(columnNumber);
  } else if (illuminationCommand == "R") {
    illuminateRow(rowNumber);
  } else if (illuminationCommand == "S") {
    illuminateWell(columnNumber, rowNumber);
  } else if (illuminationCommand == "CR") {
    clearRow(rowNumber);
  } else if (illuminationCommand == "CC") {
    clearColumn(columnNumber);
  } else if (illuminationCommand == "U") {
    updateDisplay();
  } else {
    Serial.println(F("ERROR: Appropriate value not received."));
  }
}

int convertRowLetterToNumber(char rowLetterReceived[]) {
  return rowLetterReceived[0] - 'A';
}

void clearRow(int row) {
  if (row < 0 || row >= numRows) return;
  Serial.print(F("Clearing row: ")); Serial.println(row);
  for (int column = 0; column < numColumns; column++) {
    pixelNumber = numColumns * row + column;
    if (pixelNumber < numWells) leds[pixelNumber] = CRGB::Black;
  }
  FastLED.show();
}

void clearColumn(int column) {
  column -= 1;
  if (column < 0 || column >= numColumns) return;
  Serial.print(F("Clearing column: ")); Serial.println(column + 1);
  for (int row = 0; row < numRows; row++) {
    pixelNumber = row * numColumns + column;
    if (pixelNumber < numWells) leds[pixelNumber] = CRGB::Black;
  }
  FastLED.show();
}

void updateDisplay() {
  FastLED.show();
}

CRGB getColorFromCode(const char *code) {
  switch (code[0]) {
    case 'R': return CRGB::Red;
    case 'B': return CRGB::Blue;
    default:  return CRGB::White;
  }
}
