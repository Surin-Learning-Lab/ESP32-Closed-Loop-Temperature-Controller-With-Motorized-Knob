/* Combined sketch:
   - ESP32 AP + web UI (set target temp 25..200)
   - MAX6675 thermocouple (read every 250 ms)
   - 28BYJ-48 + ULN2003 stepper (8-step half-step, high torque)
   - Reversed homing (home = 270°), limit switch on LIMIT_PIN (active LOW)
   - OLED (SSD1306) display
   - Saves targetTemp to Preferences (nonvolatile)
*/

// libs
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <max6675.h>
#include <Preferences.h>

// ---------- hardware config ----------
#define LIMIT_PIN 32   // limit switch (wired to GND when pressed)
bool homed = false;

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Stepper pins (ULN2003 board inputs)
const int IN1 = 14;
const int IN2 = 12;
const int IN3 = 13;
const int IN4 = 15;

// 8-step half-step sequence (best torque + resolution)
const int STEP_COUNT = 8;
const int stepSeq[STEP_COUNT][4] = {
  {1,0,0,0},
  {1,1,0,0},
  {0,1,0,0},
  {0,1,1,0},
  {0,0,1,0},
  {0,0,1,1},
  {0,0,0,1},
  {1,0,0,1}
};

// resolution / speed tuning (you asked for highest torque)
// If you added a 3:1 belt reduction between motor and knob:
const float BELT_RATIO = 3.0;

// Updated resolution & timing so knob angular speed is unchanged
int stepsPerDegree = int(11 * BELT_RATIO);   // 11*3 = 33 steps per knob degree
int delayTimeMs = max(1, int(12.0 / BELT_RATIO)); // 12/3 = 4 ms (use max to avoid 0)

// position tracking (reversed mounting: home = 270°, CW reduces angle)
int currentPositionDeg = 0;    // set at homing
const int maxRotationDeg = 270;

// keep a current step index in the 8-step sequence so we leave coils energized
int currentStepIndex = 0;

// MAX6675 pins
const int thermoSO  = 19;
const int thermoCS  = 5;
const int thermoSCK = 18;
MAX6675 thermocouple(thermoSCK, thermoCS, thermoSO);

// Wifi AP
const char* ap_ssid = "ESP32-TEMP-CONTROL";
const char* ap_pass = "12345678";
WebServer server(80);

// Preferences (nonvolatile storage)
Preferences prefs;
const char *PREF_NAMESPACE = "tempctl";
const char *PREF_TARGET = "targetTemp";

// target temperature
int targetTemp = 60;  // default if not stored

// timing globals
unsigned long lastTempRead = 0;
unsigned long lastMove = 0;
unsigned long lastOledUpdate = 0;
double tempC = 0.0;

// HTML UI (modern single-string UI)
// String htmlPage() {
//   String p =
//     "<!DOCTYPE html><html><head>"
//     "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
//     "<style>"
//     "body{font-family:Arial, sans-serif;background:#f2f2f7;margin:0;padding:20px;text-align:center}"
//     ".card{background:#fff;padding:20px;border-radius:12px;max-width:360px;margin:auto;box-shadow:0 6px 18px rgba(0,0,0,0.12)}"
//     "h2{margin:0 0 8px;color:#222} p{color:#555}"
//     "input[type=number]{width:70%;padding:10px;border-radius:8px;border:1px solid #ccc;font-size:18px;text-align:center}"
//     "button{margin-left:8px;padding:10px 14px;border-radius:8px;border:none;background:#007AFF;color:white;font-size:16px;cursor:pointer}"
//     ".info{margin-top:12px;color:#333;font-size:16px}"
//     "</style></head><body>"
//     "<div class='card'><h2>Temperature Control</h2><p>Enter target temperature (25 - 200 °C)</p>"
//     "<form action='/set'><input type='number' name='t' min='25' max='200' value='" + String(targetTemp) + "'>"
//     "<button type='submit'>Save</button></form>"
//     "<div class='info'>Current Target: <strong>" + String(targetTemp) + " °C</strong></div>"
//     "</div></body></html>";
//   return p;
// }

String htmlPage() {
  String p =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<style>"
    "body{"
      "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;"
      "background:#eef1f5;"
      "margin:0;padding:24px;"
      "display:flex;justify-content:center;align-items:center;"
      "min-height:100vh;"
    "}"
    ".card{"
      "background:#ffffff;"
      "padding:28px;"
      "border-radius:20px;"
      "max-width:420px;width:100%;"
      "box-shadow:0 12px 32px rgba(0,0,0,0.12);"
      "text-align:center;"
      "animation:fadeIn 0.4s ease;"
    "}"
    "@keyframes fadeIn{from{opacity:0;transform:translateY(12px);}to{opacity:1;transform:none;}}"

    "h2{margin:0 0 14px;color:#111827;font-size:24px;font-weight:700;}"
    "p{margin:0 0 20px;color:#6b7280;font-size:15px;}"
    
    ".divider{"
      "width:100%;height:1px;background:#e5e7eb;margin:20px 0;"
    "}"

    "input[type=number]{"
      "width:100%;padding:14px;border-radius:12px;"
      "border:1px solid #d1d5db;"
      "background:#f9fafb;"
      "font-size:20px;font-weight:500;text-align:center;"
      "transition:all 0.3s;"
    "}"
    "input[type=number]:focus{"
      "outline:none;"
      "border-color:#6366f1;"
      "box-shadow:0 0 0 3px rgba(99,102,241,0.25);"
      "background:#ffffff;"
    "}"

    "button{"
      "margin-top:22px;width:100%;padding:14px;border-radius:12px;border:none;"
      "background:linear-gradient(135deg,#4f46e5,#3b82f6);"
      "color:white;font-size:18px;font-weight:600;"
      "cursor:pointer;"
      "transition:transform 0.15s,box-shadow 0.15s;"
      "box-shadow:0 4px 14px rgba(63,131,248,0.35);"
    "}"
    "button:hover{transform:translateY(-2px);box-shadow:0 6px 18px rgba(63,131,248,0.4);}"
    "button:active{transform:scale(0.98);}"

    ".info{margin-top:24px;color:#374151;font-size:17px;font-weight:500;}"
    "</style></head><body>"

    "<div class='card'>"
      "<h2>Temperature Control</h2>"
      "<p>Set your target temperature</p>"

      "<form action='/set'>"
        "<input type='number' name='t' min='25' max='200' value='" + String(targetTemp) + "'>"
        "<button type='submit'>Save</button>"
      "</form>"

      "<div class='divider'></div>"

      "<div class='info'>Current Target:<br><strong>" + String(targetTemp) + " </strong></div>"
    "</div>"

    "</body></html>";
  
  return p;
}


// ----- utility: write one step (energize coils) -----
void writeStep(int idx) {
  digitalWrite(IN1, stepSeq[idx][0]);
  digitalWrite(IN2, stepSeq[idx][1]);
  digitalWrite(IN3, stepSeq[idx][2]);
  digitalWrite(IN4, stepSeq[idx][3]);
}

// ----- rotate one degree (reversed logic) -----
// Reversed mounting: CW decreases angle; CCW increases angle
void rotateCW_1deg() {
  if (!homed) return;
  if (currentPositionDeg - 1 < 0) {
    Serial.println("CW BLOCKED — Min 0°");
    return;
  }
  for (int s = 0; s < stepsPerDegree; s++) {
    currentStepIndex = (currentStepIndex + 1) % STEP_COUNT; // physical CW
    writeStep(currentStepIndex);
    delay(delayTimeMs);
  }
  currentPositionDeg--;   // CW reduces angle
  // coils remain energized for holding torque
}

void rotateCCW_1deg() {
  if (!homed) return;
  if (currentPositionDeg + 1 > maxRotationDeg) {
    Serial.println("CCW BLOCKED — Max 270°");
    return;
  }
  for (int s = 0; s < stepsPerDegree; s++) {
    currentStepIndex = (currentStepIndex - 1);
    if (currentStepIndex < 0) currentStepIndex += STEP_COUNT;
    writeStep(currentStepIndex); // physical CCW
    delay(delayTimeMs);
  }
  currentPositionDeg++;  // CCW increases angle
}

int homingDelayMs = max(1, int(8.0 / BELT_RATIO)); // becomes 2 or 3 ms for beltRatio=3
// then use delay(homingDelayMs) instead of delay(8)


// ----- homing (reversed): move CW until switch pressed, set pos = 270 -----
void homeStepper() {
  Serial.println("Homing (reversed)...");
  pinMode(LIMIT_PIN, INPUT_PULLUP);

  // gently step CW until switch closes (active LOW)
  while (digitalRead(LIMIT_PIN) == HIGH) {
    currentStepIndex = (currentStepIndex + 1) % STEP_COUNT; // CW
    writeStep(currentStepIndex);
    delay(homingDelayMs); // adjusted for belt ratio
  }

  Serial.println("Limit reached. Setting position = 270°");
  currentPositionDeg = 2;
  homed = true;

  // move slightly CCW away from switch to release it (3 degrees)
  for (int d = 0; d < 3; d++) {
    for (int s = 0; s < stepsPerDegree; s++) {
      currentStepIndex = (currentStepIndex - 1);
      if (currentStepIndex < 0) currentStepIndex += STEP_COUNT;
      writeStep(currentStepIndex);
      delay(8);
    }
    currentPositionDeg++;
  }

  Serial.println("Homing complete.");
}

// ----- web handlers -----
void handleRoot() { server.send(200, "text/html", htmlPage()); }
void handleSet() {
  if (server.hasArg("t")) {
    int v = server.arg("t").toInt();
    if (v < 25) v = 25; if (v > 200) v = 200;
    targetTemp = v;
    // save to preferences
    prefs.putInt(PREF_TARGET, targetTemp);
    Serial.printf("New target saved: %d\n", targetTemp);
  }
  server.send(200, "text/html", htmlPage());
}

// ----- OLED display -----
void oledShow(double t) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0,0);
  display.print("T:");
  display.print(t,1);
  display.println("C");

  display.setTextSize(1);
  display.setCursor(0,36);
  display.print("Target: ");
  display.print(targetTemp);
  display.print(" C");

  display.setCursor(0,50);
  display.print("Pos:");
  display.print(currentPositionDeg);
  display.print(" deg");

  display.display();
}

void setup() {
  Serial.begin(115200);

  // pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  // set motors off initially (but safe to leave last step later)
  writeStep(0);

  // OLED
  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  // Preferences - load saved target if exists
  prefs.begin(PREF_NAMESPACE, false);
  if (prefs.isKey(PREF_TARGET)) {
    targetTemp = prefs.getInt(PREF_TARGET, targetTemp);
    Serial.printf("Loaded saved target: %d\n", targetTemp);
  } else {
    prefs.putInt(PREF_TARGET, targetTemp);
  }

  // Wifi AP
  WiFi.softAP(ap_ssid, ap_pass);
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();

  // thermocouple is constructed earlier

  // Home the system (reversed homing)
  homeStepper();

  // initial OLED
  oledShow(0.0);

  // give user reminder
  Serial.println("Setup complete. Use good 5V supply (2-3A) and add 470-1000uF cap at motor supply.");
}

void loop() {
  server.handleClient();

  unsigned long now = millis();

  // Read temp every 250ms (MAX6675 ~220 ms conversion)
  if (now - lastTempRead >= 250) {
    lastTempRead = now;
    tempC = thermocouple.readCelsius();
    // update OLED every 500ms to reduce flicker
    if (now - lastOledUpdate >= 500) {
      lastOledUpdate = now;
      oledShow(tempC);
    }
  }

  // Movement decision every 5 seconds (slow, high torque)
  if (now - lastMove >= 5000) {
    lastMove = now;

    if (!homed) {
      Serial.println("Not homed - skipping movement");
    } else {
if (tempC < targetTemp - 0.1) {
  // Temperature too low -> need MORE heat -> turn knob UP
  rotateCCW_1deg();  // THIS is the correct "increase temp" direction
  Serial.printf("Increase temp: CCW -> pos %d\n", currentPositionDeg);
}
else if (tempC > targetTemp + 0.1) {
  // Temperature too high -> need LESS heat -> turn knob DOWN
  rotateCW_1deg();   // THIS is the correct "decrease temp" direction
  Serial.printf("Decrease temp: CW -> pos %d\n", currentPositionDeg);
}


      // if (tempC < targetTemp - 0.1) {         // tiny hysteresis to avoid jitter
      //   // temp too low -> increase angle (reversed: CCW increases)
      //   rotateCCW_1deg();
      //   Serial.printf("Moved CCW -> pos: %d\n", currentPositionDeg);
      // } else if (tempC > targetTemp + 0.1) {
      //   // temp too high -> decrease angle (reversed: CW decreases)
      //   rotateCW_1deg();
      //   Serial.printf("Moved CW -> pos: %d\n", currentPositionDeg);
      else {
        // within hysteresis window, hold current position (coils energized)
        Serial.printf("Holding. Temp %.2f within target %.1f\n", tempC, (double)targetTemp);
      }
    }
  }
}
