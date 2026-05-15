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
const int ch1 = 0;         // canal PWM pour SIGNAL1
const int ch2 = 1;         // canal PWM pour SIGNAL2

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
  ledcSetup(ch1, pwm_freq, pwm_res);
  ledcAttachPin(SIGNAL1, ch1);

  ledcSetup(ch2, pwm_freq, pwm_res);
  ledcAttachPin(SIGNAL2, ch2);

  ledcWrite(ch1, 0);
  ledcWrite(ch2, 0);

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

  Serial.print("Bus Voltage:   "); Serial.print(busVoltage);   Serial.println(" V");
  Serial.print("Shunt Voltage: "); Serial.print(shuntVoltage); Serial.println(" mV");
  Serial.print("Current:       "); Serial.print(current_mA);   Serial.println(" mA");
  Serial.print("Power:         "); Serial.print(power_mW);     Serial.println(" mW");
  Serial.println("-----------------------------------");

  // ----- Calcul puissance PWM (limitation énergétique) -----
  const int MPPT_mW = 4000; // puissance max dispo estimée (à ajuster après tests PCB)
  int puissanceDeChauffe = (int)((power_mW * 255.0) / MPPT_mW);

  if (puissanceDeChauffe < 0) puissanceDeChauffe = 0;
  if (puissanceDeChauffe > 255) puissanceDeChauffe = 255;

  Serial.print("PWM base (0-255) calcule: ");
  Serial.println(puissanceDeChauffe);

  // ----- Hystérésis température -----
  if (tRuche < (temperatureConsigne - bande)) {
    chauffe = true;
  } else if (tRuche > (temperatureConsigne + bande)) {
    chauffe = false;
  }

  // ----- Application PWM -----
  int duty = chauffe ? puissanceDeChauffe : 0;
  ledcWrite(ch1, duty);
  ledcWrite(ch2, duty);

  Serial.print("Etat chauffe: ");
  Serial.print(chauffe ? "ON" : "OFF");
  Serial.print(" | duty=");
  Serial.println(duty);

  delay(3000);
}
