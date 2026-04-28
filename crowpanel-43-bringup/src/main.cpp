#include <Arduino.h>
#include <Wire.h>

namespace {

constexpr uint8_t kBoardCtrlAddr = 0x30;
constexpr uint8_t kTouchAddr = 0x5D;
String serialLine;

bool i2cAddressResponds(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool sendBoardCommand(uint8_t command) {
  Wire.beginTransmission(kBoardCtrlAddr);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

String valueAfter(const String& input, const char* key) {
  const String keyEq = String(key) + "=";
  const int keyPos = input.indexOf(keyEq);
  if (keyPos < 0) {
    return String();
  }

  int start = keyPos + keyEq.length();
  int end = input.indexOf(' ', start);
  if (end < 0) {
    end = input.length();
  }

  String out = input.substring(start, end);
  out.trim();
  if (out.startsWith("\"") && out.endsWith("\"")) {
    out = out.substring(1, out.length() - 1);
  }
  return out;
}

void drawLabel(const String& text) {
  Serial.printf("LABEL: %s\n", text.c_str());
}

void fillColor(const String& line) {
  int r = valueAfter(line, "r").toInt();
  int g = valueAfter(line, "g").toInt();
  int b = valueAfter(line, "b").toInt();

  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);
  Serial.printf("COLOR: r=%d g=%d b=%d\n", r, g, b);
}

void printStatus() {
  Serial.printf("status board=0x30:%s touch=0x5D:%s\n",
                i2cAddressResponds(kBoardCtrlAddr) ? "ok" : "missing",
                i2cAddressResponds(kTouchAddr) ? "ok" : "missing");
}

void handleCommand(const String& rawLine) {
  String line = rawLine;
  line.trim();
  if (line.isEmpty()) {
    return;
  }

  if (line.equalsIgnoreCase("HELP")) {
    Serial.println("Commands: HELP, PING, STATUS, CMD:LABEL text=Your text, CMD:COLOR r=255 g=0 b=0");
    return;
  }

  if (line.equalsIgnoreCase("PING")) {
    Serial.println("PONG");
    return;
  }

  if (line.equalsIgnoreCase("STATUS")) {
    printStatus();
    return;
  }

  if (line.startsWith("CMD:LABEL")) {
    String text = valueAfter(line, "text");
    if (text.isEmpty()) {
      text = "CrowPanel online";
    }
    drawLabel(text);
    Serial.println("OK LABEL");
    return;
  }

  if (line.startsWith("CMD:COLOR")) {
    fillColor(line);
    Serial.println("OK COLOR");
    return;
  }

  Serial.printf("ERR unknown command: %s\n", line.c_str());
}

void runBoardInitSequence() {
  pinMode(1, OUTPUT);
  digitalWrite(1, LOW);
  delay(120);
  pinMode(1, INPUT);

  if (i2cAddressResponds(kBoardCtrlAddr)) {
    sendBoardCommand(0x19);
    delay(50);
    sendBoardCommand(0x10);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(600);
  Serial.println("CrowPanel 4.3 bring-up stub start (UART/I2C baseline)");
  Serial.println("pinmap RGB bus from Elecrow V1.2: D0=21 D1=47 D2=48 D3=45 D4=38 D5=9 D6=10 D7=11 D8=12 D9=13 D10=14 D11=7 D12=17 D13=18 D14=3 D15=46 DE=42 VSYNC=41 HSYNC=40 PCLK=39");

  Wire.begin(15, 16);
  delay(50);

  printStatus();
  runBoardInitSequence();

  Serial.println("Display driver is intentionally deferred in this baseline.");
  Serial.println("Next step: enable RGB panel init after board-revision confirmation.");
  Serial.println("ready");
}

void loop() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (!serialLine.isEmpty()) {
        handleCommand(serialLine);
        serialLine = "";
      }
    } else {
      serialLine += ch;
      if (serialLine.length() > 240) {
        serialLine = "";
        Serial.println("ERR line too long");
      }
    }
  }

  delay(2);
}
