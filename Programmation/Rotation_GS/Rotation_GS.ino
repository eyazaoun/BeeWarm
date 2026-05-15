#include <Servo.h>
Servo myServo;
int RotationServo = 90;

int PhotoValue = 0;
int myPin = A0;
float VCCdiv2 = 2.5;
float epsilon = 0.15;

void setup()
{
  Serial.begin(9600);
  myServo.attach(8);
  myServo.write(RotationServo);
  delay(500);
}

void loop()
{
  PhotoValue = analogRead(myPin);
  PhotoValue = (PhotoValue*5)/1023; // [0V ; 5V] <=> [0 ; 1023]
  Serial.print("Valeur : ");
  Serial.println(PhotoValue);
  
  if (PhotoValue > VCCdiv2 + epsilon){
    while (analogRead(myPin) > VCCdiv2 + epsilon){
      myServo.write(RotationServo++);
      delay(50);
    }
  }
  else if (PhotoValue < VCCdiv2 - epsilon){
    while (analogRead(myPin) < VCCdiv2 - epsilon){
      myServo.write(RotationServo--);
      delay(50);
    }
  }
  
  delay(500);
}