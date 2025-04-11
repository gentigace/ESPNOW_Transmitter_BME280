/*
  Created by: Genti Gace & ALBintech
  
  Website: http://albintech.com
  No permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

// Required Libraries
#include <esp_now.h>                // ESP-NOW communication
#include <WiFi.h>                  // WiFi functionality
#include <esp_wifi.h>              // ESP32 WiFi driver
#include <Wire.h>                  // I2C communication
#include <Adafruit_BME280.h>       // BME280 temperature, humidity, and pressure sensor
#include <esp_sleep.h>             // Deep sleep functionality
#include <BH1750.h>                // BH1750 light (lux) sensor
#include <SPI.h>                   // SPI support (included in case SD is added later)

// --- Pin Definitions ---
#define I2C_SDA     25             // I2C SDA pin
#define I2C_SCL     26             // I2C SCL pin
#define BAT_ADC     33             // Battery voltage analog pin (not used in this code)
#define SOIL_PIN    32             // Soil moisture analog input pin
#define BOOT_PIN     0             // Boot button pin (optional)
#define POWER_CTRL   4             // Power control pin (e.g., for sensor VCC line)
#define USER_BUTTON 35             // Optional user button input

// --- Calibration Values ---
#define SOIL_DRY 3490              // Raw ADC value for dry soil
#define SOIL_WET 1160              // Raw ADC value for wet soil
#define BME280_ADDRESS 0x77        // I2C address for BME280 sensor (can be 0x76 or 0x77)

// --- Global Variables ---
int luxRead;                       // Lux value from BH1750

// MAC address of the receiver ESP32
uint8_t broadcastAddress[] = {0xB4, 0x8A, 0x0A, 0x82, 0x3D, 0x08};

// --- Data Structure for Sending Sensor Data via ESP-NOW ---
typedef struct gnhc_data_struct {
  int id;   // Device ID
  int t;    // Temperature (°C)
  int h;    // Humidity (%)
  int s;    // Soil moisture (%)
  int l;    // Light (lux)
} gnhc_data_struct;

gnhc_data_struct gdata;            // Data instance to populate and send

// Sensor objects
BH1750 lightMeter(0x23);           // BH1750 light sensor at I2C address 0x23
Adafruit_BME280 bme;               // BME280 sensor object

// --- Function to Send Data via ESP-NOW ---
bool sendData(gnhc_data_struct data) {
  esp_err_t result = esp_now_send(0, (uint8_t*)&data, sizeof(gnhc_data_struct));
  Serial.println(result == ESP_OK ? "Sent with success" : "Error sending the data");
  return result == ESP_OK;
}

// --- Callback After ESP-NOW Packet is Sent ---
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.printf("Packet to: %s send status: %s\n", macStr,
                status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// --- Setup ---
void setup() {
  Serial.begin(115200);                         // Start serial monitor
  WiFi.mode(WIFI_STA);                          // Set WiFi to station mode

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callback for send status
  esp_now_register_send_cb(OnDataSent);

  // Add receiver peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Power up sensors (if POWER_CTRL controls sensor power)
  pinMode(POWER_CTRL, OUTPUT);
  digitalWrite(POWER_CTRL, 1);
  delay(1000); // Wait for sensors to power up

  // Initialize I2C
  bool wireOk = Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println(wireOk ? "Wire ok" : "Wire NOK");

  // Initialize BH1750 Light Sensor
  bool lightMeterOk = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println(lightMeterOk ? "BH1750 initialized" : "Error initializing BH1750");

  // Initialize BME280 Sensor
  if (!bme.begin(BME280_ADDRESS)) {
    Serial.println("Could not find BME280 sensor!");
  } else {
    Serial.println("BME280 initialized");
  }
}

// --- Main Loop ---
void loop() {
  // Read light level (lux)
  luxRead = lightMeter.readLightLevel();
  gdata.l = luxRead;
  Serial.print("Lux: "); Serial.println(luxRead);

  // Read temperature from BME280
  float temp = bme.readTemperature();
  if (!isnan(temp)) {
    gdata.t = temp;
    Serial.print("Temp: "); Serial.println(temp);
  }

  // Read humidity from BME280
  float hum = bme.readHumidity();
  if (!isnan(hum)) {
    gdata.h = hum;
    Serial.print("Humidity: "); Serial.println(hum);
  }

  // Read soil moisture and map it to 0–100%
  uint16_t soil = analogRead(SOIL_PIN);
  gdata.s = map(soil, SOIL_DRY, SOIL_WET, 0, 100);
  Serial.print("Soil Moisture: "); Serial.println(gdata.s);

  // Set unique device ID
  gdata.id = 3;

  // Send data via ESP-NOW
  sendData(gdata);

  delay(5000); // Wait 5 seconds before next transmission

  // Optional: Enter deep sleep (uncomment to use)
  /*
  Serial.println("Entering deep sleep...");
  esp_sleep_enable_timer_wakeup(sleepTimeSeconds * 1000000);  // sleepTimeSeconds must be defined
  esp_deep_sleep_start();
  */
}
