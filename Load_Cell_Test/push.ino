byte push() {
  byte count = 0;
again:
  int timer = 0;
  if (digitalRead(button)) {

    delay(100);  //debounce delay
    digitalWrite(led, 0);
    while (digitalRead(button)) {
      delay(50);
      timer++;
    }
    digitalWrite(led, 1);
    if (timer) {
      count++;
      timer = 0;
      while (!digitalRead(button)) {
        delay(1);
        timer++;
        if (timer > 1000) return count;
      }
      goto again;
    }
  }
  digitalWrite(led , 1);
  return count;
}
