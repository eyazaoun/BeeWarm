#include <Adafruit_INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

float tVeg = 30.0;
float tRuche = 30.0;

void setup() {
  delay(200);
  SerialBT.begin("BeeWarmESP32");           // nom de l'esp32 visible sur le smartphone
  Serial.begin(115200);
  delay(200);
}

void loop() {
  tVeg += random(-5, 6)*0.1;
  tRuche += random(-5, 6)*0.1; // [-5;6[ x 0,1 => simuler des variations entre [-0,5°C;0,5°C]
  if (tVeg < 25) tVeg = 25; // T min végétaline
  if (tVeg > 45) tVeg = 45; // T max végétaline
  if (tRuche < 25) tRuche = 25; // T min ruche
  if (tRuche > 35) tRuche = 35; // T max ruche

  // conversion float à string avec ##,#
  String message = "DATA;" + String(tVeg, 1) + "°C;" + String(tRuche, 1) + "°C";
  SerialBT.println(message); // envoi du message à l'ESP32
  Serial.println(message); // affichage dans la console arduino
  delay(3000);
}
