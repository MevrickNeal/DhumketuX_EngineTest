byte push() {
  byte count = 0;
again:
  int t = 0;
  if (digitalRead(button)) {
    delay(100);  //debounce delay
    while (digitalRead(button)) {
      delay(50);
      t++;
    }

    if (t) {
      count++;
      timer = 0;
      while (!digitalRead(button)) {
        delay(1);
        t++;
        if (t > 1000) return count;
      }
      goto again;
    }
  }
  return count;
}