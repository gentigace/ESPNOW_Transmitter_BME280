/*
  Created by: Genti Gace & ALBintech
  
  Website: http://albintech.com
  No permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Adafruit_BME280.h>
#include <esp_sleep.h>
#include <Wire.h>
#include <BH1750.h>
#include <SPI.h>

// Sleep for 1 hours = 1 * 60 * 60 = 3600 seconds
//uint64_t sleepTimeSeconds = 21600;

// Pin Definitions
#define I2C_SDA     25
#define I2C_SCL     26
#define BAT_ADC     33
#define SALT_PIN    34
#define SOIL_PIN    32
#define BOOT_PIN    0
#define POWER_CTRL  4
#define USER_BUTTON 35

// Sensor Types and Constants
#define BME280_ADDR 0x77  // I2C address for BME280

int luxRead;

// Broadcast Address (Receiver MAC Address)
uint8_t broadcastAddress[] = {0xB4, 0x8A, 0x0A, 0x82, 0x3D, 0x08}; 

// Structure to store sensor data
typedef struct gnhc_data_struct {
  int id;  // Transmitter ID
  int t;   // Temperature
  int h;   // Humidity
  int s;   // Soil Moisture
  int l;   // Lux (light level)
} gnhc_data_struct;

gnhc_data_struct gdata;  // Instance of gnhc_data_struct to store data

// Sensor objects
BH1750 lightMeter(0x23);  // I2C address for BH1750 Light Meter
Adafruit_BME280 bme;      // BME280 Sensor object

bool deviceConnected = false;  // Device connection status

// Function to send data using ESP-NOW
bool sendData(gnhc_data_struct data) {
  esp_err_t result = esp_now_send(0, (uint8_t*)&data, sizeof(gnhc_data_struct));

  if (result == ESP_OK) {
    Serial.println("Sent with success");
    return true;
  } else {
    Serial.println("Error sending the data");
    return false;
  }
}

// Callback function for data sent confirmation
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  Serial.print("Packet to: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", 
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(" send status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Function to read battery
float readBattery()
{
  int vref = 1100;
  uint16_t volt = analogRead(BAT_ADC);
  // Serial.print("Volt direct ");
  // Serial.println(volt);
  float battery_voltage = ((float)volt / 4095.0) * 2.0 * 3.3 * (vref) / 1000;
  Serial.print("Battery Voltage: ");
  Serial.println(battery_voltage);
  battery_voltage = battery_voltage * 100;
  return map(battery_voltage, 416, 290, 100, 0);
}

// Function to read soil moisture
uint16_t readSoil() {
  uint16_t soil = analogRead(SOIL_PIN);
  uint16_t dryValue = 3490;  // Calibration for dry soil
  uint16_t humidValue = 1160;  // Calibration for wet soil
  uint16_t mappedSoil = map(soil, dryValue, humidValue, 0, 100);
  return mappedSoil;
}

// Setup function to initialize sensors and ESP-NOW
void setup() {
  Serial.begin(115200);
  
  // Initialize Wi-Fi mode as Station and set the channel
  WiFi.mode(WIFI_STA);

  // Power on the BME280 sensor
  pinMode(POWER_CTRL, OUTPUT);
  digitalWrite(POWER_CTRL, HIGH);  // Power the sensor ON
  delay(200);  // Wait for the sensor to power up

  // Initialize I2C bus
  bool wireOk = Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println(wireOk ? F("Wire ok") : F("Wire NOK"));

  // Initialize the BME280 sensor
  if (!bme.begin(BME280_ADDR)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);  // Infinite loop if sensor is not found
  }

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Register send callback for ESP-NOW
  esp_now_register_send_cb(OnDataSent);

  // Add peer for communication
  esp_now_peer_info_t peerInfo;
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Attempt to initialize the BH1750 light meter
  bool lightMeterOk = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println(lightMeterOk ? F("BH1750 Advanced begin") : F("Error initializing BH1750"));

  // Display battery voltage
  Serial.print("Battery level: ");
  Serial.println(readBattery());
}

// Main loop to read sensors and send data
void loop() {
  // Read Light Level
  luxRead = lightMeter.readLightLevel();
  Serial.print("Lux: "); 
  Serial.println(luxRead);
  gdata.l = luxRead;

  // Read Temperature and Humidity from BME280
  gdata.t = bme.readTemperature();
  gdata.h = bme.readHumidity();

  Serial.print("Temperature: ");
  Serial.print(gdata.t);
  Serial.println("*C");

  Serial.print("Humidity: ");
  Serial.print(gdata.h);
  Serial.println("%");

  // Read Soil Moisture
  uint16_t soil = readSoil();
  gdata.s = soil;
  Serial.print("Soil Moisture: ");
  Serial.print(soil);
  Serial.println("%");

  // Send Data
  gdata.id = 1;  // Transmitter ID
  if (sendData(gdata)) {
    Serial.println("Data sent successfully.");
  }

  // Add delay before sending data
  delay(5000);

  // Configure the timer to wake up the ESP32
  //Serial.println("Entering deep sleep...");
  //esp_sleep_enable_timer_wakeup(sleepTimeSeconds * 1000000); /* Sleep until the next measurement */
  // Put the ESP32 into deep sleep mode
  //esp_deep_sleep_start();
}