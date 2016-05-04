
#include <FuzzyInput.h>
#include <FuzzyRuleAntecedent.h>
#include <FuzzyComposition.h>
#include <Fuzzy.h>
#include <FuzzyOutput.h>
#include <FuzzyIO.h>
#include <FuzzySet.h>
#include <FuzzyRuleConsequent.h>
#include <FuzzyRule.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
 
// Data wire is plugged into pin 8 on the Arduino
#define ONE_WIRE_BUS 8

//Sensors
// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature tempSensors(&oneWire);

//LED Matrix
Adafruit_8x8matrix matrix = Adafruit_8x8matrix();

const int mainLoopDelayDuration = 20000;

const int sensorHumidityPin = A1;
int sensorHumidityValue = 0;

const int sensorUVPin = A0;
double sensorUVValue = 0.0;
double avgUVValue = 0.0;
int sensorUVSampleCount = 0;

double tempReading = 0.0;

double drinkingTime = 0.0;
int drinkingTimeInt = 0;

const int motorSwitchPin = A2;
int motorSwitchValue = 0;

//Fuzzy Logic
Fuzzy* fuzzyHerbert = new Fuzzy();

//Temperature
FuzzySet* cold = new FuzzySet(0, 5, 10, 20); //make sure these are defined correctly
FuzzySet* warm = new FuzzySet(15, 20, 20, 30);
FuzzySet* hot = new FuzzySet(25, 40, 50, 50);

//UV Light
FuzzySet* dark = new FuzzySet(0, 10, 20, 30); // need to test UV sensor
FuzzySet* cloudy = new FuzzySet(20, 43, 66, 90);
FuzzySet* bright = new FuzzySet(80,110 , 500, 500);

//Humidity
FuzzySet* dry = new FuzzySet(0, 100, 200, 500); //need to build and test humidity sensor
FuzzySet* moist = new FuzzySet(250, 400, 600, 700);
FuzzySet* sploosh = new FuzzySet(650, 850, 1000, 1000);
  
void setup(){
  
  Serial.begin(9600);
  Serial.println("Herb is Alive!");

  //led
  pinMode(13,OUTPUT);
  //pinMode(8,INPUT);
  
  tempSensors.begin();
  matrix.begin(0x70);

  pinMode(12,OUTPUT); //relay board for pump
  digitalWrite(12,LOW);
  
  //Fuzzy Inputs
  //Temperature Sensor
  FuzzyInput* temperature = new FuzzyInput(1);
  temperature->addFuzzySet(cold);
  temperature->addFuzzySet(warm);
  temperature->addFuzzySet(hot);
  
  fuzzyHerbert->addFuzzyInput(temperature);
  
  //UV Sensor
  FuzzyInput* light = new FuzzyInput(2);
  light->addFuzzySet(dark);
  light->addFuzzySet(cloudy);
  light->addFuzzySet(bright);
  
  fuzzyHerbert->addFuzzyInput(light);
  
  //Humidity Sensor
  FuzzyInput* humidity = new FuzzyInput(3);
  humidity->addFuzzySet(dry);
  humidity->addFuzzySet(moist);
  humidity->addFuzzySet(sploosh);
  
  fuzzyHerbert->addFuzzyInput(humidity);
  
  //Fuzzy Outputs
  FuzzyOutput* waterTimeSeconds = new FuzzyOutput(1);
  
  FuzzySet* noAgua = new FuzzySet(0, 0, 0, 0);
  FuzzySet* lilAgua = new FuzzySet(0, 4, 4, 5);
  FuzzySet* medAgua = new FuzzySet(4, 6, 6, 8);
  FuzzySet* muchoAgua = new FuzzySet(6, 9, 9, 12);
  waterTimeSeconds->addFuzzySet(noAgua);
  waterTimeSeconds->addFuzzySet(lilAgua);
  waterTimeSeconds->addFuzzySet(medAgua);
  waterTimeSeconds->addFuzzySet(muchoAgua);
  
  fuzzyHerbert->addFuzzyOutput(waterTimeSeconds);

  //1 Rule Fuzzy Antecedents  
  FuzzyRuleAntecedent* ifSploosh = new FuzzyRuleAntecedent();
  ifSploosh->joinSingle(sploosh);
 
  //2 Rule Fuzzy Antecedents
  FuzzyRuleAntecedent* ifDryAndHot = new FuzzyRuleAntecedent();
  ifDryAndHot->joinWithAND(dry,hot);

  FuzzyRuleAntecedent* ifMoistAndHot = new FuzzyRuleAntecedent();
  ifMoistAndHot->joinWithAND(moist,hot);

  FuzzyRuleAntecedent* ifMoistAndWarm = new FuzzyRuleAntecedent();
  ifMoistAndWarm->joinWithAND(moist,warm);

  FuzzyRuleAntecedent* ifMoistAndCold = new FuzzyRuleAntecedent();
  ifMoistAndCold->joinWithAND(moist,cold);

  FuzzyRuleAntecedent* ifDryAndWarm = new FuzzyRuleAntecedent();
  ifDryAndWarm->joinWithAND(dry,warm);

  FuzzyRuleAntecedent* ifDryAndCold = new FuzzyRuleAntecedent();
  ifDryAndCold->joinWithAND(dry,cold);

  FuzzyRuleAntecedent* ifDarkOrCloudy = new FuzzyRuleAntecedent();
  ifDarkOrCloudy->joinWithOR(dark,cloudy);

  FuzzyRuleAntecedent* ifBrightOrCloudy = new FuzzyRuleAntecedent();
  ifBrightOrCloudy->joinWithOR(bright,cloudy);

  //Compound Rule Fuzzy Antecedents
  FuzzyRuleAntecedent* ifDryAndHotAndBright = new FuzzyRuleAntecedent();
  ifDryAndHotAndBright->joinWithAND(ifDryAndHot,bright);

  FuzzyRuleAntecedent* ifMoistAndWarmAndBright = new FuzzyRuleAntecedent();
  ifMoistAndWarmAndBright->joinWithAND(ifMoistAndWarm,bright);

  FuzzyRuleAntecedent* ifMoistAndWarmAndDarkOrCloudy = new FuzzyRuleAntecedent();
  ifMoistAndWarmAndDarkOrCloudy->joinWithAND(ifMoistAndWarm,ifDarkOrCloudy);

  FuzzyRuleAntecedent* ifDryAndHotAndDarkOrCloudy = new FuzzyRuleAntecedent();
  ifDryAndHotAndDarkOrCloudy->joinWithAND(ifDryAndHot,ifDarkOrCloudy);

  FuzzyRuleAntecedent* ifDryAndWarmAndBrightOrCloudy = new FuzzyRuleAntecedent();
  ifDryAndWarmAndBrightOrCloudy->joinWithAND(ifDryAndWarm,ifBrightOrCloudy);

  FuzzyRuleAntecedent* ifDryAndWarmAndDark = new FuzzyRuleAntecedent();
  ifDryAndWarmAndDark->joinWithAND(ifDryAndWarm,dark);

  FuzzyRuleAntecedent* ifDryAndColdAndBright = new FuzzyRuleAntecedent();
  ifDryAndColdAndBright->joinWithAND(ifDryAndCold,bright);

  FuzzyRuleAntecedent* ifDryAndColdAndDarkOrCloudy = new FuzzyRuleAntecedent();
  ifDryAndColdAndDarkOrCloudy->joinWithAND(ifDryAndCold,ifDarkOrCloudy);

  //Building Fuzzy Concequences
  FuzzyRuleConsequent* thenWaterNone = new FuzzyRuleConsequent();
  thenWaterNone->addOutput(noAgua);

  FuzzyRuleConsequent* thenWaterLil = new FuzzyRuleConsequent();
  thenWaterLil->addOutput(lilAgua);

  FuzzyRuleConsequent* thenWaterMed = new FuzzyRuleConsequent();
  thenWaterMed->addOutput(medAgua);

  FuzzyRuleConsequent* thenWaterMucho = new FuzzyRuleConsequent();
  thenWaterMucho->addOutput(muchoAgua);

  //Building Fuzzy Rules  
  FuzzyRule* fuzzyRule1 = new FuzzyRule(1, ifDryAndHotAndBright, thenWaterMucho);
  fuzzyHerbert->addFuzzyRule(fuzzyRule1);

  FuzzyRule* fuzzyRule2 = new FuzzyRule(2, ifSploosh, thenWaterNone);
  fuzzyHerbert->addFuzzyRule(fuzzyRule2);

  FuzzyRule* fuzzyRule3 = new FuzzyRule(3, ifMoistAndHot, thenWaterLil);
  fuzzyHerbert->addFuzzyRule(fuzzyRule3);

  FuzzyRule* fuzzyRule4 = new FuzzyRule(4, ifMoistAndWarmAndBright, thenWaterLil);
  fuzzyHerbert->addFuzzyRule(fuzzyRule4);

  FuzzyRule* fuzzyRule5 = new FuzzyRule(5, ifMoistAndWarmAndDarkOrCloudy, thenWaterNone);
  fuzzyHerbert->addFuzzyRule(fuzzyRule5);

  FuzzyRule* fuzzyRule6 = new FuzzyRule(6, ifMoistAndCold, thenWaterNone);
  fuzzyHerbert->addFuzzyRule(fuzzyRule6);

  FuzzyRule* fuzzyRule7 = new FuzzyRule(7, ifDryAndHotAndDarkOrCloudy, thenWaterMed);
  fuzzyHerbert->addFuzzyRule(fuzzyRule7);

  FuzzyRule* fuzzyRule8 = new FuzzyRule(8, ifDryAndWarmAndBrightOrCloudy, thenWaterMed);
  fuzzyHerbert->addFuzzyRule(fuzzyRule8);

  FuzzyRule* fuzzyRule9 = new FuzzyRule(9, ifDryAndWarmAndDark, thenWaterLil);
  fuzzyHerbert->addFuzzyRule(fuzzyRule9);

  FuzzyRule* fuzzyRule10 = new FuzzyRule(10, ifDryAndColdAndBright, thenWaterMed);
  fuzzyHerbert->addFuzzyRule(fuzzyRule10);

  FuzzyRule* fuzzyRule11 = new FuzzyRule(11, ifDryAndColdAndDarkOrCloudy, thenWaterLil);
  fuzzyHerbert->addFuzzyRule(fuzzyRule11);
}

static const uint8_t PROGMEM
  smile_bmp[] =
  { B00111100,
    B01000010,
    B10100101,
    B10000001,
    B10100101,
    B10011001,
    B01000010,
    B00111100 },
  neutral_bmp[] =
  { B00111100,
    B01000010,
    B10100101,
    B10000001,
    B10111101,
    B10000001,
    B01000010,
    B00111100 },
  frown_bmp[] =
  { B00111100,
    B01000010,
    B10100101,
    B10000001,
    B10011001,
    B10100101,
    B01000010,
    B00111100 };

void loop(){
  int mainLoopDelay = mainLoopDelayDuration;
  
  tempSensors.requestTemperatures();
  tempReading = tempSensors.getTempCByIndex(0);
  //float tempReading = random(0 ,50);
  fuzzyHerbert->setInput(1,tempReading); //temp
  

  //double UVReading = random(0.0,11.0);
  //fuzzyHerbert->setInput(2,UVReading); //UV
  sensorUVValue += analogRead(sensorUVPin)*10;
  sensorUVSampleCount += 1;
  
  if (sensorUVSampleCount >= 60/(mainLoopDelay/1000)){ //average UV samples over 1 minute
    avgUVValue = sensorUVValue/10.0;
    sensorUVValue = 0.0;
    sensorUVSampleCount = 0;  
    Serial.println("resetting uv value");  
  }
  
  fuzzyHerbert->setInput(2,avgUVValue);

  //double humidityReading = random(0,50);
  //fuzzyHerbert->setInput(3,humidityReading); //humidity
  sensorHumidityValue = analogRead(sensorHumidityPin);
  fuzzyHerbert->setInput(3,sensorHumidityValue); //humidity

  /*//testing***HOT BRIGHT DRY***
  fuzzyHerbert->setInput(1,40); //temp
  fuzzyHerbert->setInput(2,400); //UV
  fuzzyHerbert->setInput(3,100); //humidity*/

  
  fuzzyHerbert->fuzzify();
  
  drinkingTime = fuzzyHerbert->defuzzify(1);
  //drinkingTime = random(0,12);
  
  if (drinkingTime < 1.0){drinkingTime = 0;}

  Serial.print("Temp:");
  Serial.print(tempReading);
  Serial.print(", UV Idx:");
  //Serial.print(UVReading);
  Serial.print(avgUVValue);
  Serial.print(", humidity:");
  //Serial.print(humidityReading);
  Serial.print(sensorHumidityValue);
  Serial.print(" ----> Water Herbert for: ");
  Serial.print(drinkingTime);
  Serial.println(" secs");

//  digitalWrite(13,HIGH);
//  delay(drinkingTime*1000);
//  digitalWrite(13,LOW);

  drinkingTimeInt = round(drinkingTime);
  motorSwitchValue = analogRead(motorSwitchPin);

  //display drinking time on matrix
  if (drinkingTimeInt == 0){ //all smiles
    matrix.clear();
    matrix.drawBitmap(0, 0, smile_bmp, 8, 8, LED_ON);
    matrix.writeDisplay();
    //delay(1000);
  } //no scrolling required
  else if (drinkingTimeInt < 10){
    matrix.setCursor(1,1);
    matrix.clear();
    matrix.print(drinkingTimeInt);
    matrix.writeDisplay();
    //delay(2000);  
  }
  else{ //scrolling required
    matrix.setTextSize(1);
    matrix.setTextWrap(false);
    for (int8_t x=0; x>=-16; x--) {
      matrix.clear();
      matrix.setCursor(x,1);
      matrix.print(drinkingTimeInt);
      matrix.writeDisplay();
      delay(100);
      mainLoopDelay -= 100;
    }
  }

  //activate relay
  if (motorSwitchValue > 1020 && drinkingTimeInt > 0){
    digitalWrite(12,HIGH);
    delay(drinkingTimeInt*1000);
    digitalWrite(12,LOW);
    mainLoopDelay -= drinkingTimeInt*1000;
  }
  
  delay(mainLoopDelay);
}
