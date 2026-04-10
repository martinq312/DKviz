const int analogPin = A0;
 
void setup() {
  //Setup serial connection
  Serial.begin(115200); 
}
 
void loop() {
  //Read analog pin
  int val = analogRead(analogPin);

  //Write analog value to serial port:
  Serial.write( 0xff );                                                         
  Serial.write( (val >> 8) & 0xff );                                            
  Serial.write( val & 0xff );
}