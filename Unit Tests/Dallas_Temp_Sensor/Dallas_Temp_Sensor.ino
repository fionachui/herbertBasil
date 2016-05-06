#include <OneWire.h>
#include <DallasTemperature.h>
 
// Data wire is plugged into pin 7 on the Arduino
#define ONE_WIRE_BUS 8
 
// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature tempSensors(&oneWire);

int sensorUVPin = A0;
int sensorUVValue = 0;
 
void setup(void)
{
  // start serial port
  Serial.begin(9600);
  Serial.println("Dallas Temperature IC Control Library Demo");

  // Start up the library
  tempSensors.begin();
}
 
 
void loop(void)
{
  // call tempSensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  //Serial.print(" Requesting temperatures...");
  tempSensors.requestTemperatures(); // Send the command to get temperatures
  //Serial.println("DONE");

  Serial.print("Temperature for Device 1 is: ");
  Serial.print(tempSensors.getTempCByIndex(0)); // Why "byIndex"? 
    // You can have more than one IC on the same bus. 
    // 0 refers to the first IC on the wire

  sensorUVValue = analogRead(sensorUVPin);
  Serial.print(", UV sensor reading is:");
  Serial.println(sensorUVValue);
  
 
}
