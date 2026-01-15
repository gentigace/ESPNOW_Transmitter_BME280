/*
  Created by: Genti Gace & ALBintech

  Website: http://albintech.com
  No permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

// --- Include Required Libraries ---
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <Wire.h>
#include <BH1750.h>
#include <SPI.h>

// ✅ BME280
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

// --- Optional Deep Sleep Time (in seconds) ---
//uint64_t sleepTimeSeconds = 21600; // 6 hours

// --- GPIO Pin Definitions ---
#define I2C_SDA     25
#define I2C_SCL     26
#define BAT_ADC     33
#define SOIL_PIN    32
#define BOOT_PIN     0
#define POWER_CTRL   4
#define USER_BUTTON 35

// --- Constants ---
#define SOIL_DRY 3490
#define SOIL_WET 1160

int luxRead;

// --- Receiver MAC Address (ESP-NOW peer) ---
uint8_t broadcastAddress[] = {0xB4, 0x8A, 0x0A, 0x82, 0x3D, 0x08};

// --- Data Structure for ESP-NOW Transmission ---
typedef struct gnhc_data_struct {
  int id;    // Device ID
  int t;     // Temperature (°C)
  int h;     // Humidity (%)
  int s;     // Soil moisture (0–100%)
  int l;     // Light (lux)
} gnhc_data_struct;

gnhc_data_struct gdata;

// --- Sensor Object Declarations ---
BH1750 lightMeter(0x23);

// ✅ BME280 object
Adafruit_BME280 bme;

// 3) Send to your peer explicitly (safer than 0 / broadcast)
bool sendData(gnhc_data_struct data) {
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&data, sizeof(gnhc_data_struct));
  Serial.println(result == ESP_OK ? "Sent with success" : "Error sending the data");
  return result == ESP_OK;
}

// --- ESP-NOW Send Callback ---
void OnDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  Serial.printf("Packet send status: %s\n",
                status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// --- Setup Function ---
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  // Setup peer device for communication
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Power on the sensor rail
  pinMode(POWER_CTRL, OUTPUT);
  digitalWrite(POWER_CTRL, 1);
  delay(1000);

  // Initialize I2C communication
  bool wireOk = Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println(wireOk ? "Wire ok" : "Wire NOK");

  // Initialize BH1750
  bool lightMeterOk = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println(lightMeterOk ? "BH1750 initialized" : "Error initializing BH1750");

  // ✅ Initialize BME280 (try 0x76 first, then 0x77)
  bool bmeOk = bme.begin(0x76);
  if (!bmeOk) bmeOk = bme.begin(0x77);

  Serial.println(bmeOk ? "BME280 initialized" : "Error initializing BME280 (check wiring/address)");
}

// --- Main Loop ---
void loop() {
  // Read light intensity
  luxRead = (int)lightMeter.readLightLevel();
  gdata.l = luxRead;
  Serial.print("Lux: "); Serial.println(luxRead);

  // ✅ Read BME280 values
  float tempC = bme.readTemperature();        // °C
  float hum   = bme.readHumidity();           // %
  float pres  = bme.readPressure() / 100.0F;  // hPa

  if (!isnan(tempC)) {
    gdata.t = (int)tempC; // keeping your struct as int
    Serial.print("Temp (BME): "); Serial.println(tempC);
  } else {
    Serial.println("Temp (BME): NaN");
  }

  if (!isnan(hum)) {
    gdata.h = (int)hum;   // keeping your struct as int
    Serial.print("Humidity (BME): "); Serial.println(hum);
  } else {
    Serial.println("Humidity (BME): NaN");
  }

  // Pressure is NOT sent (no field in struct), but printed for debugging
  if (!isnan(pres)) {
    Serial.print("Pressure (BME): "); Serial.print(pres); Serial.println(" hPa");
  } else {
    Serial.println("Pressure (BME): NaN");
  }

  // Soil moisture
  uint16_t soil = analogRead(SOIL_PIN);
  gdata.s = map(soil, SOIL_DRY, SOIL_WET, 0, 100);
  Serial.print("Soil Moisture: "); Serial.println(gdata.s);

  // Device ID
  gdata.id = 3;

  // Send via ESP-NOW
  sendData(gdata);

  delay(5000);

  // Deep sleep
  Serial.println("Entering deep sleep...");
  //esp_sleep_enable_timer_wakeup(sleepTimeSeconds * 1000000ULL);
  //esp_deep_sleep_start();
}
