const int motorSwitchPin = A2;

void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);
  Serial.println("Testing");

  pinMode(12,OUTPUT);
  digitalWrite(12,LOW);

}

void loop() {
  // put your main code here, to run repeatedly:

  int motorSwitchValue = analogRead(motorSwitchPin);
  Serial.println(motorSwitchValue);

  if (motorSwitchValue > 1020){
    digitalWrite(12,HIGH);
    delay(5000);
    digitalWrite(12,LOW);
  }
  
  delay(1000);

}
