#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include "BluetoothSerial.h"
#include <Adafruit_INA219.h>

BluetoothSerial SerialBT;

// -------------------- Température (DS18B20) --------------------
#define ONE_WIRE_BUS 14
#define TEMP_SENSOR_PIN ONE_WIRE_BUS

OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);

DeviceAddress tempDeviceAddress;
DeviceAddress capteurRuche = { 0x28, 0x61, 0x64, 0x08, 0xEB, 0xE3, 0x98, 0x0D };

int numberOfDevices = 0;
int numberOfCapteurRuche = 1;

float tVeg = 0.0;
float tRuche = 0.0;
int temperatureConsigne = 33;

// -------------------- INA219 (I2C) --------------------
Adafruit_INA219 ina219;

#define SDA_PIN 21
#define SCL_PIN 22

// -------------------- Chauffage (PWM -> MOSFET) --------------------
#define SIGNAL1 2
#define SIGNAL2 5

const int pwm_freq = 1000; // Hz
const int pwm_res  = 8;    // 8 bits => 0..255

// -------------------- Paramètres contrôle --------------------
const float bande = 1.0;   // hystérésis ±1°C
static bool chauffe = false;

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Bluetooth
  SerialBT.begin("BeeWarmESP32");

  // PWM (LEDC)
  ledcAttach(SIGNAL1, pwm_freq, pwm_res);
  ledcAttach(SIGNAL2, pwm_freq, pwm_res);

  ledcWrite(SIGNAL1, 0);
  ledcWrite(SIGNAL2, 0);

  // Temp sensors
  sensors.begin();
  numberOfDevices = sensors.getDeviceCount();
  Serial.print("Nombre de capteurs DS18B20 detectes: ");
  Serial.println(numberOfDevices);

  for (int i = 0; i < numberOfDevices; i++) {
    if (sensors.getAddress(tempDeviceAddress, i)) {
      Serial.print("SENSOR n° ");
      Serial.print(i);
      Serial.print(" adresse: ");
      printAddress(tempDeviceAddress);
      Serial.println();
    } else {
      Serial.print("Ghost device at ");
      Serial.print(i);
      Serial.println(" (adresse non lue).");
    }
  }

  // I2C
  if (!Wire.begin(SDA_PIN, SCL_PIN)) {
    Serial.println("E: I2C non initialise");
  } else {
    Serial.println("I: I2C initialise");
  }

  // INA219
  if (!ina219.begin()) {
    Serial.println("E: Failed to find INA219 chip");
    while (1) delay(10);
  }
  ina219.setCalibration_32V_2A();
  Serial.println("I: INA219 initialise (calibration 32V / 2A)");
}

void loop() {
  // ----- Réception consigne via Bluetooth (ex: "TEMP=33") -----
  if (SerialBT.available()) {
    String messageRecu = SerialBT.readString();
    messageRecu.trim();
    Serial.println(messageRecu);

    int newConsigne = messageRecu.substring(messageRecu.lastIndexOf('=') + 1).toInt();
    if (newConsigne > 0 && newConsigne < 60) { // garde-fou simple
      temperatureConsigne = newConsigne;
      Serial.print("Température de consigne modifiée : ");
      Serial.print(temperatureConsigne);
      Serial.println(" °C");
    }
  }

  // ----- Lecture températures -----
  sensors.requestTemperatures();

  tVeg = 0.0;
  tRuche = 0.0;
  int nbVeg = 0;

  for (int i = 0; i < numberOfDevices; i++) {
    if (sensors.getAddress(tempDeviceAddress, i)) {
      float tempC = sensors.getTempC(tempDeviceAddress);

      if (tempC == DEVICE_DISCONNECTED_C) {
        Serial.println("Capteur de temp deconnecte !");
        continue;
      }

      if (memcmp(tempDeviceAddress, capteurRuche, 8) == 0) {
        tRuche = tempC; // 1 capteur ruche attendu
      } else {
        tVeg += tempC;
        nbVeg++;
      }
    }
  }

  if (nbVeg > 0) tVeg = tVeg / nbVeg;

  String message = "DATA;" + String(tRuche, 1) + "°C;" + String(tVeg, 1) + "°C";
  SerialBT.println(message);
  Serial.println(message);

  // ----- Lecture INA219 -----
  float shuntVoltage = ina219.getShuntVoltage_mV();
  float busVoltage   = ina219.getBusVoltage_V();
  float current_mA   = ina219.getCurrent_mA();
  float power_mW     = ina219.getPower_mW();

  Serial.println("-----------------------------------");
  Serial.print("Bus Voltage:   "); Serial.print(busVoltage);   
  Serial.println(" V");
  Serial.print("Shunt Voltage: "); Serial.print(shuntVoltage); 
  Serial.println(" mV");
  Serial.print("Current:       "); Serial.print(current_mA);   
  Serial.println(" mA");
  Serial.print("Power:         "); Serial.print(power_mW);     
  Serial.println(" mW");
  Serial.println("-----------------------------------");

  // ----- Hystérésis température -----
  if (tRuche < (temperatureConsigne - bande)) {
    chauffe = true;
  } else if (tRuche > (temperatureConsigne + bande)) {
    chauffe = false;
  }

  static float alpha = 0.5;
  static float delta = 0.02;
  static float P_old = 0;
  static float V_old = 0;
  static int direction = 1;

  float P = power_mW;
  float V = busVoltage;
  float dP = P - P_old;
  float dV = V - V_old;

  if (dP > 0) {
    if (dV > 0) {
        direction = -1;
    } 
    else if (dV < 0) {
        direction = 1;
    }
  }
  else if (dP < 0) {
    if (dV > 0) {
        direction = 1;
    } 
    else if (dV < 0) {
        direction = -1;
    }
  }

  float seuil_dV = 0.01; // 10 mV
  float seuil_dP = 50;   // 50 mW

  if (abs(dP) > seuil_dP && abs(dV) > seuil_dV) {
    // mettre à jour alpha
    alpha += direction * delta;
  }

  if (alpha > 0.95) alpha = 0.95;
  if (alpha < 0.05) alpha = 0.05;

  int duty = alpha * 255;

  if (chauffe) {
    ledcWrite(SIGNAL1, duty);
    ledcWrite(SIGNAL2, duty);
  } 
  else {
    ledcWrite(SIGNAL1, 0);
    ledcWrite(SIGNAL2, 0);
  }

  P_old = P;
  V_old = V;

  delay(3000);
}