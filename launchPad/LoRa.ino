void incoming() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();
    incoming.trim();

    Serial.print("Received: ");
    Serial.println(incoming);

    if (incoming == "CHECK") sendStatus();

    else if (incoming == "IDLE") {
      state = IDLE;
      servo.write(90);         //change the direction if needed==================================
      digitalWrite(relay, 1);  //hip hip hoorray!
    }

    else if (incoming == "ARM") {
      state = ARMED;
      servo.write(0);  //change the direction if needed==================================
    }

    else if (incoming == "LAUNCH") {
      state = LAUNCHED;
      digitalWrite(relay, 0);  //hip hip hoorray!
      timer = millis();
    }
  }
}


void sendStatus() {
  Serial.println("Responding with OK and load cell value...");
  check_weight();
  LoRa.beginPacket();
  (calibrated) ? LoRa.print("OK,") : LoRa.print("NO,");
  LoRa.print(weight, 1);  // send 2 decimal precision
  LoRa.endPacket();
}
