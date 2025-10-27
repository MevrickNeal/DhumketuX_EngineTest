void incoming() {
  // Check for received packets
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String received = "";
    while (LoRa.available()) received += (char)LoRa.read();
    received.trim();
    // Serial.print("Received raw: ");
    // Serial.println(received);

    if (received.startsWith("OK")) {
      int commaIndex = received.indexOf(',');
      if (commaIndex > 0 && commaIndex < received.length() - 1) {
        String valStr = received.substring(commaIndex + 1);
        loadCellValue = valStr.toFloat();
        Serial.println(loadCellValue, 2);
      } else {
        Serial.println("OK received but no value found.");
      }
    }

    else Serial.println("Launch Pad not Ready");
  }
}