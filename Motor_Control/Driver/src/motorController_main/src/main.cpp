/*
Cree le 18/03/2019
Auteurs : Charles et Maude

Code pour controller un moteur en mode slave par I2C

Commandes pouvant etre entrees "#___": 
	Moteur
resetAngle : Set encoder to 0
readRegisters : Print the values of the registers
stop : Activate brake pin

	PID
togglePrint : Affiche les valeurs du PID ou non
setkp : Pour changer la valeur de kp
setki : Pour changer la valeur de ki
setkd : Pour changer la valeur de kd

Autres :
Entrer '=x' : Set encoder value to x
Entrer 'x' : Set goal to x

*/
#define ENCODER_OPTIMIZE_INTERRUPTS

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Encoder.h>


#pragma region "Prototypes" //{
//Fonctions SPI
void writeRegister(byte reg, byte value1, byte value2);
void writeRegisters();
unsigned int readRegister(byte reg);
void SPISetup();
void motorSetup();
void readRegisters();
void writeDefaultRegisters();


//Fonctions qui fonctionnent :
inline double readAngle(bool fullTurn);
void updateAngle(double newAngle, double& angle);
void resetAngle();
void checkIfFullTurn(double& angle);

void readSerial(double& angle);

inline void binaryControl(double);
inline void setDirection(bool);
inline void toggleGoal();

#pragma endregion //}

#pragma region "Variables et constantes" //{

//SPI communication
const byte READ = 0b10000000;
const byte WRITE = 0b00000000;
const byte faultReg = 0x2a;
const int maxSpeed = 480000; //Speed of the SPI (HZ)
const int CS = 10;
const byte registersAdress[] = {0x00,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x2A };

//Moteur
const int resetPin = 2;
const int enablePin = 3;
const int directionPin = 4;
const int brakePin = 5;
const int motorPin = 6;
const int lockPin = 7;
const int faultPin = 8;
const int encoderPin1 = 0, encoderPin2 = 1;

//Lecture de l'encoder
const double gearReduction = 1; //100 with gearbox, 1 without
const double encoderResolution = 4;
const double countsPerTurn = 800;

//Controle du moteur
const int pulseMax = 2047;
const int pulseMin = 100;
const int turnCW = 1;
const int turnCCW = 2;

//Conraintes sur les angles max et min
const int angleMax = 400;
const int angleMin = -10;

//Control
IntervalTimer timer;
Encoder encoder(encoderPin2, encoderPin1);
double goal = 10000; //Variation in angle to go
double position = 0;
double setpoint = 0;

unsigned long prevTime = 0;
bool toTurn = false;

bool state = false;

#pragma endregion //}

int comPin = 18;
long upGoal = 500;
long downGoal = 1;

void setup() {
	//Serial.begin(9600);
	//Serial.println("Starting");
  
	SPISetup();
	motorSetup();

	//timer.begin(toggleGoal, 5000000);
	pinMode(comPin, INPUT_PULLUP);
	// attachInterrupt(comPin, toggleState, CHANGE);
}

inline void toggleState(){
	Serial.println("Toggling state");
	state = digitalRead(comPin);
}

bool prevState = state;
int count = 0;
int turnTime = 350;
int speed = 400;

inline void timeControl(){
	if(state!=prevState){
		Serial.print("State changed to : ");
		Serial.println(state);
		prevState=state;
		count = 0;
		while(count <= turnTime){
			// Serial.println(count);
			digitalWrite(enablePin,HIGH);
			digitalWrite(brakePin, LOW);
			analogWrite(motorPin, speed);
			
			if(state){
				setDirection(HIGH);
			}
			else{
				setDirection(LOW);
			}
			count++;
			delay(10);
		}
		digitalWrite(enablePin, LOW);
		digitalWrite(brakePin, HIGH);
	}
}

void loop() {
	//readRegisters();
	// setDirection(true);
	analogWrite(motorPin, 2000);
	// state = digitalRead(comPin);
	Serial.println(readAngle(false));
	Serial.print('\t');
	// Serial.println(state);
	// timeControl();
	

	// //To control with EMG
	// state = digitalRead(comPin);
	// Serial.println(state); 
	// timeControl();

	// if (state)
	// {
	// binaryControl(upGoal);
	// }
	// else
	// {
	// binaryControl(downGoal);
	// }
}

#pragma region "Setup and SPI" //{

void writeRegister(byte reg, byte value1, byte value2) {

	byte toSend = WRITE | reg;

	SPI.beginTransaction(SPISettings(maxSpeed, MSBFIRST, SPI_MODE0));
	digitalWrite(CS, HIGH);
	delayMicroseconds(1);

	//send write command + reg adress
	SPI.transfer(toSend);

	//send first 8 bits (MSB)
	SPI.transfer(value1);
	//send last 8 bits (LSB)
	SPI.transfer(value2);

	digitalWrite(CS, LOW);
	SPI.endTransaction();
}

unsigned int readRegister(byte reg) {
	byte toRead = READ | reg;

	SPI.beginTransaction(SPISettings(maxSpeed, MSBFIRST, SPI_MODE0));
	digitalWrite(CS, HIGH);
	delayMicroseconds(1);
	//send read command + reg adress
	SPI.transfer(toRead);

	int result;
	byte readValue;

	//get first 8 bits of data (MSB)
	readValue = SPI.transfer(0x00);
	result = readValue << 8;
	//get last 8 bits of data (LSB)
	readValue = SPI.transfer(0x00);

	//combine into 16 bits result
	result = result | readValue;

	digitalWrite(CS, LOW);
	SPI.endTransaction();

	return (result);
}

void readRegisters() {
	//To Read the registers
	Serial.println("Lecture des registres");
	Serial.println(readRegister(0x00), HEX);
	Serial.println(readRegister(0x02), HEX);
	Serial.println(readRegister(0x03), HEX);
	Serial.println(readRegister(0x04), HEX);
	Serial.println(readRegister(0x0A), HEX);
	Serial.println();
	Serial.println();
}

void SPISetup() {
	//SPI Setup
	pinMode(CS, OUTPUT);
	SPI.begin();
	writeRegisters();
	//writeDefaultRegisters();
}

void writeRegisters() {
	writeRegister(0x00, 0x01, 0x11);
	writeRegister(0x01,0x00,0x00);
	writeRegister(0x02, 0x04, 0xFF);
	writeRegister(0x03, 0x68, 0x00);
	writeRegister(0x04, 0x00, 0xD7);
	writeRegister(0x05, 0x00, 0x00);
	writeRegister(0x06, 0x00, 0x00);
	writeRegister(0x07, 0x00, 0x00);
	writeRegister(0x08, 0x00, 0x00);
	writeRegister(0x09, 0x00, 0x00);
	writeRegister(0x0A, 0xF0, 0x00);
	writeRegister(0x0B, 0x00, 0x00);
	writeRegister(0x2A, 0x00, 0x00);
}

void readFault() {
	if (digitalRead(faultPin) == HIGH) {
		Serial.println("Il y a une faute :(");
	}
	else
		Serial.print("Pas de faute");

	Serial.print("Le registre de faute est : ");
	Serial.println(readRegister(0x2A), BIN);
}

void writeDefaultRegisters() {
	//Recommended setup
	writeRegister(0x00, 0x01, 0x11);
	writeRegister(0x01,0x00,0x00);
	writeRegister(0x02, 0x04, 0xFF);
	writeRegister(0x03, 0x68, 0x00);
	writeRegister(0x04, 0x00, 0xD7);
	writeRegister(0x05, 0x00, 0x00);
	writeRegister(0x06, 0x00, 0x00);
	writeRegister(0x07, 0x00, 0x00);
	writeRegister(0x08, 0x00, 0x00);
	writeRegister(0x09, 0x00, 0x00);
	writeRegister(0x0A, 0xF0, 0x00);
	writeRegister(0x0B, 0x00, 0x00);
	writeRegister(0x2A, 0x00, 0x00);
}

/*
void checkRegisters() {
	bool registersOk = true;
	for (int i = 0; i < 12; i++) {
		if (readRegister(registersAdress[i] != defaultValues[i]))
			registersOk = false;
	}
	if (registersOk)
		Serial.println("Les registres ont ete bien ecrit");
	else
		Serial.println("Erreur lors de l'ecriture des registres");
}*/

void motorSetup() {
	//Motor Setup
	pinMode(2, OUTPUT);
	pinMode(3, OUTPUT);
	pinMode(4, OUTPUT);
	pinMode(5, OUTPUT); //Brake
	pinMode(6, OUTPUT); //Motor
	pinMode(7, INPUT);
	pinMode(8, INPUT);

	analogWriteResolution(11);
	analogWriteFrequency(motorPin, 23437.5); //Values between 0-2047

	digitalWrite(2, LOW); //RESET
	digitalWrite(3, HIGH); //Enable
	digitalWrite(4, HIGH); //DIR
	digitalWrite(5, LOW); //Brake
}
#pragma endregion //}

#pragma region "Encoder" //{

//TESTER : Pour old controller, Fonction qui retourne l'angle du moteur, If fullTurn = true, will convert the angle to a circle (0-360), false for no correction
double countToAngleFactor = 360 / (countsPerTurn*gearReduction*encoderResolution);

inline double readAngle(bool fullTurn=false) {
	double angle =  encoder.read()*countToAngleFactor ; //To get degrees

	//if (fullTurn)
	//	checkIfFullTurn(angle);

	return angle;
}

#pragma endregion //}

#pragma region "Motor" //{

inline void setDirection(bool direction){
	if (direction)
		digitalWrite(directionPin, HIGH);
	else
		digitalWrite(directionPin, LOW);
}


inline void binaryControl(double goal)
{
	setpoint = goal;
	position = readAngle(false);
	int delta = abs(position - setpoint);
	if(!toTurn && delta > 25){
		toTurn = true;
	}
	else if (toTurn && delta < 10)
	{
		toTurn = false;
	}

	setDirection(position > setpoint);

	if (toTurn)
	{
		digitalWrite(brakePin,LOW);
		digitalWrite(enablePin, HIGH);
		analogWrite(motorPin, 400);
	}
	else 
	{
		digitalWrite(brakePin,HIGH);
		digitalWrite(enablePin,LOW);
	}
	
	// Serial.print("To turn : ");
	// Serial.print(toTurn);
	// Serial.print("\tPosition : ");
	// Serial.print(position);
	// Serial.print("\tSetpoint :");
	// Serial.println(setpoint);
}

inline void toggleGoal(){
	state = !state;
}

#pragma endregion //}

//Reset position if #reset is sent to the serial monitor
void resetAngle() {
	Serial.println("Angle reset to 0");
	encoder.write(0);
}


