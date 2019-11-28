#include <Arduino.h>
#include <Wire.h>
int incomingByte = 0;
void setup() {
  Serial.begin(9600);
  pinMode(13,OUTPUT);
  Serial.setTimeout(60000);
  Wire.begin(4);                // join i2c bus with address #4
  
}
String alphabet = "";
 
void loop() {

  if(Serial.available()) { // read the incoming byte:
    alphabet = Serial.readStringUntil('\n'); // say what you got until enter:
    Serial.print("recu : ");
    Serial.println(alphabet);
    
    //transmission
    Wire.beginTransmission(18);
    for(unsigned int i = 0; i < alphabet.length() ;i++){
      Wire.write(alphabet[i]);
    }

    Wire.endTransmission();
  
    alphabet= "";
  }

}




