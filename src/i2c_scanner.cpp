#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for Serial to be ready
  }
  Serial.println("\nI2C Scanner Starting...");
  Wire.begin(21, 22); // SDA = 21, SCL = 22 for ESP32
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Scanning...");
  byte addressesToScan[] = {0x3C, 0x40};
  for (int i = 0; i < 2; i++) {
    address = addressesToScan[i];
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) Serial.println("No I2C devices found\n");
  else Serial.println("done\n");
  delay(5000); // Wait 5 seconds before next scan
}
