#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <WiFi.h>
#include <ArduinoUniqueID.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <HTTPClient.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DHTPIN 26
#define VOC_PIN 25
#define DHTTYPE DHT11
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT_Unified dht(DHTPIN, DHTTYPE);

int adcResolution = 4095;
float vRef = 5.0;

const char* ssid = ""; // Add your ssid
const char* password = ""; // add your pw to wifi
String serialNumber = "";
String serverUrl = "http://192.168.86.225:8080/sensorData/log"; //change to your servers ip

void clearDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
}

float adcToVoltage(int adcValue) {
  return ((float)adcValue / adcResolution) * vRef;
}

float calculateToluenePPM(float voltage) {
  return pow(10, (-3.478 + 1.104 * voltage));
}

float calculateFormaldehyde(float voltage){
  return pow(10, (-1.095 + 0.627 * voltage));
}

float calculateVOCs(float voltage) {
  return pow(10, 1.5 * voltage);
}

void connectToWifi() {
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

String getSerial() {
  String uuid = "";
  for (size_t i = 0; i < UniqueIDsize; i++) {
    uuid += String(UniqueID[i], HEX);
  }
  return uuid;
}

void postData(float vocCount, float temperature, float humidity, float toluenePpm, float formaldehydePpm) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String payload = "{\"vocCount\":" + String(vocCount, 2) + 
                     ",\"temperature\":" + String(temperature, 2) + 
                     ",\"humidity\":" + String(humidity, 2) + 
                     ",\"toluenePpm\":" + String(toluenePpm, 2) + 
                     ",\"formaldehydePpm\":" + String(formaldehydePpm, 2) + 
                     ",\"device\":{\"deviceId\":\"" + serialNumber + "\"}}";

    http.begin(serverUrl); 
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      String response = http.getString();
    } else {
      Serial.println("Error on sending POST: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected. Reconnecting...");
  }
}

void setup() {
  dht.begin();
  Serial.begin(115200);
  pinMode(VOC_PIN, INPUT);
  serialNumber = getSerial();

  Wire.begin(5, 4);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  connectToWifi();
}

void loop() {
  WiFi.disconnect(true);
  delay(500);
  sensors_event_t event;

  int gasAnalogValue = analogRead(VOC_PIN);
  
  float voltage = adcToVoltage(gasAnalogValue);
  float vocCount = calculateVOCs(voltage);
  float toluenePpm = calculateToluenePPM(voltage);
  float formaldehydePpm = calculateFormaldehyde(voltage);
  
  dht.temperature().getEvent(&event);
  float temperature = ((1.8 * event.temperature) + 32);
  
  dht.humidity().getEvent(&event);
  float humidity = event.relative_humidity;

  Serial.print("VOCs: ");
  Serial.println(vocCount);
  Serial.print("Temperature: ");
  Serial.println(temperature);
  Serial.print("Humidity: ");
  Serial.println(humidity);
  Serial.print("Toluene PPM: ");
  Serial.println(toluenePpm);
  Serial.print("Formaldehyde PPM: ");
  Serial.println(formaldehydePpm);

  clearDisplay();
  display.println("PhCH3:");
  display.print(toluenePpm);
  display.println(" ppm");

  display.println("CH2O:");
  display.print(formaldehydePpm);
  display.println(" ppm");
  
  display.display();
  
  connectToWifi();

  while(WiFi.status()!= WL_CONNECTED) {
    delay(1000);
    Serial.print("#");
  }
  Serial.println();

  postData(vocCount, temperature, humidity, toluenePpm, formaldehydePpm);

  esp_sleep_enable_timer_wakeup(60000000); // One minute
  esp_deep_sleep_start();
}
