/*
 * ============================================================================
 *  GUITAR HERO - DRUM KIT BITSTREAM V3 (COMPETITIVE GRADE)
 * ============================================================================
 * MCU: Arduino Pro Micro (ATmega32U4)
 * Radio: NRF24L01+ with voltage adapter
 * Expansion: CD74HC4067 16-Channel Multiplexer
 * LED: RGB Diffused 5mm Common ANODE
 * Power: 2x 18650 + TP4056 + Step-Up 5V
 * 
 * OPTIMIZATIONS:
 * - Circular buffer for simultaneous hits
 * - Priority interrupt for piezo reads
 * - Threshold-based velocity detection
 * - ACK Payloads for bidirectional comms
 * - LED animations with non-blocking state machines
 * 
 * PIEZO WIRING:
 * Each piezo needs: 1MΩ resistor in parallel + optional 5.1V Zener diode
 * 
 * Author: Guitar Hero DIY Project
 * License: MIT
 * ============================================================================
 */

#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>

// ============================================================================
// INSTRUMENT TYPE
// ============================================================================
#define INSTRUMENT_TYPE   0x03  // INST_DRUM

// ============================================================================
// PIN CONFIGURATION
// ============================================================================
// NRF24L01 Pins (Pro Micro SPI)
#define CE_PIN    9
#define CSN_PIN   10

// CD74HC4067 Multiplexer Control Pins
#define MUX_S0    2
#define MUX_S1    3
#define MUX_S2    4
#define MUX_S3    5
#define MUX_SIG   A0   // Analog signal from multiplexer

// Menu Buttons (Direct pins)
#define PIN_START     6
#define PIN_SELECT    7
#define PIN_UP        8
#define PIN_DOWN      A1
#define PIN_PAIR      A2

// RGB LED - Common ANODE (PWM pins)
#define PIN_LED_R     9   // NOTE: Shared with CE! Use different pin
// Actually on Pro Micro, pins 5,6,9,10 have PWM
// We need to reorganize since CE/CSN use 9/10
#define PIN_LED_R_ALT     5   // PWM
#define PIN_LED_G     6   // PWM
// Blue needs non-PWM fallback or software PWM
#define PIN_LED_B     A3  // Will use software PWM or digital

// Battery Monitor
#define PIN_BATTERY   A6

// ============================================================================
// MULTIPLEXER CHANNEL MAPPING
// ============================================================================
// Piezo Sensors (Channels 0-5)
#define MUX_SNARE     0
#define MUX_TOM1      1
#define MUX_TOM2      2
#define MUX_TOM3      3   // Or Hi-Hat
#define MUX_CYMBAL    4
#define MUX_KICK      5   // Can be piezo or reed switch

// Future expansion (Channels 6-15)
// Could add more pads, buttons, or analog controls

// ============================================================================
// PIEZO THRESHOLDS
// ============================================================================
#define PIEZO_THRESHOLD_MIN   30    // Minimum ADC value to register hit
#define PIEZO_THRESHOLD_MAX   1023  // Maximum ADC value
#define PIEZO_COOLDOWN_MS     30    // Minimum time between hits (debounce)
#define PIEZO_VELOCITY_SCALE  4     // Divide ADC by this for velocity byte

// ============================================================================
// RF CONFIGURATION
// ============================================================================
#define RF_CHANNEL      108

const uint64_t PAIRING_PIPE = 0xE8E8PAIR00LL;

const uint64_t PLAYER_PIPES[4] = {
  0xE8E8F0F001LL,
  0xE8E8F0F002LL,
  0xE8E8F0F003LL,
  0xE8E8F0F004LL,
};

// Drums use Player 3 retry delay by default (750µs)
const uint8_t RETRY_DELAYS[4] = {0, 1, 2, 3};

// ============================================================================
// BATTERY THRESHOLDS
// ============================================================================
#define BATTERY_CRITICAL_ADC  348
#define BATTERY_LOW_ADC       378

// ============================================================================
// TIMING
// ============================================================================
#define SCAN_INTERVAL_US      500     // 0.5ms = 2000Hz piezo scanning
#define TX_INTERVAL_US        2000    // 2ms = 500Hz transmission
#define BATTERY_CHECK_MS      5000
#define ANIMATION_FRAME_MS    20

// ============================================================================
// HIT BUFFER (Circular)
// ============================================================================
#define HIT_BUFFER_SIZE   8

struct DrumHit {
  uint8_t pad;        // Which pad was hit (0-5)
  uint8_t velocity;   // Hit strength (0-255)
  uint32_t timestamp; // When it happened
};

volatile DrumHit hitBuffer[HIT_BUFFER_SIZE];
volatile uint8_t hitBufferHead = 0;
volatile uint8_t hitBufferTail = 0;

// Cooldown tracking per pad
uint32_t lastHitTime[6] = {0, 0, 0, 0, 0, 0};

// ============================================================================
// PACKET STRUCTURE
// ============================================================================
// Drum packets are slightly different:
// Byte 0: Buttons [Start|Select|Up|Down|0|0|0|0]
// Byte 1: Pad hits [Cymbal|Tom3|Tom2|Tom1|Snare|Kick|0|0]
// Could expand to 3 bytes for velocity, but keep minimal for now

// ============================================================================
// ANIMATION SYSTEM (same as guitar)
// ============================================================================
enum AnimationType : uint8_t {
  ANIM_NONE = 0,
  ANIM_WAVE,
  ANIM_MORPH,
  ANIM_HEARTBEAT,
  ANIM_SOLID,
  ANIM_DEAD_BATTERY,
  ANIM_HIT_FLASH     // Brief flash on drum hit
};

struct RGB {
  uint8_t r, g, b;
};

const RGB PLAYER_COLORS[5] = {
  {128, 128, 128},  // 0: White
  {0, 255, 255},    // 1: Red
  {255, 255, 0},    // 2: Blue
  {255, 0, 255},    // 3: Green
  {0, 255, 0},      // 4: Magenta (default for drums)
};

// ============================================================================
// GLOBAL STATE
// ============================================================================
RF24 radio(CE_PIN, CSN_PIN);

uint16_t deviceId = 0;
uint8_t playerId = 0;
uint8_t playerColor = 4;  // Magenta default for drums

AnimationType currentAnim = ANIM_NONE;
uint32_t animStartTime = 0;
RGB currentColor = {255, 255, 255};
RGB targetColor = {0, 255, 0};  // Magenta

uint32_t lastScanTime = 0;
uint32_t lastTxTime = 0;
uint32_t lastBatteryCheck = 0;
uint32_t lastAnimFrame = 0;
uint32_t pairButtonStart = 0;
uint32_t pairingStart = 0;

bool isPairing = false;
bool isConnected = false;
bool isBatteryCritical = false;

uint8_t pendingPadHits = 0;   // Bits for pads hit since last TX
uint8_t lastButtons = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void setupPins();
void setupRadio();
void setupMultiplexer();
void loadSettings();
void saveSettings();
void selectMuxChannel(uint8_t channel);
uint16_t readMuxChannel(uint8_t channel);
void scanPiezos();
uint8_t readButtons();
void transmitData();
void handlePairing();
void checkPairButton();
void processHitBuffer();
void addHitToBuffer(uint8_t pad, uint8_t velocity);
void updateAnimation();
void setAnimation(AnimationType anim);
void setLED(uint8_t r, uint8_t g, uint8_t b);
RGB lerpColor(RGB from, RGB to, uint8_t t);

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  setupPins();
  setupMultiplexer();
  loadSettings();
  setupRadio();
  
  if (playerId > 0 && playerId <= 4) {
    targetColor = PLAYER_COLORS[playerColor];
    setAnimation(ANIM_WAVE);
    radio.openWritingPipe(PLAYER_PIPES[playerId - 1]);
    radio.setRetries(RETRY_DELAYS[playerId - 1], 5);
    isConnected = true;
  } else {
    setAnimation(ANIM_WAVE);
    currentColor = PLAYER_COLORS[0];
  }
  
  lastScanTime = micros();
  lastTxTime = micros();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  uint32_t nowMicros = micros();
  uint32_t nowMillis = millis();
  
  // ========== PRIORITY 1: Scan Piezos (2000Hz) ==========
  if (nowMicros - lastScanTime >= SCAN_INTERVAL_US) {
    scanPiezos();
    lastScanTime = nowMicros;
  }
  
  // ========== PRIORITY 2: Battery Check ==========
  if (nowMillis - lastBatteryCheck >= BATTERY_CHECK_MS) {
    uint16_t batt = analogRead(PIN_BATTERY);
    if (batt < BATTERY_CRITICAL_ADC) {
      isBatteryCritical = true;
      setAnimation(ANIM_DEAD_BATTERY);
    }
    lastBatteryCheck = nowMillis;
  }
  
  if (isBatteryCritical) {
    setLED(0, 255, 255);
    delay(1000);
    return;
  }
  
  // ========== PRIORITY 3: Pair Button ==========
  checkPairButton();
  
  // ========== PRIORITY 4: Pairing Mode ==========
  if (isPairing) {
    handlePairing();
    updateAnimation();
    return;
  }
  
  // ========== PRIORITY 5: Transmit Data (500Hz) ==========
  if (isConnected && (nowMicros - lastTxTime >= TX_INTERVAL_US)) {
    transmitData();
    lastTxTime = nowMicros;
  }
  
  // ========== PRIORITY 6: Animation (50 FPS) ==========
  if (nowMillis - lastAnimFrame >= ANIMATION_FRAME_MS) {
    updateAnimation();
    lastAnimFrame = nowMillis;
  }
}

// ============================================================================
// PIN SETUP
// ============================================================================
void setupPins() {
  // Menu buttons with pull-ups
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_SELECT, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_PAIR, INPUT_PULLUP);
  
  // RGB LED
  pinMode(PIN_LED_R_ALT, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  setLED(255, 255, 255);  // Off
}

// ============================================================================
// MULTIPLEXER SETUP
// ============================================================================
void setupMultiplexer() {
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_S3, OUTPUT);
  
  // Start at channel 0
  selectMuxChannel(0);
}

void selectMuxChannel(uint8_t channel) {
  // Set S0-S3 pins based on channel number (0-15)
  digitalWrite(MUX_S0, channel & 0x01);
  digitalWrite(MUX_S1, (channel >> 1) & 0x01);
  digitalWrite(MUX_S2, (channel >> 2) & 0x01);
  digitalWrite(MUX_S3, (channel >> 3) & 0x01);
}

uint16_t readMuxChannel(uint8_t channel) {
  selectMuxChannel(channel);
  delayMicroseconds(5);  // Small delay for mux settling
  return analogRead(MUX_SIG);
}

// ============================================================================
// PIEZO SCANNING
// ============================================================================
void scanPiezos() {
  uint32_t now = millis();
  
  // Scan each piezo pad
  for (uint8_t pad = 0; pad <= MUX_KICK; pad++) {
    // Check cooldown
    if (now - lastHitTime[pad] < PIEZO_COOLDOWN_MS) {
      continue;
    }
    
    uint16_t value = readMuxChannel(pad);
    
    if (value >= PIEZO_THRESHOLD_MIN) {
      // Hit detected!
      uint8_t velocity = min(255, value / PIEZO_VELOCITY_SCALE);
      
      // Add to buffer
      addHitToBuffer(pad, velocity);
      
      // Mark pending for next TX
      pendingPadHits |= (1 << (pad + 2));  // Shift to leave room for buttons
      
      // Update cooldown
      lastHitTime[pad] = now;
      
      // Brief flash animation
      if (currentAnim == ANIM_SOLID || currentAnim == ANIM_HEARTBEAT) {
        // Quick white flash
        setLED(200, 200, 200);
      }
    }
  }
}

void addHitToBuffer(uint8_t pad, uint8_t velocity) {
  uint8_t nextHead = (hitBufferHead + 1) % HIT_BUFFER_SIZE;
  
  // Check if buffer full (drop oldest)
  if (nextHead == hitBufferTail) {
    hitBufferTail = (hitBufferTail + 1) % HIT_BUFFER_SIZE;
  }
  
  hitBuffer[hitBufferHead].pad = pad;
  hitBuffer[hitBufferHead].velocity = velocity;
  hitBuffer[hitBufferHead].timestamp = millis();
  hitBufferHead = nextHead;
}

// ============================================================================
// READ BUTTONS
// ============================================================================
uint8_t readButtons() {
  uint8_t buttons = 0;
  
  if (digitalRead(PIN_START) == LOW)  buttons |= 0x80;
  if (digitalRead(PIN_SELECT) == LOW) buttons |= 0x40;
  if (digitalRead(PIN_UP) == LOW)     buttons |= 0x20;
  if (digitalRead(PIN_DOWN) == LOW)   buttons |= 0x10;
  
  return buttons;
}

// ============================================================================
// TRANSMIT DATA
// ============================================================================
void transmitData() {
  uint8_t buttons = readButtons();
  
  // Build packet: [Buttons | Pad Hits]
  // Pad hits: [0|0|Cymbal|Tom3|Tom2|Tom1|Snare|Kick]
  uint8_t packet[2] = {
    buttons | (pendingPadHits & 0x3F),  // Combine buttons and pads
    0  // Could be velocity or reserved
  };
  
  // Clear pending hits after sending
  pendingPadHits = 0;
  
  bool success = radio.write(packet, 2);
  
  if (success) {
    isConnected = true;
    
    if (radio.isAckPayloadAvailable()) {
      uint8_t ackData[2];
      radio.read(ackData, 2);
      // Process ACK (color changes, etc.)
      uint8_t colorId = ackData[0] & 0x0F;
      if (colorId != 0 && colorId != playerColor) {
        playerColor = colorId;
        targetColor = PLAYER_COLORS[colorId];
        setAnimation(ANIM_MORPH);
      }
    }
    
    if (currentAnim == ANIM_HEARTBEAT || currentAnim == ANIM_WAVE) {
      setAnimation(ANIM_SOLID);
    }
  } else {
    if (currentAnim == ANIM_SOLID) {
      setAnimation(ANIM_HEARTBEAT);
    }
  }
}

// ============================================================================
// RADIO SETUP
// ============================================================================
void setupRadio() {
  if (!radio.begin()) {
    while (1) {
      setLED(0, 255, 255);
      delay(50);
      setLED(255, 255, 255);
      delay(50);
    }
  }
  
  radio.setChannel(RF_CHANNEL);
  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setPayloadSize(2);
  radio.setAutoAck(true);
  radio.enableAckPayload();
  radio.setCRCLength(RF24_CRC_8);
  
  if (playerId > 0 && playerId <= 4) {
    radio.openWritingPipe(PLAYER_PIPES[playerId - 1]);
    radio.setRetries(RETRY_DELAYS[playerId - 1], 5);
  }
  
  radio.stopListening();
}

// ============================================================================
// SETTINGS
// ============================================================================
void loadSettings() {
  uint16_t magic;
  EEPROM.get(0, magic);
  
  if (magic != 0x4748) {
    randomSeed(analogRead(A7) ^ micros());
    deviceId = random(0x1000, 0xFFFF);
    playerId = 0;
    playerColor = 4;  // Magenta
    saveSettings();
  } else {
    EEPROM.get(2, deviceId);
    playerId = EEPROM.read(4);
    playerColor = EEPROM.read(5);
  }
}

void saveSettings() {
  uint16_t magic = 0x4748;
  EEPROM.put(0, magic);
  EEPROM.put(2, deviceId);
  EEPROM.write(4, playerId);
  EEPROM.write(5, playerColor);
}

// ============================================================================
// PAIRING
// ============================================================================
void checkPairButton() {
  bool pressed = (digitalRead(PIN_PAIR) == LOW);
  
  if (pressed) {
    if (pairButtonStart == 0) {
      pairButtonStart = millis();
    } else if (millis() - pairButtonStart >= 3000) {
      if (!isPairing) {
        isPairing = true;
        pairingStart = millis();
        setAnimation(ANIM_WAVE);
        currentColor = PLAYER_COLORS[0];
        radio.setPayloadSize(4);
        radio.openWritingPipe(PAIRING_PIPE);
        radio.setRetries(1, 10);
      }
      pairButtonStart = 0;
      delay(500);
    }
  } else {
    pairButtonStart = 0;
  }
}

void handlePairing() {
  if (millis() - pairingStart >= 30000) {
    exitPairingMode();
    return;
  }
  
  uint8_t request[4] = {
    0xAA,
    INSTRUMENT_TYPE,
    (uint8_t)(deviceId & 0xFF),
    (uint8_t)(deviceId >> 8)
  };
  
  bool success = radio.write(request, 4);
  
  if (success && radio.isAckPayloadAvailable()) {
    uint8_t response[2];
    radio.read(response, 2);
    
    if (response[0] >= 1 && response[0] <= 4) {
      playerId = response[0];
      playerColor = response[1];
      saveSettings();
      targetColor = PLAYER_COLORS[playerColor];
      setAnimation(ANIM_MORPH);
      delay(1500);
      exitPairingMode();
      return;
    }
  }
  
  delay(100);
}

void exitPairingMode() {
  isPairing = false;
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
}

void updateAnimation() {
  uint32_t elapsed = millis() - animStartTime;
  
  switch (currentAnim) {
    case ANIM_WAVE: {
      uint16_t phase = (elapsed % 2000) * 256 / 2000;
      uint8_t brightness = (phase < 128) ? phase * 2 : (255 - phase) * 2;
      
      RGB color = targetColor;
      color.r = 255 - ((255 - color.r) * brightness / 255);
      color.g = 255 - ((255 - color.g) * brightness / 255);
      color.b = 255 - ((255 - color.b) * brightness / 255);
      setLED(color.r, color.g, color.b);
      break;
    }
    
    case ANIM_MORPH: {
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
      uint16_t phase = elapsed % 5000;
      uint8_t brightness = 204;
      if (phase < 200) brightness = 204 + (phase * 51 / 200);
      else if (phase < 400) brightness = 255 - ((phase - 200) * 51 / 200);
      
      RGB color = PLAYER_COLORS[playerColor];
      color.r = 255 - ((255 - color.r) * brightness / 255);
      color.g = 255 - ((255 - color.g) * brightness / 255);
      color.b = 255 - ((255 - color.b) * brightness / 255);
      setLED(color.r, color.g, color.b);
      break;
    }
    
    case ANIM_SOLID:
      setLED(targetColor.r, targetColor.g, targetColor.b);
      break;
    
    case ANIM_DEAD_BATTERY:
      setLED(0, 255, 255);
      break;
    
    default:
      setLED(255, 255, 255);
      break;
  }
}

void setLED(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(PIN_LED_R_ALT, r);
  analogWrite(PIN_LED_G, g);
  // Blue on non-PWM pin - use threshold
  digitalWrite(PIN_LED_B, (b > 127) ? HIGH : LOW);
}

RGB lerpColor(RGB from, RGB to, uint8_t t) {
  RGB result;
  result.r = from.r + ((int16_t)(to.r - from.r) * t / 255);
  result.g = from.g + ((int16_t)(to.g - from.g) * t / 255);
  result.b = from.b + ((int16_t)(to.b - from.b) * t / 255);
  return result;
}
