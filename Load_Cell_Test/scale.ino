void calibrate_scale() {
  Serial.println("Calibrating...");
  delay(1000);
  tare = 0;
  for (byte i = 0; i < sample_count; i++) tare += hx711.readChannelBlocking(CHAN_A_GAIN_64);
  avg_tare = tare / sample_count;
  Serial.println("Calibration Done!");
  Serial.print("Tare Value: ");
  Serial.println(avg_tare);
}

void check_weight() {
  while (!push()) {
    tare = hx711.readChannelBlocking(CHAN_A_GAIN_64);
    weight = (float)(tare - avg_tare) / weight_scale;
    Serial.print(tare);
    Serial.print(" ");
     Serial.println(weight, 2);
  }
}
