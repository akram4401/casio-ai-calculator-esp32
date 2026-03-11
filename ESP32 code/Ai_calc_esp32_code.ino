#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

// WiFi Config
const char* ssid = "100";
const char* password = "12345678";

// Gemini API Config
const char* Gemini_Token = "AIzaSyAk3jU-Qa-maH1L-a5tnOTSXwDC3g1svB0";
const char* Gemini_Max_Tokens = "80";

// Telegram Config
const String botToken = "8709556036:AAHQcqkQk-yq18msPYlzyTG5XHTYihz-3Oc";
const String chatId = "892762707";
unsigned long lastTelegramCheck = 0;
const unsigned long telegramInterval = 5000;
String lastMessageId = "";

// Display Setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// MCP23017 Keypad Setup
Adafruit_MCP23X17 mcp;
uint8_t rowPins[6] = {0, 1, 2, 3, 4, 5};
uint8_t colPins[9] = {8, 9, 10, 11, 12, 13, 14, 15, 6};

// Dual keyboard layouts
const char* keys[6][9] = {
  { nullptr, nullptr, nullptr, nullptr, "p",   "5",    "BATTERY", "SPACE",      nullptr },
  { "n",     "k",     "m",     "j",     "o",   "4",    "SCAN",    "CLEAR",      "MODE" },
  { "b",     "h",     "l",     "u",     "i",   "3",    "9",       "ZOOM+",      "UP" },
  { "v",     "g",     "t",     "y",     "e",   "2",    "8",       "DOWN",       "ZOOM-" },
  { "c",     "f",     "s",     "r",     "z",   "1",    "7",       "UPER",       "\b" },
  { "x",     "d",     "w",     "q",     "a",   "0",    "6",       "KEYBOARD",   "\n" },
};

const char* symbolKeys[6][9] = {
  { nullptr, nullptr, nullptr, nullptr, "@",   "%",    "BATTERY",   "SPACE",     nullptr },
  { "=",     "-",     "/",     "\b",    "_",   "|",    "SCAN",      "CLEAR",     "MODE" },
  { "^",     "+",     "*",     "\n",    ")",   "!",    "<",         "ZOOM+",      "UP" },
  { "SPACE", "3",     "6",     "9",     "(",   "?",    ">",         "DOWN",      "ZOOM-" },
  { ".",     "2",     "5",     "8",     "'",   ",",    ":",         "UPER",      "\b" },
  { "0",     "1",     "4",     "7",     "\"",   ".",    "&",         "KEYBOARD",  "\n" },
};

// Battery monitoring parameters
const int BATTERY_PIN = 34;              // ADC pin for battery voltage measurement
const float VOLTAGE_DIVIDER_RATIO = 2.0;  // 150k+150k divider (1:1 ratio → 2.0 multiplier)
const float ADC_REFERENCE = 3.3;          // ESP32's internal reference voltage
const int ADC_MAX = 4095;                 // For 12-bit ADC

// Battery voltage range
const float VOLTAGE_MAX = 3.8f;           // 100% charge voltage
const float VOLTAGE_MIN = 2.9f;           // 0% charge voltage
const float LOW_BATTERY_THRESHOLD = 2.92f; // Voltage warning threshold

// Battery animation bitmaps
const unsigned char battery_bitmap_empty[] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x78, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x78, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x08, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f
};

const unsigned char battery_bitmap_low[] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x08, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x7d, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x08, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x7d, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x7d, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x78, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x78, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x7d, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x78, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x7d, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 
  0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x08, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x08, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f
};

const int battery_bitmap_count = 2;
const unsigned char* battery_bitmaps[2] = {
  battery_bitmap_low,
  battery_bitmap_empty
};

// Globals
String buffer = "";
String lastKey = "";
unsigned long lastPress = 0;
int mode = 0;
String res = "";
unsigned long lastGeminiRequest = 0;
const unsigned long geminiCooldown = 3000;
bool waitingForResponse = false;

int scrollIndex = 0;
String currentDisplayText = "";

// Battery variables
float batteryVoltage = 0.0;
int batteryPercent = 0;
bool lowBatteryActive = false;
bool showBatteryStatus = false;
unsigned long batteryDisplayStart = 0;
const unsigned long batteryDisplayDuration = 3000; // 3 seconds
int lowBatteryWarnings = 0;
const int maxLowBatteryWarnings = 3;
unsigned long lastLowBatteryWarning = 0;
const unsigned long lowBatteryWarningInterval = 10000; // 10 seconds
bool batteryLocked = false;

// New variables for keyboard features
bool useSymbolKeyboard = false;
bool useUpperCase = false;
int textSize = 1; // For zoom functionality
Preferences preferences; // For storing WiFi credentials

// WiFi scanning variables
bool scanningWifi = false;
int numNetworks = 0;
int selectedNetwork = 0;
String networkNames[20]; // Store up to 20 networks
String wifiPassword = "";
bool enteringPassword = false;
bool showPassword = false; // New variable for showing/hiding password

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // Initialize preferences for storing WiFi credentials
  preferences.begin("wifi-config", false);

  // Configure ADC for battery monitoring
  analogReadResolution(12);          // Set ADC to 12-bit resolution
  analogSetAttenuation(ADC_11db);    // Set full 0-3.3V range
  pinMode(BATTERY_PIN, INPUT);

  // Init display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Display init failed");
    while (1);
  }
  display.clearDisplay();
  display.display();

  // Try to connect to saved WiFi if available
  tryConnectSavedWifi();

  // Init MCP
  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed");
    while (1);
  }

  // Set keypad pins
  for (int i = 0; i < 9; i++) {
    mcp.pinMode(colPins[i], OUTPUT);
    mcp.digitalWrite(colPins[i], HIGH);
  }
  for (int i = 0; i < 6; i++) {
    mcp.pinMode(rowPins[i], INPUT_PULLUP);
  }

  showMode();
}

void tryConnectSavedWifi() {
  // Check if we have saved credentials
  String savedSSID = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");
  
  if (savedSSID.length() > 0) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Connecting to saved WiFi...");
    display.println(savedSSID);
    display.display();
    
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    
    unsigned long wifiTimeout = millis() + 10000;
    while (WiFi.status() != WL_CONNECTED && millis() < wifiTimeout) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Connected to WiFi");
      display.println(savedSSID);
      display.display();
      delay(1000);
    } else {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Failed to connect");
      display.println("Press SCAN to search");
      display.display();
      delay(2000);
    }
  } else {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("No saved WiFi");
    display.println("Press SCAN to search");
    display.display();
    delay(2000);
  }
}

void loop() {
  // Update battery status
  updateBatteryStatus();

  // Handle battery display if active
  if (showBatteryStatus && millis() - batteryDisplayStart > batteryDisplayDuration) {
    showBatteryStatus = false;
    if (!batteryLocked) {
      if (scanningWifi && enteringPassword) {
        displayPasswordEntry();
      } else if (scanningWifi) {
        displayWifiNetworks();
      } else {
        showMode();
      }
    }
  }

  // Handle low battery warnings
  if (lowBatteryActive && !batteryLocked && !showBatteryStatus && 
      millis() - lastLowBatteryWarning > lowBatteryWarningInterval) {
    showBatteryStatus = true;
    batteryDisplayStart = millis();
    lastLowBatteryWarning = millis();
    lowBatteryWarnings++;
    if (lowBatteryWarnings >= maxLowBatteryWarnings) {
      batteryLocked = true;
    }
  }

  // If battery is locked in low state, only show battery status
  if (batteryLocked) {
    displayBatteryStatus();
    return;
  }

  // Normal operation if battery isn't locked
  String key = scanKeypad();
  if (key != "" && key != lastKey) {
    lastKey = key;
    lastPress = millis();

    // Handle special keys
    if (key == "MODE") {
      if (!scanningWifi) {
        mode = !mode;
        buffer = "";
        res = "";
        waitingForResponse = false;
        scrollIndex = 0;
        showMode();
      }
    } else if (key == "KEYBOARD") {
      if (scanningWifi && enteringPassword) {
        // Toggle password visibility when entering WiFi password
        showPassword = !showPassword;
        displayPasswordEntry();
      } else {
        useSymbolKeyboard = !useSymbolKeyboard;
        String keyboardType = useSymbolKeyboard ? "Symbol" : "Alphabet";
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Keyboard: " + keyboardType);
        display.display();
        delay(1000);
        if (mode == 0) {
          updateOLED(buffer);
        } else {
          updateOLED(res);
        }
      }
    } else if (key == "UPER") {
      useUpperCase = !useUpperCase;
      String caseType = useUpperCase ? "UPPERCASE" : "lowercase";
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Case: " + caseType);
      display.display();
      delay(1000);
      if (mode == 0) {
        updateOLED(buffer);
      } else {
        updateOLED(res);
      }
    } else if (key == "ZOOM+") {
      textSize = min(textSize + 1, 3); // Max size 3
      updateTextSize();
    } else if (key == "ZOOM-") {
      textSize = max(textSize - 1, 1); // Min size 1
      updateTextSize();
    } else if (key == "SCAN") {
      startWifiScan();
    } else if (key == "UP" && scanningWifi) {
      selectedNetwork = max(0, selectedNetwork - 1);
      displayWifiNetworks();
    } else if (key == "DOWN" && scanningWifi) {
      selectedNetwork = min(numNetworks - 1, selectedNetwork + 1);
      displayWifiNetworks();
    } else if (key == "UP" && !scanningWifi) {
      scrollIndex = max(0, scrollIndex - 1);
      updateOLED(currentDisplayText);
    } else if (key == "DOWN" && !scanningWifi) {
      scrollIndex++;
      updateOLED(currentDisplayText);
    } else if (key == "BATTERY") {
      showBatteryStatus = true;
      batteryDisplayStart = millis();
      displayBatteryStatus();
    } else if (key == "\b" && scanningWifi && enteringPassword) {
      // Handle backspace in password entry mode
      if (wifiPassword.length() > 0) {
        wifiPassword.remove(wifiPassword.length() - 1);
        displayPasswordEntry();
      }
    } else if (key == "\n" && scanningWifi && enteringPassword) {
      // Connect to selected network with password
      connectToWifi(networkNames[selectedNetwork], wifiPassword);
      scanningWifi = false;
      enteringPassword = false;
      showPassword = false; // Reset password visibility
    } else if (key == "\b" && scanningWifi && !enteringPassword) {
      // Select network and enter password
      enteringPassword = true;
      wifiPassword = "";
      showPassword = false; // Reset password visibility
      displayPasswordEntry();
    } else if (key == "CLEAR" && scanningWifi && enteringPassword) {
      // Clear password when in password entry mode
      wifiPassword = "";
      displayPasswordEntry();
    } else if (scanningWifi && enteringPassword) {
      // Add character to password
      wifiPassword += key;
      displayPasswordEntry();
    } else {
      if (mode == 0) telegramMode(key);
      else geminiMode(key);
    }
  }

  if (millis() - lastPress > 300) lastKey = "";

  if (mode == 0 && millis() - lastTelegramCheck > telegramInterval) {
    checkTelegramMessages();
    lastTelegramCheck = millis();
  }
}

void updateTextSize() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(textSize);
  display.println("Zoom level: " + String(textSize));
  display.display();
  delay(1000);
  
  display.setTextSize(textSize);
  if (mode == 0) {
    updateOLED(buffer);
  } else {
    updateOLED(res);
  }
}

void startWifiScan() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Scanning WiFi...");
  display.display();
  
  scanningWifi = true;
  WiFi.disconnect();
  numNetworks = WiFi.scanNetworks();
  
  if (numNetworks == 0) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("No networks found");
    display.display();
    delay(2000);
    scanningWifi = false;
    showMode();
  } else {
    // Store network names
    for (int i = 0; i < min(numNetworks, 20); i++) {
      networkNames[i] = WiFi.SSID(i);
    }
    selectedNetwork = 0;
    displayWifiNetworks();
  }
}

void displayWifiNetworks() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("WiFi Networks:");
  
  int startIdx = max(0, selectedNetwork - 1);
  for (int i = startIdx; i < min(startIdx + 3, numNetworks); i++) {
    if (i == selectedNetwork) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    display.println(networkNames[i]);
  }
  
  display.println("\nPress \b to select");
  display.display();
}

void displayPasswordEntry() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Enter password for:");
  display.println(networkNames[selectedNetwork]);
  display.println();
  
  // Show password with asterisks or in plain text based on showPassword flag
  if (showPassword) {
    display.println(wifiPassword);
  } else {
    String maskedPwd = "";
    for (int i = 0; i < wifiPassword.length(); i++) {
      maskedPwd += "*";
    }
    display.println(maskedPwd);
  }
  
  display.println("\nKEYBOARD: Show/Hide");
  display.println("Enter: Connect");
  display.display();
}

void connectToWifi(String ssid, String pwd) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting to:");
  display.println(ssid);
  display.display();
  
  WiFi.begin(ssid.c_str(), pwd.c_str());
  
  unsigned long wifiTimeout = millis() + 10000;
  while (WiFi.status() != WL_CONNECTED && millis() < wifiTimeout) {
    delay(500);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connected!");
    display.println("Saving credentials...");
    display.display();
    
    // Save credentials
    preferences.putString("ssid", ssid);
    preferences.putString("password", pwd);
    
    delay(2000);
    showMode();
  } else {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connection failed");
    display.println("Try again");
    display.display();
    delay(2000);
    startWifiScan();
  }
}

void updateBatteryStatus() {
  // Take 16 samples and average them for stability
  const int samples = 16;
  int adcValue = 0;
  
  for(int i = 0; i < samples; i++) {
    adcValue += analogRead(BATTERY_PIN);
    delay(1);
  }
  adcValue /= samples;

  // Calculate battery voltage
  batteryVoltage = (adcValue * ADC_REFERENCE / ADC_MAX) * VOLTAGE_DIVIDER_RATIO;
  batteryVoltage = constrain(batteryVoltage, VOLTAGE_MIN, VOLTAGE_MAX); // Clamp to min/max range

  // Calculate battery percentage
  batteryPercent = ((batteryVoltage - VOLTAGE_MIN) / (VOLTAGE_MAX - VOLTAGE_MIN)) * 100.0f;
  batteryPercent = constrain(batteryPercent, 0, 100); // Ensure 0-100 range

  // Check if battery is low
  lowBatteryActive = (batteryVoltage <= LOW_BATTERY_THRESHOLD);
}

void displayBatteryStatus() {
  display.clearDisplay();
  
  if (lowBatteryActive) {
    // Show animation frame based on time
    int frame = (millis() / 500) % battery_bitmap_count;
    display.drawBitmap(28, 5, battery_bitmaps[frame], 71, 21, SSD1306_WHITE);
    
    // Add warning text and percentage
    display.setCursor(10, 30);
    display.print("LOW BATTERY! ");
    display.print(batteryPercent);
    display.print("%");
  } else {
    // Display normal battery status
    display.setCursor(0, 0);
    display.print("Battery: ");
    display.print(batteryVoltage, 2);
    display.print("V (");
    display.print(batteryPercent);
    display.print("%)");
    
    // Draw a simple battery icon
    display.drawRect(40, 15, 50, 12, SSD1306_WHITE);
    display.fillRect(90, 18, 3, 6, SSD1306_WHITE);
    int fillWidth = map(batteryPercent, 0, 100, 0, 48);
    display.fillRect(41, 16, fillWidth, 10, SSD1306_WHITE);
  }
  
  display.display();
}

String scanKeypad() {
  static unsigned long lastKeyTime = 0;
  const unsigned long debounceTime = 150;

  for (int c = 0; c < 9; c++) {
    mcp.digitalWrite(colPins[c], LOW);

    for (int r = 0; r < 6; r++) {
      if (mcp.digitalRead(rowPins[r]) == LOW) {
        if (millis() - lastKeyTime > debounceTime) {
          lastKeyTime = millis();
          
          // Get the key based on current keyboard mode
          const char* val = useSymbolKeyboard ? symbolKeys[r][c] : keys[r][c];
          mcp.digitalWrite(colPins[c], HIGH);
          
          if (val) {
            String keyVal = String(val);
            
            // Handle uppercase if needed (only for alphabet keys)
            if (!useSymbolKeyboard && useUpperCase && keyVal.length() == 1) {
              char c = keyVal.charAt(0);
              if (c >= 'a' && c <= 'z') {
                keyVal = String((char)(c - 32)); // Convert to uppercase
              }
            }
            
            return keyVal;
          } else {
            return "";
          }
        }
      }
    }

    mcp.digitalWrite(colPins[c], HIGH);
  }
  return "";
}

void updateOLED(String text) {
  if (showBatteryStatus || batteryLocked || scanningWifi) return;
  
  currentDisplayText = text;
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(textSize);

  int charsPerLine = 21 / textSize;
  int linesPerScreen = 4 / textSize;
  
  int y = 0;
  int lineCount = 0;
  int visibleLines = 0;
  
  // Split text into words for proper word wrapping
  String words[100]; // Assuming max 100 words
  int wordCount = 0;
  
  // Parse words from text
  String currentWord = "";
  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    
    if (c == ' ' || c == '\n') {
      if (currentWord.length() > 0) {
        words[wordCount++] = currentWord;
        currentWord = "";
      }
      
      // Add space or newline as separate "word"
      if (c == ' ') {
        words[wordCount++] = " ";
      } else if (c == '\n') {
        words[wordCount++] = "\n";
      }
    } else {
      currentWord += c;
    }
  }
  
  // Add the last word if there is one
  if (currentWord.length() > 0) {
    words[wordCount++] = currentWord;
  }
  
  // Now render words with proper wrapping
  String currentLine = "";
  int startWordIndex = 0;
  
  // Skip words based on scroll position
  int linesSkipped = 0;
  int wordIndex = 0;
  
  while (wordIndex < wordCount && linesSkipped < scrollIndex) {
    String word = words[wordIndex++];
    
    if (word == "\n") {
      // Explicit newline
      linesSkipped++;
      currentLine = "";
    } else if (currentLine.length() + word.length() > charsPerLine && word != " ") {
      // Line would be too long, wrap
      linesSkipped++;
      currentLine = "";
      
      // If this word isn't a space, we need to process it on the next line
      if (word != " ") {
        wordIndex--;
      }
    } else {
      // Add word to current line
      currentLine += word;
    }
  }
  
  // Reset for actual rendering
  currentLine = "";
  
  // Render visible lines
  while (wordIndex < wordCount && visibleLines < linesPerScreen) {
    String word = words[wordIndex++];
    
    if (word == "\n") {
      // Explicit newline
      display.println();
      y += 8 * textSize;
      display.setCursor(0, y);
      visibleLines++;
      currentLine = "";
    } else if (currentLine.length() + word.length() > charsPerLine && word != " ") {
      // Line would be too long, wrap to next line
      display.println();
      y += 8 * textSize;
      display.setCursor(0, y);
      visibleLines++;
      currentLine = "";
      
      // If this word isn't a space, we need to process it on the next line
      if (word != " ") {
        display.print(word);
        currentLine = word;
      }
    } else {
      // Add word to current line and display
      display.print(word);
      currentLine += word;
    }
    
    // Check if we've reached the screen limit
    if (visibleLines >= linesPerScreen) {
      break;
    }
  }

  display.display();
}

void showMode() {
  if (showBatteryStatus || batteryLocked || scanningWifi) return;
  
  String modeText = (mode == 0) ? "Telegram" : "AI Assistant";
  String keyboardType = useSymbolKeyboard ? "Symbol" : "Alphabet";
  String caseType = useUpperCase ? "UPPER" : "lower";
  
  updateOLED("Mode: " + modeText + "\nKeyboard: " + keyboardType + "\nCase: " + caseType + "\n> ");
}

// ---------------- Telegram ----------------

void telegramMode(String key) {
  if (key == "SPACE") buffer += " ";
  else if (key == "CLEAR") buffer = "";
  else if (key == "\n") {
    sendTelegramMessage(buffer);
    buffer += "\n";
  } else if (key == "\b") {
    if (buffer.length() > 0) buffer.remove(buffer.length() - 1);
  } else {
    buffer += key;
  }
  updateOLED(buffer);
}

void sendTelegramMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    updateOLED("No WiFi");
    return;
  }

  HTTPClient http;
  http.begin("https://api.telegram.org/bot" + botToken + "/sendMessage");
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(256);
  doc["chat_id"] = chatId;
  doc["text"] = message;

  String body;
  serializeJson(doc, body);
  int httpCode = http.POST(body);
  if (httpCode != 200) {
    Serial.printf("Telegram error: %d\n", httpCode);
  }

  http.end();
}

void checkTelegramMessages() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + botToken + "/getUpdates";
  if (!lastMessageId.isEmpty()) {
    url += "?offset=" + String(lastMessageId.toInt() + 1);
  }

  http.begin(url);
  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    JsonArray results = doc["result"];
    if (results.size() > 0) {
      JsonObject lastMsg = results[results.size() - 1];
      String msgId = String((long)lastMsg["update_id"]);
      String msgText = lastMsg["message"]["text"] | "";

      if (msgId != lastMessageId && msgText.length() > 0) {
        lastMessageId = msgId;
        buffer += "\n" + msgText;
        updateOLED(buffer);
      }
    }
  }

  http.end();
}

// ---------------- Gemini ----------------

void geminiMode(String key) {
  if (waitingForResponse) return;

  if (key == "SPACE") res += " ";
  else if (key == "CLEAR") res = "";
  else if (key == "\n" && res.length() > 0) {
    if (millis() - lastGeminiRequest > geminiCooldown) {
      String question = "In short: " + res;
      res = "Asking...";
      updateOLED(res);
      waitingForResponse = true;
      askGemini(question);
      lastGeminiRequest = millis();
      return;
    }
  } else if (key == "\b" && res.length() > 0) res.remove(res.length() - 1);
  else if (key != "\n") res += key;

  updateOLED(res);
}

void askGemini(String question) {
  if (WiFi.status() != WL_CONNECTED) {
    updateOLED("No WiFi");
    waitingForResponse = false;
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);

  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-lite:generateContent?key=" + String(Gemini_Token);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"contents\":[{\"parts\":[{\"text\":\"" + question + "\"}]}],\"generationConfig\":{\"maxOutputTokens\":" + String(Gemini_Max_Tokens) + "}}";
  int code = http.POST(body);

  if (code == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, response);

    if (doc.containsKey("candidates")) {
      String answer = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
      updateOLED(answer);
    } else {
      updateOLED("Response error");
    }
  } else {
    updateOLED("API error " + String(code));
  }

  http.end();
  waitingForResponse = false;
}