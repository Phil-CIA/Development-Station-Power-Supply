#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#if defined(LED_DATA_PIN)
constexpr uint8_t kLedDataPin = LED_DATA_PIN;
#else
constexpr uint8_t kLedDataPin = 18;
#endif

#if defined(LED_COUNT)
constexpr uint16_t kLedCount = LED_COUNT;
#else
constexpr uint16_t kLedCount = 30;
#endif

#if defined(LED_BRIGHTNESS)
constexpr uint8_t kDefaultBrightness = LED_BRIGHTNESS;
#else
constexpr uint8_t kDefaultBrightness = 64;
#endif

Adafruit_NeoPixel strip(kLedCount, kLedDataPin, NEO_GRB + NEO_KHZ800);

uint8_t brightness = kDefaultBrightness;
uint16_t hueOffset = 0;
uint32_t lastFrameMs = 0;

static uint32_t wheel(uint8_t wheelPos) {
	wheelPos = 255 - wheelPos;
	if (wheelPos < 85) {
		return strip.Color(255 - wheelPos * 3, 0, wheelPos * 3);
	}
	if (wheelPos < 170) {
		wheelPos -= 85;
		return strip.Color(0, wheelPos * 3, 255 - wheelPos * 3);
	}
	wheelPos -= 170;
	return strip.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
}

static void renderRainbowStep() {
	for (uint16_t i = 0; i < kLedCount; ++i) {
		const uint8_t colorIndex = static_cast<uint8_t>((i * 256U / kLedCount + hueOffset) & 0xFFU);
		strip.setPixelColor(i, wheel(colorIndex));
	}
	strip.show();
	hueOffset++;
}

static void clearStrip() {
	strip.clear();
	strip.show();
}

static void handleSerial() {
	if (!Serial.available()) {
		return;
	}

	const String cmd = Serial.readStringUntil('\n');
	String c = cmd;
	c.trim();
	c.toLowerCase();

	if (c == "off") {
		clearStrip();
		Serial.println("LEDs off");
		return;
	}

	if (c == "on") {
		strip.setBrightness(brightness);
		Serial.println("LED animation on");
		return;
	}

	if (c.startsWith("b")) {
		const int value = c.substring(1).toInt();
		const int clamped = constrain(value, 0, 255);
		brightness = static_cast<uint8_t>(clamped);
		strip.setBrightness(brightness);
		Serial.printf("Brightness set to %u\n", brightness);
		return;
	}

	Serial.println("Commands: on | off | b<0-255>");
}

void setup() {
	Serial.begin(115200);
	delay(300);

	strip.begin();
	strip.setBrightness(brightness);
	clearStrip();

	Serial.println();
	Serial.println("WS281x bring-up ready");
	Serial.printf("Data pin: GPIO%u, LED count: %u, brightness: %u\n", kLedDataPin, kLedCount, brightness);
	Serial.println("If colors are wrong, try RGB/BGR order in code.");
}

void loop() {
	handleSerial();

	const uint32_t now = millis();
	if (now - lastFrameMs >= 20) {
		lastFrameMs = now;
		renderRainbowStep();
	}
}
