#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "HX711.h"

// LCD configuration
LiquidCrystal_I2C lcd(0x27, 16, 2);

const String adminUsername = "admin";
const String adminPassword = "password123";
bool isAuthenticated = false;

// Wi-Fi credentials for the Access Point
const char* ssid = "BottleVendingMachine"; // Removed password

// Web server on port 80
WebServer server(80);

// Servo pins
Servo servo1;
Servo servo2;
#define SERVO1_PIN 27
#define SERVO2_PIN 26

// Buzzer pin
#define BUZZER_PIN 33

// HX711 weight sensor pins
#define DT 18
#define SCK 19
HX711 scale;
const float calibration_factor = -7050; // Adjust as needed for calibration

// Ultrasonic sensor pins
#define TRIGGER_PIN 17
#define ECHO_PIN 16

// Device timers and data usage tracking
struct DeviceTimer {
  String ip;
  int remainingTime;
  unsigned long dataUsed; // in bytes
};

DeviceTimer deviceTimers[10];
int deviceCount = 0;

// Function to get distance from ultrasonic sensor
long getDistance() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = duration * 0.034 / 2;
  return distance;
}

// Handle root webpage
void handleRoot() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Bottle Vending Machine</title>
      <style>
        body { font-family: Arial, sans-serif; text-align: center; background: linear-gradient(to bottom, #4facfe, #00f2fe); color: white; margin: 0; padding: 0; }
        .container { max-width: 600px; margin: 50px auto; padding: 20px; background: rgba(255, 255, 255, 0.1); border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); }
        h1 { font-size: 2.5em; margin-bottom: 10px; }
        p { font-size: 1.2em; margin: 15px 0; }
        .button { display: inline-block; background: #28a745; color: white; padding: 10px 20px; text-decoration: none; border-radius: 5px; font-size: 1.2em; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.2); }
        .button:hover { background: #218838; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Welcome to the Bottle Vending Machine</h1>
        <p>Deposit a bottle and earn internet access time!</p>
        <p><b>Remaining Time:</b> <span id="time">0 seconds</span></p>
        <a href="/insert" class="button">Insert Bottle</a>
        <br><br>
        <a href="/admin" class="button">Admin Panel</a>
      </div>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// Admin panel to view connected devices and their data usage
void handleAdmin() {
  if (!isAuthenticated) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirecting...");
    return;
  }

  String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Admin Panel</title>
      <style>
        body { font-family: Arial, sans-serif; text-align: center; background: linear-gradient(to bottom, #4facfe, #00f2fe); color: white; }
        .container { max-width: 600px; margin: 50px auto; padding: 20px; background: rgba(255, 255, 255, 0.1); border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); }
        table { width: 100%; margin-top: 20px; border-collapse: collapse; }
        th, td { padding: 8px; text-align: center; border: 1px solid white; }
        th { background-color: #28a745; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Admin Panel</h1>
        <table>
          <tr>
            <th>IP Address</th>
            <th>Remaining Time (seconds)</th>
            <th>Data Used (MB)</th>
          </tr>
  )rawliteral";

  for (int i = 0; i < deviceCount; i++) {
    html += "<tr><td>" + deviceTimers[i].ip + "</td><td>" + String(deviceTimers[i].remainingTime) + "</td><td>" + String(deviceTimers[i].dataUsed / 1024.0 / 1024.0, 2) + "</td></tr>";
  }

  html += R"rawliteral(
        </table>
        <br><br>
        <a href="/" class="button">Go Back</a>
      </div>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// Handle bottle insertion
void handleInsert() {
  // Check if a bottle is present using the ultrasonic sensor
  long distance = getDistance();
  if (distance > 10) { // Adjust this threshold based on your setup
    String htmlResponse = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Bottle Vending Machine</title>
        <style>
          body { font-family: Arial, sans-serif; text-align: center; background: linear-gradient(to bottom, #4facfe, #00f2fe); color: white; margin: 0; padding: 0; }
          .container { max-width: 600px; margin: 50px auto; padding: 20px; background: rgba(255, 255, 255, 0.1); border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); }
          h1 { font-size: 2.5em; margin-bottom: 10px; }
          p { font-size: 1.2em; margin: 15px 0; }
          .button { display: inline-block; background: #28a745; color: white; padding: 10px 20px; text-decoration: none; border-radius: 5px; font-size: 1.2em; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.2); }
          .button:hover { background: #218838; }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>Result</h1>
          <p><b>Status:</b> No bottle detected. Please insert a bottle.</p>
          <p><a href="/" class="button">Go Back</a></p>
        </div>
      </body>
      </html>
    )rawliteral";
    server.send(200, "text/html", htmlResponse);
    return;
  }

  servo1.write(90); // Open the deposit slot
  delay(3000);
  servo1.write(0);  // Close the deposit slot

  float weight = scale.get_units(10); // Get weight of the bottle
  Serial.print("Measured Weight: ");
  Serial.println(weight);

  int timeCredit = 0;
  String resultMessage = "";

  lcd.clear();

  if (weight >= -0.00 || weight < 0.03 || weight >= 0.11) {
    timeCredit = 1;
    servo2.write(90);
    delay(3000);
    servo2.write(0);
    resultMessage = "Bottle Accepted! 1 Minute Granted";
  } else if (weight >= 0.01 && weight < 0.99) {
    timeCredit = 2;
    servo2.write(90);
    delay(3000);
    servo2.write(0);
    resultMessage = "Bottle Accepted! 2 Minutes Granted";
  } else if (weight >= 1.20 ) {
    timeCredit = 3;
    servo2.write(90);
    delay(3000);
    servo2.write(0);
    resultMessage = "Bottle Accepted! 3 Minutes Granted";
  } else {
    resultMessage = "Invalid Bottle. Try Again";
    lcd.setCursor(0, 0);
    lcd.print("Invalid Bottle");
    lcd.setCursor(0, 1);
    lcd.print("Try Again");
    digitalWrite(BUZZER_PIN, HIGH);
    delay(2000);
    digitalWrite(BUZZER_PIN, LOW);
    servo1.write(90);
    delay(3000);
    servo1.write(0);
  }

  String htmlResponse = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Bottle Vending Machine</title>
      <style>
        body { font-family: Arial, sans-serif; text-align: center; background: linear-gradient(to bottom, #4facfe, #00f2fe); color: white; margin: 0; padding: 0; }
        .container { max-width: 600px; margin: 50px auto; padding: 20px; background: rgba(255, 255, 255, 0.1); border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); }
        h1 { font-size: 2.5em; margin-bottom: 10px; }
        p { font-size: 1.2em; margin: 15px 0; }
        .button { display: inline-block; background: #28a745; color: white; padding: 10px 20px; text-decoration: none; border-radius: 5px; font-size: 1.2em; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.2); }
        .button:hover { background: #218838; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Result</h1>
        <p><b>Status:</b> %RESULT%</p>
        <p><a href="/" class="button">Go Back</a></p>
      </div>
    </body>
    </html>
  )rawliteral";

  htmlResponse.replace("%RESULT%", resultMessage);
  server.send(200, "text/html", htmlResponse);
  delay(5000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bottle Vending");
  lcd.setCursor(0, 1);
  lcd.print("Ready!");
}

void handleTime() {
  String clientIP = server.client().remoteIP().toString();
  int time = 0;

  for (int i = 0; i < deviceCount; i++) {
    if (deviceTimers[i].ip == clientIP) {
      time = deviceTimers[i].remainingTime;
      break;
    }
  }

  server.send(200, "text/plain", String(time));
}

void decrementTime() {
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate >= 1000) {
    for (int i = 0; i < deviceCount; i++) {
      if (deviceTimers[i].remainingTime > 0) {
        deviceTimers[i].remainingTime--;
        deviceTimers[i].dataUsed += 100; 
      }
    }
    lastUpdate = millis();
  }
}

void handleLogin() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Admin Login</title>
      <style>
        body { font-family: Arial, sans-serif; text-align: center; background: linear-gradient(to bottom, #4facfe, #00f2fe); color: white; }
        .container { max-width: 400px; margin: 50px auto; padding: 20px; background: rgba(255, 255, 255, 0.1); border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); }
        input { width: 80%; padding: 10px; margin: 10px 0; border-radius: 5px; border: none; }
        button { background: #28a745; color: white; padding: 10px 20px; border: none; border-radius: 5px; }
        button:hover { background: #218838; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Admin Login</h1>
        <form action="/auth" method="POST">
          <input type="text" name="username" placeholder="Username" required><br>
          <input type="password" name="password" placeholder="Password" required><br>
          <button type="submit">Login</button>
        </form>
      </div>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleAuth() {
  if (server.hasArg("username") && server.hasArg("password")) {
    String username = server.arg("username");
    String password = server.arg("password");

    if (username == adminUsername && password == adminPassword) {
      isAuthenticated = true;
      server.sendHeader("Location", "/admin");
      server.send(302, "text/plain", "Redirecting...");
      return;
    }
  }
  server.send(403, "text/plain", "Access Denied. Invalid credentials.");
}

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  Serial.println("LCD Initialized");

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo1.write(0);
  servo2.write(0);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  scale.begin(DT, SCK);
  scale.set_scale(calibration_factor);
  scale.tare();

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  WiFi.softAP(ssid);

  Serial.println("Access Point started:");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/insert", handleInsert);
  server.on("/time", handleTime);
  server.on("/admin", handleAdmin);

  server.begin();
  Serial.println("Web Server started");

  server.on("/login", handleLogin);
  server.on("/auth", HTTP_POST, handleAuth);
  server.on("/admin", handleAdmin);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bottle Vending");
  lcd.setCursor(0, 1);
  lcd.print("Ready!");
  Serial.println("LCD Ready");
}

void loop() {
  server.handleClient();
  decrementTime();
}
