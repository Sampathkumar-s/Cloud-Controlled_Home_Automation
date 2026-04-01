#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <time.h>

// ── WiFi Credentials ──────────────────────────────────────
const char* ssid     = "raghav";
const char* password = "qwertyuiop";

// ── Firebase Config ───────────────────────────────────────
const char* FIREBASE_HOST    = "smart-light-eec9a-default-rtdb.firebaseio.com";
const char* FIREBASE_API_KEY = "AIzaSyCyfeV54y8udccB9MA3idNV54uxtJx-GTM";
// Firebase link : https://console.firebase.google.com/project/smart-light-eec9a/database
// ── Pin Definitions ───────────────────────────────────────
#define LED_PIN   2
#define SERVO_PIN 13   // Connect servo signal wire here

// ── NTP Time Config (IST) ─────────────────────────────────
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET = 19800;  // IST UTC+5:30
const int   DST_OFFSET = 0;

// ── Servo Object ──────────────────────────────────────────
Servo myServo;

// ── State Tracking ────────────────────────────────────────
String prevLed   = "";
String prevServo = "";

// ─────────────────────────────────────────────────────────
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Time Error";
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

// ─────────────────────────────────────────────────────────
void firebasePut(String path, String value) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "https://" + String(FIREBASE_HOST) + path + ".json?key=" + FIREBASE_API_KEY;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT("\"" + value + "\"");
  Serial.println("PUT " + path + " = " + value + " [" + String(code) + "]");
  http.end();
}

// ─────────────────────────────────────────────────────────
String firebaseGet(String path) {
  if (WiFi.status() != WL_CONNECTED) return "";
  HTTPClient http;
  String url = "https://" + String(FIREBASE_HOST) + path + ".json?key=" + FIREBASE_API_KEY;
  http.begin(url);
  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    payload.replace("\"", "");
    payload.trim();
    http.end();
    return payload;
  }
  http.end();
  return "";
}

// ─────────────────────────────────────────────────────────
void logHistory(String device, String action) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "https://" + String(FIREBASE_HOST) + "/history.json?key=" + FIREBASE_API_KEY;
  String timestamp = getTimestamp();
  String body = "{\"device\":\"" + device +
                "\",\"action\":\"" + action +
                "\",\"time\":\"" + timestamp + "\"}";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.POST(body);
  Serial.println("History → " + device + " " + action + " @ " + timestamp);
  http.end();
}

// ─────────────────────────────────────────────────────────
void applyLED(String state) {
  digitalWrite(LED_PIN, state == "ON" ? HIGH : LOW);
  Serial.println("LED → " + state);
  firebasePut("/status/led", state);
  logHistory("LED", state);
}

void applyServo(String angleStr) {
  int angle = angleStr.toInt();
  angle = constrain(angle, 0, 180);  // Safety clamp
  myServo.write(angle);
  Serial.println("SERVO → " + String(angle) + "°");
  firebasePut("/status/servo", String(angle));
  // Log as ON if angle > 0, OFF if 0
  logHistory("SERVO", angle > 0 ? "ON (" + String(angle) + "deg)" : "OFF (0deg)");
}

// ─────────────────────────────────────────────────────────
void reconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("WiFi lost — reconnecting...");
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("\nReconnected: " + WiFi.localIP().toString());
}

// ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  // Pin setup
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Servo setup
  myServo.attach(SERVO_PIN);
  myServo.write(0);  // Start at 0 degrees

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // NTP
  configTime(GMT_OFFSET, DST_OFFSET, NTP_SERVER);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
    Serial.println("Time synced: " + getTimestamp());
  else
    Serial.println("Time sync failed");

  // Set initial states in Firebase
  firebasePut("/controls/led",   "OFF");
  firebasePut("/controls/servo", "0");
  firebasePut("/status/led",     "OFF");
  firebasePut("/status/servo",   "0");

  Serial.println("ESP32 ready!");
}

// ─────────────────────────────────────────────────────────
void loop() {
  reconnectWiFi();

  String ledCmd   = firebaseGet("/controls/led");
  String servoCmd = firebaseGet("/controls/servo");

  if (ledCmd != "" && ledCmd != prevLed) {
    applyLED(ledCmd);
    prevLed = ledCmd;
  }

  if (servoCmd != "" && servoCmd != prevServo) {
    applyServo(servoCmd);
    prevServo = servoCmd;
  }

  delay(2000);
}