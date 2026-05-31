/*
  Hat Board I2C Bridge Firmware (ESP32-C6)
  
  Simple I2C slave that acts as GPIO bridge for switch matrix and lamp control.
  No stateful protocol - just reads GPIO and responds to I2C queries.
  
  I2C Slave Address: 0x24
  
  Register Map (simplified, no HT16K33 emulation):
  - Register 0x00: Switch state (sw0) - GPIO inputs as bitmask
  - Register 0x01: Lamp control (out0) - GPIO outputs as bitmask
  - ... (future expansion for more rows/columns)
  
  Protocol:
  1. Control board sends read command to get switch state at address 0x00
  2. Hat board responds with current GPIO pin states as bitmask
  3. Control board sends write command to set lamps (not implemented yet)
*/

#include <Arduino.h>
#include <Wire.h>

// I2C Configuration
const int I2C_SDA = 21;
const int I2C_SCL = 22;
const uint8_t I2C_SLAVE_ADDRESS = 0x24;

// Switch column GPIOs from Captain mapping (active low)
const int SWITCH_GPIO_0[] = {20, 18, 21, 19};

// Lamp control GPIO mapping (outputs)
// const int LAMP_GPIO[] = {...};  // Future: add lamp control pins

// I2C buffer
static uint8_t i2cRegister = 0x00;  // Current register pointer
static uint8_t switchState = 0xFF;  // Default all open

// ============================================================================
// I2C Slave Callbacks
// ============================================================================

void onI2CReceive(int length) {
    // First byte is always register address
    if (Wire.available()) {
        i2cRegister = Wire.read();
        
        // If more bytes follow, this is a write operation (for lamp control, etc.)
        // For now, just consume any extra bytes
        while (Wire.available()) {
            Wire.read();
        }
    }
}

void onI2CRequest() {
    // Control board is reading from current register
    switch (i2cRegister) {
        case 0x00:  // Switch state register
            // Read all switch GPIO pins and pack into byte
            switchState = 0xFF;  // Default all open
            for (int i = 0; i < 4; i++) {
                if (!digitalRead(SWITCH_GPIO_0[i])) {
                    switchState &= ~(1 << i);  // Clear bit if pin is LOW (switch closed)
                }
            }
            Wire.write(switchState);
            break;
            
        case 0x01:  // Diagnostics register (status byte)
            // Respond with simple status: 0x80 = enabled, 0x00 = error
            // For simplified bridge, just return 0x80 (enabled) always
            Wire.write(0x80);
            break;
            
        default:
            // Unknown register - return 0x00
            Wire.write(0x00);
            break;
    }
}

// ============================================================================
// Setup & Main Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n=== Hat Board I2C Bridge Firmware ===");
    
    // Initialize switch GPIO as inputs
    for (int i = 0; i < 4; i++) {
        pinMode(SWITCH_GPIO_0[i], INPUT_PULLUP);
    }
    
    // Initialize I2C slave at 0x24
    Wire.begin(I2C_SLAVE_ADDRESS, I2C_SDA, I2C_SCL, 100000);
    Wire.onReceive(onI2CReceive);
    Wire.onRequest(onI2CRequest);
    Wire.setBufferSize(32);
    
    // Set I2C slave address
    // Note: ESP32-C6 Wire library slave mode differs from ESP32
    // Using begin() with address parameter if available
    Serial.printf("I2C Slave initialized at address 0x%02X\n", I2C_SLAVE_ADDRESS);
    Serial.printf("SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);
    Serial.println("Ready to bridge switch matrix I2C reads");
}

void loop() {
    // Minimal loop - I2C callbacks handle all communication
    delay(100);
    
    // Optional: periodically log current switch state for debugging
    static uint32_t lastLogMs = 0;
    uint32_t now = millis();
    if (now - lastLogMs > 5000) {
        lastLogMs = now;
        uint8_t current = 0xFF;
        for (int i = 0; i < 4; i++) {
            if (!digitalRead(SWITCH_GPIO_0[i])) {
                current &= ~(1 << i);
            }
        }
        Serial.printf("[%7lu] Switch state: 0x%02X\n", now, current);
    }
}
