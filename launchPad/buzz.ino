void beep(byte count){
  for(byte i=0; i<count; i++){
    digitalWrite(buzzer, 1);
    delay(100);
    digitalWrite(buzzer, 0);
    delay(100);
  }
}