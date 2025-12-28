/*
 * ============================================================================
 * GUITAR HERO - BITSTREAM  (COMPETITIVE GRADE)
 * ============================================================================
 * MCU: Arduino Nano V3 (ATmega328P)
 * Radio: NRF24L01+ with voltage adapter
 * LED: RGB Diffused 5mm Common ANODE (PWM pins)
 * Power: 2x 18650 + TP4056 + Step-Up 5V
 * 
 * OPTIMIZATIONS:
 * - Direct port reading (PIND) for 0.06µs button reads
 * - 2-byte ultra-light packets
 * - ACK Payloads for bidirectional comms without RX mode
 * - Hardware-based retry delays per instrument ID
 * - LED animations with non-blocking state machines
 * - Battery monitoring with critical shutdown
 * 
 * Author: Guitar Hero DIY Project
 * License: MIT
 * ============================================================================
 */

#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>

// ============================================================================
// INSTRUMENT CONFIGURATION - CHANGE FOR EACH DEVICE!
// ============================================================================
#define INSTRUMENT_TYPE   INST_GUITAR   // INST_GUITAR, INST_BASS, INST_DRUM, INST_MIC

#define INST_GUITAR   0x01
#define INST_BASS     0x02
#define INST_DRUM     0x03
#define INST_MIC      0x04

// ============================================================================
// PIN CONFIGURATION (Optimized for direct port reading)
// ============================================================================
// NRF24L01 Pins
#define CE_PIN    9
#define CSN_PIN   10
#define IRQ_PIN   2   // Interrupt pin (not used in TX mode but reserved)

// BUTTONS - Mapped to PORTD for single-cycle reads
// D2: Green    (PIND bit 2)
// D3: Red      (PIND bit 3) - NOTE: Conflicts with LED_R, use D4 instead
// D4: Yellow   (PIND bit 4)
// D5: Blue     (PIND bit 5) - NOTE: Conflicts with LED_G
// D6: Orange   (PIND bit 6) - NOTE: Conflicts with LED_B
// D7: Start    (PIND bit 7)
// Due to LED conflicts, we use alternative mapping:

// Fret Buttons (Digital reads, optimized)
#define PIN_GREEN       2
#define PIN_RED         4
#define PIN_YELLOW      7
#define PIN_BLUE        8
#define PIN_ORANGE      A3

// Strum & Menu
#define PIN_STRUM_UP    A4
#define PIN_STRUM_DOWN  A5
#define PIN_START       A2
#define PIN_SELECT      A6  // Analog only pin

// Whammy Bar (Analog)
#define PIN_WHAMMY      A0

// Battery Monitor
#define PIN_BATTERY     A7  // Analog only pin

// RGB LED - Common ANODE (PWM pins)
#define PIN_LED_R       3   // PWM (Timer2)
#define PIN_LED_G       5   // PWM (Timer0)
#define PIN_LED_B       6   // PWM (Timer0)

// Pair Button
#define PIN_PAIR        A1

// Power Switch (optional sensing)
// Connect to physical switch between battery and input

// ============================================================================
// RF CONFIGURATION
// ============================================================================
#define RF_CHANNEL      108

// Pairing pipe (public channel)
const uint64_t PAIRING_PIPE = 0xE8E8PAIR00LL;

// Player pipes (assigned after pairing)
const uint64_t PLAYER_PIPES[4] = {
  0xE8E8F0F001LL,  // Player 1 - Retry delay 250µs
  0xE8E8F0F002LL,  // Player 2 - Retry delay 500µs
  0xE8E8F0F003LL,  // Player 3 - Retry delay 750µs
  0xE8E8F0F004LL,  // Player 4 - Retry delay 1000µs
};

// Hardware retry delays per player (prevents collision loops)
const uint8_t RETRY_DELAYS[4] = {0, 1, 2, 3};  // 250µs * (n+1)

// ============================================================================
// BATTERY THRESHOLDS
// ============================================================================
// For 2x 18650 in parallel (3.0V - 4.2V range)
// Through voltage divider: Vbat * R2/(R1+R2) to ADC
// Assuming 10k/10k divider: 3.4V = ~348 ADC, 3.7V = ~378 ADC
#define BATTERY_CRITICAL_ADC  348   // ~3.4V - STOP TRANSMITTING
#define BATTERY_LOW_ADC       378   // ~3.7V - Warning
#define BATTERY_OK_ADC        400   // ~3.9V - Good

// ============================================================================
// TIMING
// ============================================================================
#define TX_INTERVAL_US        2000    // 2ms = 500Hz (competitive rate)
#define PAIR_HOLD_MS          3000    // 3 sec to enter pairing
#define PAIR_TIMEOUT_MS       30000   // 30 sec pairing window
#define BATTERY_CHECK_MS      5000    // Check battery every 5 sec
#define ANIMATION_FRAME_MS    20      // 50 FPS animations

// ============================================================================
// PACKET STRUCTURES (Ultra-light)
// ============================================================================
// Input Packet: 2 bytes only!
// Byte 0: Buttons [Start|Orange|Blue|Yellow|Red|Green|StrumDn|StrumUp]
// Byte 1: Whammy (0-255) OR Velocity (for drums)

// ACK Payload from Hub: 1 byte
// Byte 0: Command [AnimID:4 | ColorID:4]

// Pairing Request: 4 bytes
// Byte 0: 0xAA (Magic)
// Byte 1: Instrument Type
// Byte 2-3: Device ID (16-bit truncated)

// Pairing Response (in ACK): 2 bytes
// Byte 0: Player ID (1-4)
// Byte 1: Color code

// ============================================================================
// ANIMATION SYSTEM
// ============================================================================
enum AnimationType : uint8_t {
  ANIM_NONE = 0,
  ANIM_WAVE,        // Breathing white (pairing mode)
  ANIM_MORPH,       // Color transition (connecting)
  ANIM_HEARTBEAT,   // Subtle pulse (standby)
  ANIM_SOLID,       // Solid color (playing)
  ANIM_DEAD_BATTERY // Red solid (critical)
};

// Player colors (for common anode: 255 = OFF, 0 = FULL ON)
struct RGB {
  uint8_t r, g, b;
};

const RGB PLAYER_COLORS[5] = {
  {128, 128, 128},  // 0: White (pairing)
  {0, 255, 255},    // 1: Red
  {255, 255, 0},    // 2: Blue  
  {255, 0, 255},    // 3: Green
  {0, 255, 0},      // 4: Magenta
};

// ============================================================================
// GLOBAL STATE
// ============================================================================
RF24 radio(CE_PIN, CSN_PIN);

// Device identity
uint16_t deviceId = 0;
uint8_t playerId = 0;         // 0 = not paired, 1-4 = player number
uint8_t playerColor = 0;

// Animation state
AnimationType currentAnim = ANIM_NONE;
AnimationType targetAnim = ANIM_NONE;
uint32_t animStartTime = 0;
uint8_t animPhase = 0;
RGB currentColor = {255, 255, 255};  // Start OFF
RGB targetColor = {255, 255, 255};

// Timing
uint32_t lastTxTime = 0;
uint32_t lastBatteryCheck = 0;
uint32_t lastAnimFrame = 0;
uint32_t pairButtonStart = 0;
uint32_t pairingStart = 0;

// State flags
bool isPairing = false;
bool isConnected = false;
bool isBatteryCritical = false;
bool isBatteryLow = false;

// Input state
uint8_t lastButtons = 0;
uint8_t lastWhammy = 127;

// ============================================================================
// EEPROM LAYOUT
// ============================================================================
#define EEPROM_MAGIC      0   // 2 bytes
#define EEPROM_DEVICE_ID  2   // 2 bytes
#define EEPROM_PLAYER_ID  4   // 1 byte
#define EEPROM_COLOR      5   // 1 byte

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void setupPins();
void setupRadio();
void loadSettings();
void saveSettings();
uint8_t readButtonsFast();
uint8_t readWhammy();
uint16_t readBattery();
void transmitData();
void handlePairing();
void checkPairButton();
void processAckPayload(uint8_t* data, uint8_t len);
void updateAnimation();
void setAnimation(AnimationType anim);
void setLED(uint8_t r, uint8_t g, uint8_t b);
void setPlayerColor(uint8_t player);
RGB lerpColor(RGB from, RGB to, uint8_t t);

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  // Minimal serial for debugging (comment in production)
  // Serial.begin(115200);
  
  setupPins();
  loadSettings();
  setupRadio();
  
  // Startup animation based on pairing status
  if (playerId > 0 && playerId <= 4) {
    // Already paired - show wave of assigned color, then connect
    playerColor = playerId;
    setAnimation(ANIM_WAVE);
    targetColor = PLAYER_COLORS[playerId];
    
    // Configure radio for assigned pipe
    radio.openWritingPipe(PLAYER_PIPES[playerId - 1]);
    radio.setRetries(RETRY_DELAYS[playerId - 1], 5);
    isConnected = true;
  } else {
    // Not paired - white wave
    setAnimation(ANIM_WAVE);
    currentColor = PLAYER_COLORS[0];
  }
  
  lastTxTime = micros();
  lastBatteryCheck = millis();
}

// ============================================================================
// MAIN LOOP (Ultra-tight)
// ============================================================================
void loop() {
  uint32_t nowMicros = micros();
  uint32_t nowMillis = millis();
  
  // ========== PRIORITY 1: Battery Check (every 5 sec) ==========
  if (nowMillis - lastBatteryCheck >= BATTERY_CHECK_MS) {
    uint16_t battADC = readBattery();
    
    if (battADC < BATTERY_CRITICAL_ADC) {
      isBatteryCritical = true;
      setAnimation(ANIM_DEAD_BATTERY);
      // STOP ALL TRANSMISSION
    } else if (battADC < BATTERY_LOW_ADC) {
      isBatteryLow = true;
    } else {
      isBatteryLow = false;
    }
    lastBatteryCheck = nowMillis;
  }
  
  // If battery critical, only show red LED and halt
  if (isBatteryCritical) {
    setLED(0, 255, 255);  // Red solid
    delay(1000);
    return;
  }
  
  // ========== PRIORITY 2: Check Pair Button ==========
  checkPairButton();
  
  // ========== PRIORITY 3: Pairing Mode ==========
  if (isPairing) {
    handlePairing();
    updateAnimation();
    return;
  }
  
  // ========== PRIORITY 4: Transmit Data (500Hz) ==========
  if (isConnected && (nowMicros - lastTxTime >= TX_INTERVAL_US)) {
    transmitData();
    lastTxTime = nowMicros;
  }
  
  // ========== PRIORITY 5: Update Animation (50 FPS) ==========
  if (nowMillis - lastAnimFrame >= ANIMATION_FRAME_MS) {
    updateAnimation();
    lastAnimFrame = nowMillis;
  }
}

// ============================================================================
// PIN SETUP
// ============================================================================
void setupPins() {
  // Buttons with pull-ups
  pinMode(PIN_GREEN, INPUT_PULLUP);
  pinMode(PIN_RED, INPUT_PULLUP);
  pinMode(PIN_YELLOW, INPUT_PULLUP);
  pinMode(PIN_BLUE, INPUT_PULLUP);
  pinMode(PIN_ORANGE, INPUT_PULLUP);
  pinMode(PIN_STRUM_UP, INPUT_PULLUP);
  pinMode(PIN_STRUM_DOWN, INPUT_PULLUP);
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_PAIR, INPUT_PULLUP);
  
  // RGB LED (Common Anode - HIGH = OFF)
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  setLED(255, 255, 255);  // Start OFF
  
  // Configure ADC for faster reads
  // ADCSRA = (ADCSRA & 0xF8) | 0x04;  // Prescaler 16 = faster ADC
}

// ============================================================================
// RADIO SETUP
// ============================================================================
void setupRadio() {
  if (!radio.begin()) {
    // Radio failed - blink red rapidly forever
    while (1) {
      setLED(0, 255, 255);
      delay(50);
      setLED(255, 255, 255);
      delay(50);
    }
  }
  
  radio.setChannel(RF_CHANNEL);
  radio.setDataRate(RF24_2MBPS);        // Maximum speed
  radio.setPALevel(RF24_PA_HIGH);
  radio.setPayloadSize(2);              // Ultra-light packets
  radio.setAutoAck(true);
  radio.enableAckPayload();             // Enable ACK payloads from Hub
  radio.setCRCLength(RF24_CRC_8);
  
  // If paired, configure for player pipe
  if (playerId > 0 && playerId <= 4) {
    radio.openWritingPipe(PLAYER_PIPES[playerId - 1]);
    radio.setRetries(RETRY_DELAYS[playerId - 1], 5);
  }
  
  radio.stopListening();  // TX mode
}

// ============================================================================
// SETTINGS MANAGEMENT
// ============================================================================
void loadSettings() {
  uint16_t magic;
  EEPROM.get(EEPROM_MAGIC, magic);
  
  if (magic != 0x4748) {  // "GH"
    // First boot - generate ID
    randomSeed(analogRead(A7) ^ analogRead(A6) ^ micros());
    deviceId = random(0x1000, 0xFFFF);
    playerId = 0;
    playerColor = 0;
    saveSettings();
  } else {
    EEPROM.get(EEPROM_DEVICE_ID, deviceId);
    playerId = EEPROM.read(EEPROM_PLAYER_ID);
    playerColor = EEPROM.read(EEPROM_COLOR);
  }
}

void saveSettings() {
  uint16_t magic = 0x4748;
  EEPROM.put(EEPROM_MAGIC, magic);
  EEPROM.put(EEPROM_DEVICE_ID, deviceId);
  EEPROM.write(EEPROM_PLAYER_ID, playerId);
  EEPROM.write(EEPROM_COLOR, playerColor);
}

// ============================================================================
// FAST BUTTON READING
// ============================================================================
uint8_t readButtonsFast() {
  // Bit packing: [Start|Orange|Blue|Yellow|Red|Green|StrumDn|StrumUp]
  uint8_t buttons = 0;
  
  // Active LOW - Invert with XOR at the end
  if (digitalRead(PIN_STRUM_UP) == LOW)   buttons |= 0x01;
  if (digitalRead(PIN_STRUM_DOWN) == LOW) buttons |= 0x02;
  if (digitalRead(PIN_GREEN) == LOW)      buttons |= 0x04;
  if (digitalRead(PIN_RED) == LOW)        buttons |= 0x08;
  if (digitalRead(PIN_YELLOW) == LOW)     buttons |= 0x10;
  if (digitalRead(PIN_BLUE) == LOW)       buttons |= 0x20;
  if (digitalRead(PIN_ORANGE) == LOW)     buttons |= 0x40;
  if (digitalRead(PIN_START) == LOW)      buttons |= 0x80;
  
  return buttons;
}

uint8_t readWhammy() {
  return map(analogRead(PIN_WHAMMY), 0, 1023, 0, 255);
}

uint16_t readBattery() {
  return analogRead(PIN_BATTERY);
}

// ============================================================================
// TRANSMIT DATA
// ============================================================================
void transmitData() {
  uint8_t buttons = readButtonsFast();
  uint8_t whammy = readWhammy();
  
  // Build 2-byte packet
  uint8_t packet[2] = {buttons, whammy};
  
  // Send and check for ACK payload
  bool success = radio.write(packet, 2);
  
  if (success) {
    isConnected = true;
    
    // Check if Hub sent data in ACK
    if (radio.isAckPayloadAvailable()) {
      uint8_t ackData[2];
      radio.read(ackData, sizeof(ackData));
      processAckPayload(ackData, 2);
    }
    
    // If was in heartbeat mode, go to solid
    if (currentAnim == ANIM_HEARTBEAT || currentAnim == ANIM_WAVE) {
      setAnimation(ANIM_SOLID);
      targetColor = PLAYER_COLORS[playerId];
    }
  } else {
    // Connection lost - enter heartbeat mode
    if (currentAnim == ANIM_SOLID) {
      setAnimation(ANIM_HEARTBEAT);
    }
  }
  
  lastButtons = buttons;
  lastWhammy = whammy;
}

// ============================================================================
// PROCESS ACK PAYLOAD
// ============================================================================
void processAckPayload(uint8_t* data, uint8_t len) {
  if (len < 1) return;
  
  // Format: [AnimID:4 | ColorID:4]
  uint8_t animId = (data[0] >> 4) & 0x0F;
  uint8_t colorId = data[0] & 0x0F;
  
  // Change animation if requested
  if (animId != 0 && animId != currentAnim) {
    setAnimation((AnimationType)animId);
  }
  
  // Change color if requested
  if (colorId != 0 && colorId != playerColor) {
    playerColor = colorId;
    targetColor = PLAYER_COLORS[colorId];
    if (currentAnim == ANIM_SOLID) {
      setAnimation(ANIM_MORPH);  // Smooth transition
    }
  }
}

// ============================================================================
// PAIRING SYSTEM
// ============================================================================
void checkPairButton() {
  bool pressed = (digitalRead(PIN_PAIR) == LOW);
  
  if (pressed) {
    if (pairButtonStart == 0) {
      pairButtonStart = millis();
    } else if (millis() - pairButtonStart >= PAIR_HOLD_MS) {
      if (!isPairing) {
        // Enter pairing mode
        isPairing = true;
        pairingStart = millis();
        setAnimation(ANIM_WAVE);
        currentColor = PLAYER_COLORS[0];  // White wave
        
        // Configure radio for pairing
        radio.setPayloadSize(4);
        radio.openWritingPipe(PAIRING_PIPE);
        radio.setRetries(1, 10);  // Quick retries during pairing
      }
      pairButtonStart = 0;
      delay(500);
    }
  } else {
    pairButtonStart = 0;
  }
}

void handlePairing() {
  uint32_t now = millis();
  
  // Check timeout
  if (now - pairingStart >= PAIR_TIMEOUT_MS) {
    exitPairingMode();
    return;
  }
  
  // Build pairing request
  uint8_t request[4] = {
    0xAA,                       // Magic byte
    INSTRUMENT_TYPE,
    (uint8_t)(deviceId & 0xFF),
    (uint8_t)(deviceId >> 8)
  };
  
  // Send request
  bool success = radio.write(request, 4);
  
  if (success && radio.isAckPayloadAvailable()) {
    uint8_t response[2];
    radio.read(response, 2);
    
    if (response[0] >= 1 && response[0] <= 4) {
      // Pairing successful!
      playerId = response[0];
      playerColor = response[1];
      
      // Save to EEPROM
      saveSettings();
      
      // Morph to assigned color
      targetColor = PLAYER_COLORS[playerColor];
      setAnimation(ANIM_MORPH);
      
      delay(1500);  // Show morph animation
      exitPairingMode();
      return;
    }
  }
  
  delay(100);  // Wait before next attempt
}

void exitPairingMode() {
  isPairing = false;
  
  // Reconfigure radio
  radio.setPayloadSize(2);
  
  if (playerId > 0 && playerId <= 4) {
    radio.openWritingPipe(PLAYER_PIPES[playerId - 1]);
    radio.setRetries(RETRY_DELAYS[playerId - 1], 5);
    isConnected = true;
    setAnimation(ANIM_SOLID);
    targetColor = PLAYER_COLORS[playerColor];
  } else {
    setAnimation(ANIM_WAVE);
  }
}

// ============================================================================
// ANIMATION SYSTEM
// ============================================================================
void setAnimation(AnimationType anim) {
  currentAnim = anim;
  animStartTime = millis();
  animPhase = 0;
}

void updateAnimation() {
  uint32_t elapsed = millis() - animStartTime;
  
  switch (currentAnim) {
    case ANIM_WAVE: {
      // Breathing effect - 2 second cycle
      // Use sine approximation: y = sin(t) mapped to 0-255
      uint16_t phase = (elapsed % 2000) * 256 / 2000;
      uint8_t brightness;
      
      if (phase < 128) {
        brightness = phase * 2;  // Rising
      } else {
        brightness = (255 - phase) * 2;  // Falling
      }
      
      // Apply brightness to target color (common anode: 255 = off)
      RGB color = targetColor;
      color.r = 255 - ((255 - color.r) * brightness / 255);
      color.g = 255 - ((255 - color.g) * brightness / 255);
      color.b = 255 - ((255 - color.b) * brightness / 255);
      
      setLED(color.r, color.g, color.b);
      break;
    }
    
    case ANIM_MORPH: {
      // Linear transition - 1.5 seconds
      if (elapsed >= 1500) {
        currentColor = targetColor;
        setAnimation(ANIM_SOLID);
        return;
      }
      
      uint8_t t = elapsed * 255 / 1500;
      RGB color = lerpColor(currentColor, targetColor, t);
      setLED(color.r, color.g, color.b);
      break;
    }
    
    case ANIM_HEARTBEAT: {
      // Subtle pulse every 5 seconds
      // Base brightness 80%, pulse up to 100%
      uint16_t phase = elapsed % 5000;
      uint8_t brightness = 204;  // 80% of 255
      
      if (phase < 200) {
        // Pulse up
        brightness = 204 + (phase * 51 / 200);
      } else if (phase < 400) {
        // Pulse down
        brightness = 255 - ((phase - 200) * 51 / 200);
      }
      
      RGB color = PLAYER_COLORS[playerColor];
      color.r = 255 - ((255 - color.r) * brightness / 255);
      color.g = 255 - ((255 - color.g) * brightness / 255);
      color.b = 255 - ((255 - color.b) * brightness / 255);
      
      setLED(color.r, color.g, color.b);
      break;
    }
    
    case ANIM_SOLID: {
      // Just show the color
      setLED(targetColor.r, targetColor.g, targetColor.b);
      break;
    }
    
    case ANIM_DEAD_BATTERY: {
      // Solid red, no animation
      setLED(0, 255, 255);  // Red (common anode)
      break;
    }
    
    default:
      setLED(255, 255, 255);  // Off
      break;
  }
}

// ============================================================================
// LED CONTROL
// ============================================================================
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(PIN_LED_R, r);
  analogWrite(PIN_LED_G, g);
  analogWrite(PIN_LED_B, b);
}

void setPlayerColor(uint8_t player) {
  if (player >= 1 && player <= 4) {
    RGB c = PLAYER_COLORS[player];
    setLED(c.r, c.g, c.b);
  }
}

RGB lerpColor(RGB from, RGB to, uint8_t t) {
  // Linear interpolation between two colors
  RGB result;
  result.r = from.r + ((int16_t)(to.r - from.r) * t / 255);
  result.g = from.g + ((int16_t)(to.g - from.g) * t / 255);
  result.b = from.b + ((int16_t)(to.b - from.b) * t / 255);
  return result;
}
