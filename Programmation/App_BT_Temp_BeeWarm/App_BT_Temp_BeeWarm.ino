#include <OneWire.h>
#include <DallasTemperature.h>
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

#define ONE_WIRE_BUS 14   // Broche du DS18B20 (DATA_temp)
#define TEMP_SENSOR_PIN ONE_WIRE_BUS

DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address

float tVeg = 0.0;
float tRuche = 0.0;
float temperatureConsigne=33.0;

// Initialisation du bus OneWire et du capteur
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);
int numberOfDevices; // Number of temperature devices found
int numberOfCapteurRuche = 1;

DeviceAddress capteurRuche = { 0x28, 0x61, 0x64, 0x08, 0xEB, 0xE3, 0x98, 0x0D };

void setup() {
  delay(200);
  SerialBT.begin("BeeWarmESP32");  // nom de l'esp32 visible sur le smartphone
  Serial.begin(115200);
  delay(200);

  // Start up the library for temp sensor
  sensors.begin();
  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  // Loop through each device, print out address
  for(int i=0;i<numberOfDevices; i++) {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)) {
      Serial.print("SENSOR found n° ");
      Serial.print(i, DEC);
      Serial.println();
      printAddress(tempDeviceAddress);
      Serial.println();
		}
    else {
		  Serial.print("Ghost device at ");
		  Serial.print(i, DEC);
		  Serial.println(" but could not detect address. Check power and cabling...");
		}
  }
  //end temp config
  delay(200);
}

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}


void loop() {
  // Lire la valeur du capteur de temperature
  sensors.requestTemperatures(); // Send the command to get temperatures
  // Loop through each device, print out temperature data
  tVeg = 0.0;
  tRuche = 0.0;
  for(int i=0;i<numberOfDevices; i++) {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)){
      Serial.print("Capteur: ");
      Serial.println(i,DEC);
      float tempC = sensors.getTempC(tempDeviceAddress);
      if (tempC == DEVICE_DISCONNECTED_C) {
        Serial.println("Capteur de temp deconnecte !");
        return;
      }
      if (memcmp(tempDeviceAddress, capteurRuche, 8) == 0) {
        tRuche = tRuche + tempC;
      }
      else {
        tVeg = tVeg + tempC;
      }
      Serial.print(tempC);
      Serial.println("°C");
    } 	
  }
  tVeg = tVeg/(numberOfDevices-numberOfCapteurRuche);
  
  String message = "DATA;" + String(tRuche, 1) + "°C;" + String(tVeg, 1) + "°C"; // conversion float à string avec ##,#
  SerialBT.println(message); // envoie du message
  Serial.println();
  Serial.println(message);
  delay(3000);
}
