const int sensorHumidityPin = A1;
int sensorHumidityValue = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

}

void loop() {
  // put your main code here, to run repeatedly:
  sensorHumidityValue = analogRead(sensorHumidityPin);
  Serial.print("HumidityValue: ");
  Serial.println(sensorHumidityValue);
  delay(1000);
}
