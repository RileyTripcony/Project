#include <WiFiNINA.h>
#include "Secret.h"
#include <BH1750.h>
#include <Wire.h>
#include "DHT.h"
#include "ThingSpeak.h"

// DHT sensor settings
#define DHTPIN 2     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11

// 5V Relay settings
#define RELAY_PIN 4  // Relay connected to digital pin 4

// Initialize the DHT and BH1750 sensors
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;

// WiFi credentials
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASSWORD;

WiFiClient client;

// ThingSpeak details
unsigned long myChannelNumber = SECRET_CH_ID;
const char* myWriteAPIKey = SECRET_WRITE_APIKEY;

// Webhook details
char HOST_NAME[] = "maker.ifttt.com";
String SUFFICIENT_LIGHT_PATH = "/trigger/sunlight_received/with/key/cuXtFESE6lJYSaWqt3OMMW";
String INSUFFICIENT_LIGHT_PATH = "/trigger/insufficient_light/with/key/cuXtFESE6lJYSaWqt3OMMW";
String queryString = "?value1=57&value2=25";

bool emailSent = false;

// Timing variables
unsigned long lastMoistureCheckTime = 0;
const unsigned long moistureCheckInterval = 600000; // 10 minutes in milliseconds
const unsigned long pumpRunTime = 10000; // 10 seconds in milliseconds

unsigned long lastPrintTime = 0;
const unsigned long printInterval = 30000; // 30 seconds in milliseconds

unsigned long lastThingSpeakUpdateTime = 0;
const unsigned long thingSpeakUpdateInterval = 60000; // 60 seconds in milliseconds

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println(F("PlantMonitor!"));

  // Initialize the I2C bus
  Wire.begin();

  // Initialize sensors
  lightMeter.begin();
  dht.begin();

  // Initialize WiFi connection
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to WiFi");

  // Initialize ThingSpeak
  ThingSpeak.begin(client);

  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is off initially

  checkMoistureSensor();
}

void loop() {
  unsigned long currentTime = millis();

  // Check moisture sensor every 10 minutes
  if (currentTime - lastMoistureCheckTime >= moistureCheckInterval) {
    checkMoistureSensor();
    lastMoistureCheckTime = currentTime;
  }

  // Print sensor values every 30 seconds
  if (currentTime - lastPrintTime >= printInterval) {
    printSensorValues();
    lastPrintTime = currentTime;
  }

  // Update ThingSpeak every 60 seconds
  if (currentTime - lastThingSpeakUpdateTime >= thingSpeakUpdateInterval) {
    updateThingSpeak();
    lastThingSpeakUpdateTime = currentTime;
  }

  // Check light level and send email if necessary
  checkLightLevel();

}

void checkMoistureSensor() {
  // Read moisture sensor value
  int moistureValue = analogRead(A0);
  Serial.print("Moisture Sensor Value: ");
  Serial.println(moistureValue);

  // Check if soil is dry (value < 300)
  if (moistureValue < 300) {
    Serial.println("Soil is dry. Activating relay...");
    digitalWrite(RELAY_PIN, HIGH); // Turn on relay
    delay(pumpRunTime);            // Run pump for 20 seconds
    digitalWrite(RELAY_PIN, LOW);  // Turn off relay
    Serial.println("Relay deactivated after 20 seconds.");
  } else {
    Serial.println("Soil is sufficiently moist.");
  }
}

void printSensorValues() {
  //-------------------------LIGHT SENSOR-------------------------
  float lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");

  //---------------TEMPERATURE AND HUMIDITY SENSOR---------------
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.println("%");
  
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println("°C");
  //-------------------------MOISTURE SENSOR-------------------------
  //Connect the sensor to the A0(Analog 0) pin on the Arduino board
  // the sensor value description
  // 0  ~300     dry soil
  // 300~700     humid soil
  // 700~950     in water
  int moistureValue = analogRead(A0);
  Serial.print("Moisture Sensor Value: ");
  Serial.println(moistureValue);
  //-----------------------------------------------------------------
}

void updateThingSpeak() {
  // Read current sensor values
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float lux = lightMeter.readLightLevel();
  int moistureValue = analogRead(A0);

  // Update ThingSpeak fields
  ThingSpeak.setField(1, t); // Temperature
  ThingSpeak.setField(2, h); // Humidity
  ThingSpeak.setField(3, lux); // Light
  ThingSpeak.setField(4, moistureValue); // Moisture

  // Write to ThingSpeak
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200) {
    Serial.println("ThingSpeak update successful.");
  } else {
    Serial.println("Problem updating ThingSpeak. HTTP error code " + String(x));
  }
}

void checkLightLevel() {
  float lux = lightMeter.readLightLevel();

  if (lux < 100 && !emailSent) { // If light is insufficient and email not sent yet
    sendEmail(INSUFFICIENT_LIGHT_PATH);
    emailSent = true;
  } else if (lux >= 100 && emailSent) { // If light is sufficient and email was sent
    sendEmail(SUFFICIENT_LIGHT_PATH);
    emailSent = false;
  }
}

void sendEmail(String path) {
  if (client.connect(HOST_NAME, 80)) {
    Serial.println("Connected to server");

    client.print("GET ");
    client.print(path);
    client.print(queryString);
    client.println(" HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("Connection: close");
    client.println();

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.print(c);
      }
    }

    client.stop();
    Serial.println();
    Serial.println("Disconnected");
  } else {
    Serial.println("Connection to server failed");
  }
}






