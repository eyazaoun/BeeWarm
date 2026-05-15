#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include "BluetoothSerial.h"
#include <Adafruit_INA219.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

BluetoothSerial SerialBT;

// -------------------- Température (DS18B20) --------------------
#define ONE_WIRE_BUS 14
#define TEMP_SENSOR_PIN ONE_WIRE_BUS

OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);

DeviceAddress tempDeviceAddress;
// *** NOUVELLE ADRESSE CAPTEUR RUCHE ***
DeviceAddress capteurRuche = { 0x28, 0x61, 0x64, 0x08, 0xEF, 0xA0, 0x01, 0xD0 };

int numberOfDevices = 0;
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
const int pwm_freq = 1000;
const int pwm_res  = 8;

// -------------------- Paramètres contrôle --------------------
const float bande = 1.0;
static bool chauffe = false;

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

String formatTemp(float t) {
  if (t >= 0 && t < 10.0) {
    return "0" + String(t, 1);
  }
  return String(t, 1);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(500);

  ledcAttach(SIGNAL1, pwm_freq, pwm_res);
  ledcAttach(SIGNAL2, pwm_freq, pwm_res);
  ledcWrite(SIGNAL1, 0);
  ledcWrite(SIGNAL2, 0);

  delay(200);

  SerialBT.begin("BeeWarmESP32");

  sensors.begin();
  numberOfDevices = sensors.getDeviceCount();
  Serial.print("Capteurs DS18B20 détectés: ");
  Serial.println(numberOfDevices);

  for (int i = 0; i < numberOfDevices; i++) {
    if (sensors.getAddress(tempDeviceAddress, i)) {
      Serial.print("Capteur "); Serial.print(i); Serial.print(" : { ");
      for (uint8_t j = 0; j < 8; j++) {
        Serial.print("0x");
        if (tempDeviceAddress[j] < 16) Serial.print("0");
        Serial.print(tempDeviceAddress[j], HEX);
        if (j < 7) Serial.print(", ");
      }
      Serial.println(" }");
    }
  }

  if (!Wire.begin(SDA_PIN, SCL_PIN)) {
    Serial.println("E: I2C non initialise");
  } else {
    Serial.println("I: I2C initialise");
  }

  if (!ina219.begin()) {
    Serial.println("E: Failed to find INA219 chip");
    while (1) delay(10);
  }
  ina219.setCalibration_32V_2A();
  Serial.println("I: INA219 initialise (calibration 32V / 2A)");
}

void loop() {
  // ----- Réception consigne via Bluetooth -----
  if (SerialBT.available()) {
    String messageRecu = SerialBT.readString();
    messageRecu.trim();
    Serial.println(messageRecu);

    int newConsigne = messageRecu.substring(messageRecu.lastIndexOf('=') + 1).toInt();
    if (newConsigne > 0 && newConsigne < 60) {
      temperatureConsigne = newConsigne;
      Serial.print("Consigne modifiée : ");
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
        Serial.println("Capteur déconnecté !");
        continue;
      }

      if (memcmp(tempDeviceAddress, capteurRuche, 8) == 0) {
        tRuche = tempC;
      } else {
        tVeg += tempC;
        nbVeg++;
      }
    }
  }

  if (nbVeg > 0) tVeg = tVeg / nbVeg;

  String message = "DATA;" + formatTemp(tRuche) + ";" + formatTemp(tVeg);
  SerialBT.println(message);
  Serial.println(message);

  // ----- Lecture INA219 -----
  float shuntVoltage = ina219.getShuntVoltage_mV();
  float busVoltage   = ina219.getBusVoltage_V();
  float current_mA   = ina219.getCurrent_mA();
  float power_mW     = ina219.getPower_mW();

  Serial.println("-----------------------------------");
  Serial.print("Bus Voltage:   "); Serial.print(busVoltage);   Serial.println(" V");
  Serial.print("Shunt Voltage: "); Serial.print(shuntVoltage); Serial.println(" mV");
  Serial.print("Current:       "); Serial.print(current_mA);   Serial.println(" mA");
  Serial.print("Power:         "); Serial.print(power_mW);     Serial.println(" mW");
  Serial.println("-----------------------------------");

  // ----- Hystérésis température -----
  if (tRuche < (temperatureConsigne - bande)) {
    chauffe = true;
  } else if (tRuche > (temperatureConsigne + bande)) {
    chauffe = false;
  }

  // ----- MPPT simplifié (P&O) -----
  static float alpha     = 0.5;
  static float delta     = 0.02;
  static float P_old     = 0;
  static float V_old     = 0;
  static int   direction = 1;

  float P  = power_mW;
  float V  = busVoltage;
  float dP = P - P_old;
  float dV = V - V_old;

  if (dP > 0) {
    direction = (dV > 0) ? -1 : 1;
  } else if (dP < 0) {
    direction = (dV > 0) ? 1 : -1;
  }

  if (abs(dP) > 50 && abs(dV) > 0.01) {
    alpha += direction * delta;
  }

  alpha = constrain(alpha, 0.05, 0.95);
  int duty = (int)(alpha * 255);

  if (chauffe) {
    ledcWrite(SIGNAL1, duty);
    ledcWrite(SIGNAL2, duty);
  } else {
    ledcWrite(SIGNAL1, 0);
    ledcWrite(SIGNAL2, 0);
  }

  P_old = P;
  V_old = V;

  delay(3000);
}