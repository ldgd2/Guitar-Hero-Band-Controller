# Drum Controller

<p align="center">
  <img src="https://img.shields.io/badge/MCU-Arduino_Pro_Micro-00979D?style=for-the-badge&logo=arduino&logoColor=white" alt="Pro Micro"/>
  <img src="https://img.shields.io/badge/SCAN_RATE-2000Hz-00ff00?style=for-the-badge" alt="Scan Rate"/>
  <img src="https://img.shields.io/badge/TX_RATE-500Hz-blue?style=for-the-badge" alt="TX Rate"/>
  <img src="https://img.shields.io/badge/PADS-5_Piezo-yellow?style=for-the-badge" alt="Pads"/>
</p>

Robust design for handling multiple analog sensors simultaneously without blocking.
Uses a 16-channel multiplexer to read all piezos with a single analog pin.

---

## Table of Contents

1. [Bill of Materials](#bill-of-materials)
2. [Wiring Diagrams](#wiring-diagrams)
3. [Power System](#power-system)
4. [LED System](#led-system)
5. [CD74HC4067 Multiplexer](#cd74hc4067-multiplexer)
6. [Piezo Circuit](#piezo-circuit)
7. [Kick Pedal](#kick-pedal)
8. [Code Architecture](#code-architecture)
9. [Communication Protocol](#communication-protocol)
10. [Pairing System](#pairing-system)
11. [Calibration](#calibration)
12. [Upload Code](#upload-code)
13. [Troubleshooting](#troubleshooting)

---

## Bill of Materials

### Brain

| Component | Quantity |
|-----------|----------|
| Arduino Pro Micro (ATmega32U4) | 1 |

### Expansion

| Component | Quantity |
|-----------|----------|
| CD74HC4067 Multiplexer (16 channels) | 1 |

### Communication

| Component | Quantity |
|-----------|----------|
| NRF24L01 Module (onboard antenna) | 1 |
| Voltage adapter (8-pin socket) | 1 |

### Power

| Component | Quantity |
|-----------|----------|
| Li-Ion 18650 Batteries | 2 |
| Dual battery holder (parallel) | 1 |
| Mini PFM Step-Up Booster Module | 1 |
| TP4056 Charger Module (USB-C) | 1 |
| ON/OFF Switch | 1 |

### Sensors

| Component | Quantity |
|-----------|----------|
| 27mm Piezoelectric Discs | 5 |
| 1M Ohm Resistors | 5 |
| 5.1V Zener Diodes | 5 |
| Reed Switch + Magnet (pedal) | 1 |

### Buttons

| Component | Quantity |
|-----------|----------|
| Tactile Push Buttons | 5 |

### Visual Feedback

| Component | Quantity |
|-----------|----------|
| 5mm Diffused RGB LED (common anode) | 1 |
| 220 Ohm Resistors | 3 |

---

## Wiring Diagrams

### General Diagram

![Drum general diagram](./resource/diagram/drum/drumDiagram.png)

### Complete Pin Map

| Pro Micro Pin | Function | Direction | Notes |
|---------------|----------|-----------|-------|
| D2 | MUX S0 | OUTPUT | Channel control bit 0 |
| D3 | MUX S1 | OUTPUT | Channel control bit 1 |
| D4 | MUX S2 | OUTPUT | Channel control bit 2 |
| D5 | MUX S3 / Red LED | OUTPUT | Shared (PWM) |
| D6 | Green LED | OUTPUT | PWM |
| D7 | Select | INPUT_PULLUP | Menu button |
| D8 | Up | INPUT_PULLUP | Menu button |
| D9 | NRF24 CE | OUTPUT | Radio Chip Enable |
| D10 | NRF24 CSN | OUTPUT | Radio Chip Select |
| D14/MISO | NRF24 MISO | INPUT | SPI |
| D15/SCLK | NRF24 SCK | OUTPUT | SPI |
| D16/MOSI | NRF24 MOSI | OUTPUT | SPI |
| A0 | MUX SIG | INPUT | Analog signal from MUX |
| A1 | Down | INPUT_PULLUP | Menu button |
| A2 | Pair | INPUT_PULLUP | Pairing button |
| A3 | Blue LED | OUTPUT | Digital (no PWM) |
| A6 | Battery | INPUT | Voltage monitor |

---

## Power System

![Power diagram](./resource/diagram/battery/batteryDiagram.png)

### Connection Schematic

```
+------------------+
|  18650 BATTERIES |
|    (PARALLEL)    |
|                  |
|  (+)----+----(+) |
|         |        |
|  (-)----+----(-)	|
+--------+---------+
         |
         | B+ / B-
         v
+------------------+
|     TP4056       |
|    (Charger)     |
|                  |
|  USB-C <-- Charge|
|                  |
| OUT+ ----+       |
| OUT- ----|----+  |
+----------+----+--+
           |    |
           v    |
+----------+--+ |
|  SWITCH     | |
|  ON/OFF     | |
+------+------+ |
       |        |
       v        v
+------+--------+--+
|    Step-Up       |
|    Mini PFM      |
|                  |
|  IN+ <--- Switch |
|  IN- <--- OUT-   |
|                  |
|  OUT+ ---> 5V    |
|  OUT- ---> GND   |
+--------+---------+
         |
         v
+------------------+
|   PRO MICRO      |
|                  |
|  VCC <--- 5V     |
|  GND <--- GND    |
+------------------+
```

> **IMPORTANT**: Batteries go in PARALLEL (3.7V), NOT in series (7.4V).

### Battery Monitoring

Pin A6 reads voltage through a divider:

```cpp
#define BATTERY_CRITICAL_ADC  348   // ~3.4V - STOP
#define BATTERY_LOW_ADC       378   // ~3.7V - Warning
```

When voltage drops below 3.4V:
1. `isBatteryCritical = true` is activated
2. LED fixed RED
3. All transmission stops
4. Protects batteries from deep discharge

---

## LED System

![Drum LED Diagram](./resource/diagram/led/ledIndicatorDrum.png)

### RGB LED Connection (Common Anode)

On the Pro Micro, PWM pins are limited. Connection is different from Nano:

```
                 Pro Micro
                +---------+
                |         |
     +5V -------+-- VCC   |
                |         |
     LED Anode -+----+    |
     (common)   |    |    |
                |    |    |
     R (cathode)+--[220R]--D5  (PWM)
                |    |    |
     G (cathode)+--[220R]--D6  (PWM)
                |    |    |
     B (cathode)+--[220R]--A3  (Digital, no PWM)
                |         |
                +---------+
```

### Inverted Logic (Common Anode)

| PWM Value | LED State |
|-----------|-----------|
| 255 | Off |
| 0 | Maximum brightness |
| 128 | 50% brightness |

### Blue Pin Limitation

Pin A3 doesn't have PWM on Pro Micro. Code uses digital threshold:

```cpp
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(PIN_LED_R_ALT, r);  // D5 - PWM
  analogWrite(PIN_LED_G, g);       // D6 - PWM
  digitalWrite(PIN_LED_B, (b > 127) ? HIGH : LOW);  // A3 - Digital
}
```

### Player Colors

| ID | Color | RGB Display | PWM Common Anode |
|----|-------|-------------|------------------|
| 0 | White | (128,128,128) | (128,128,128) |
| P1 | Red | (255,0,0) | (0,255,255) |
| P2 | Blue | (0,0,255) | (255,255,0) |
| P3 | Green | (0,255,0) | (255,0,255) |
| P4 | Magenta | (255,0,255) | (0,255,0) |

### Animations

| Animation | Description | When |
|-----------|-------------|------|
| ANIM_WAVE | Smooth breathing 2s | Searching Hub |
| ANIM_MORPH | Color transition 1.5s | On connect |
| ANIM_HEARTBEAT | Subtle pulse every 5s | Standby |
| ANIM_SOLID | Fixed color | Playing |
| ANIM_DEAD_BATTERY | Fixed red | Critical battery |
| ANIM_HIT_FLASH | Brief white flash | On pad hit |

---

## CD74HC4067 Multiplexer

The multiplexer allows reading 16 analog inputs using only 5 pins.

![Multiplexer diagram](./resource/diagram/drum/muxDiagram.png)

### Physical Connection

```
CD74HC4067 (16-Channel MUX)
+---------------------------+
|                           |
|  VCC -------- 5V          |
|  GND -------- GND         |
|  EN  -------- GND         |  (Always enabled)
|                           |
|  S0  -------- D2          |  Control bits
|  S1  -------- D3          |
|  S2  -------- D4          |
|  S3  -------- D5          |
|                           |
|  SIG -------- A0          |  Analog signal
|                           |
|  C0  <------- Snare       |  Piezo inputs
|  C1  <------- Tom1        |
|  C2  <------- Tom2        |
|  C3  <------- Tom3/HiHat  |
|  C4  <------- Cymbal      |
|  C5  <------- Kick        |
|  C6-C15 ----- (free)      |  Future expansion
|                           |
+---------------------------+
```

### Channel Selection

The code selects each channel with S0-S3:

```cpp
void selectMuxChannel(uint8_t channel) {
  digitalWrite(MUX_S0, channel & 0x01);        // Bit 0
  digitalWrite(MUX_S1, (channel >> 1) & 0x01); // Bit 1
  digitalWrite(MUX_S2, (channel >> 2) & 0x01); // Bit 2
  digitalWrite(MUX_S3, (channel >> 3) & 0x01); // Bit 3
}
```

| Channel | S3 | S2 | S1 | S0 | Pad |
|---------|----|----|----|----|-----|
| 0 | 0 | 0 | 0 | 0 | Snare |
| 1 | 0 | 0 | 0 | 1 | Tom1 |
| 2 | 0 | 0 | 1 | 0 | Tom2 |
| 3 | 0 | 0 | 1 | 1 | Tom3 |
| 4 | 0 | 1 | 0 | 0 | Cymbal |
| 5 | 0 | 1 | 0 | 1 | Kick |

### Channel Reading

```cpp
uint16_t readMuxChannel(uint8_t channel) {
  selectMuxChannel(channel);
  delayMicroseconds(5);  // Stabilization time
  return analogRead(MUX_SIG);
}
```

---

## Piezo Circuit

Each piezoelectric disc needs a protection circuit.

![Piezo diagram](./resource/diagram/drum/piezoDiagram.png)

### Schematic per Pad

```
+-------------------+
|                   |
|    PIEZO DISC     |
|      27mm         |
|                   |
|   (+) ----+       |
|           |       |
|   (-) ----|---+   |
|           |   |   |
+-----------+---+---+
            |   |
            |   |
       +----+   +----+
       |             |
   +---+---+     +---+---+
   |  1M   |     | 5.1V  |
   | Ohm   |     | Zener |
   +---+---+     +---+---+
       |             |
       +------+------+
              |
              |
       -------+-------> To MUX channel (C0-C5)
              |
             GND
```

### Component Functions

| Component | Function |
|-----------|----------|
| **27mm Piezo** | Generates voltage when struck |
| **1M Ohm** | Discharges piezo between hits, prevents ghost readings |
| **5.1V Zener** | Limits maximum voltage to 5.1V, protects ADC |

> Piezos can generate spikes up to 50V. Without the Zener, you would damage the Arduino.

---

## Kick Pedal

You have two options for the pedal:

### Option A: Reed Switch (Recommended)

![Reed Switch Diagram](./resource/diagram/drum/reedSwitchDiagram.png)

```
      PEDAL (with magnet)
          |
          v
    +-----+-----+
    |   MAGNET  |
    +-----+-----+
          |
          | (Approaches when pressing)
          v
    +-----+-----+
    | REED      |
    | SWITCH    |----> MUX Channel C5
    +-----+-----+
          |
         GND
```

**Advantages:**
- No bouncing
- No additional circuit needed
- Very durable

**Code configuration:**
The Reed Switch acts as a digital button. When the magnet approaches, it closes the circuit.

### Option B: Piezo

Uses the same circuit as pads (1M + Zener).

**Advantages:**
- Detects hit intensity

**Disadvantages:**
- Needs a lot of padding
- More prone to false positives

---

## Code Architecture

### Libraries

```cpp
#include <SPI.h>      // SPI Communication
#include <RF24.h>     // NRF24L01 Driver (by TMRh20)
#include <EEPROM.h>   // Persistent storage
```

### Main Constants

```cpp
#define INSTRUMENT_TYPE     0x03      // Type: Drum
#define RF_CHANNEL          108       // Radio channel
#define SCAN_INTERVAL_US    500       // 0.5ms = 2000Hz scan
#define TX_INTERVAL_US      2000      // 2ms = 500Hz transmission
#define PIEZO_THRESHOLD_MIN 30        // Minimum to detect hit
#define PIEZO_COOLDOWN_MS   30        // Debounce between hits
```

### Circular Buffer

The system stores hits in a circular buffer to handle simultaneous hits:

```cpp
struct DrumHit {
  uint8_t pad;        // Which pad (0-5)
  uint8_t velocity;   // Force (0-255)
  uint32_t timestamp; // When it occurred
};

volatile DrumHit hitBuffer[8];
volatile uint8_t hitBufferHead = 0;
volatile uint8_t hitBufferTail = 0;
```

```
Circular buffer of 8 slots:

   tail                    head
     |                      |
     v                      v
+----+----+----+----+----+----+----+----+
| H0 | H1 | H2 |    |    |    |    |    |
+----+----+----+----+----+----+----+----+
  0    1    2    3    4    5    6    7

When full, discards the oldest.
```

### Main Loop

```cpp
void loop() {
  uint32_t nowMicros = micros();
  uint32_t nowMillis = millis();
  
  // PRIORITY 1: Scan piezos (2000Hz)
  if (nowMicros - lastScanTime >= SCAN_INTERVAL_US) {
    scanPiezos();
    lastScanTime = nowMicros;
  }
  
  // PRIORITY 2: Check battery (every 5s)
  if (nowMillis - lastBatteryCheck >= BATTERY_CHECK_MS) {
    // Read ADC and check threshold
  }
  
  // PRIORITY 3: Pairing button
  checkPairButton();
  
  // PRIORITY 4: Pairing mode
  if (isPairing) {
    handlePairing();
    return;
  }
  
  // PRIORITY 5: Transmit data (500Hz)
  if (isConnected && (nowMicros - lastTxTime >= TX_INTERVAL_US)) {
    transmitData();
    lastTxTime = nowMicros;
  }
  
  // PRIORITY 6: LED animation (50 FPS)
  if (nowMillis - lastAnimFrame >= ANIMATION_FRAME_MS) {
    updateAnimation();
    lastAnimFrame = nowMillis;
  }
}
```

### Piezo Scanning Flow

```
scanPiezos() @ 2000Hz
         |
         v
    +----+----+
    |  pad=0  |  (Snare)
    +----+----+
         |
   +-----+-----+
   |           |
cooldown?    no cooldown
   |           |
 skip          v
          +---------+
          | readMux |  Read analog value
          +----+----+
               |
          +----+----+
          |         |
        < 30      >= 30  (threshold)
          |         |
        skip        v
               +----------+
               | velocity |  ADC / 4
               +----+-----+
                    |
                    v
               +----------+
               |  buffer  |  Add hit
               +----+-----+
                    |
                    v
               +----------+
               | pending  |  Mark bit
               | PadHits  |
               +----------+
                    |
         +----------+----------+
         |                     |
         v                     v
    +----+-----+          +----+----+
    |  pad++   |          |  LED    |
    |  (0->5)  |          |  flash  |
    +----------+          +---------+
```

---

## Communication Protocol

### Packet Structure

**Data Packet (2 bytes)**

```
+---------------------------+---------------------------+
|          Byte 0           |          Byte 1           |
+---------------------------+---------------------------+
| Buttons + Pads (bitmap)   |         Reserved          |
+---------------------------+---------------------------+

Byte 0 - Bit mapping:
  Bit 7: Start
  Bit 6: Select
  Bit 5: Up
  Bit 4: Down
  Bit 3: Cymbal
  Bit 2: Tom3/HiHat
  Bit 1: Tom2
  Bit 0: Tom1

(Snare and Kick are mapped internally)
```

### Transmission Flow

```
transmitData() @ 500Hz
         |
         v
+------------------+
|   readButtons()  |  Read Start, Select, Up, Down
+--------+---------+
         |
         v
+------------------+
| Combine buttons  |
| + pendingPadHits |
+--------+---------+
         |
         v
+------------------+
|  radio.write()   |  Send 2 bytes
+--------+---------+
         |
    +----+----+
    |         |
 SUCCESS    FAIL
    |         |
    v         v
+-------+ +----------+
| read  | | Heartbeat|
| ACK   | | animation|
+---+---+ +----------+
    |
    v
+----------+
| Process  |
| color    |
+----------+
```

### ACK Payload (from Hub)

The Hub can send data in the ACK response:

```
+---------------------------+---------------------------+
|          Byte 0           |          Byte 1           |
+---------------------------+---------------------------+
|    [AnimID:4][ColorID:4]  |         Reserved          |
+---------------------------+---------------------------+
```

When receiving a new color:

```cpp
if (radio.isAckPayloadAvailable()) {
  uint8_t ackData[2];
  radio.read(ackData, 2);
  
  uint8_t colorId = ackData[0] & 0x0F;
  if (colorId != 0 && colorId != playerColor) {
    playerColor = colorId;
    targetColor = PLAYER_COLORS[colorId];
    setAnimation(ANIM_MORPH);  // Smooth transition
  }
}
```

---

## Pairing System

### EEPROM Storage

| Address | Size | Content |
|---------|------|---------|
| 0x00 | 2 bytes | Magic "GH" (0x4748) |
| 0x02 | 2 bytes | Unique Device ID |
| 0x04 | 1 byte | Player ID (1-4) |
| 0x05 | 1 byte | Assigned color |

### Pairing Process

```
1. Hold PAIR 3 seconds
         |
         v
2. isPairing = true
   LED: White Wave
         |
         v
3. Switch radio to PAIRING_PIPE
   Payload size = 4 bytes
         |
         v
4. Send every 100ms:
   [0xAA, 0x03, ID_Lo, ID_Hi]
         |
    +----+----+
    |         |
  timeout   response
  (30s)        |
    |          v
    v    +----------+
  exit   | Read ACK |
         | [PID,Col]|
         +----+-----+
              |
              v
         +----------+
         | Save to  |
         | EEPROM   |
         +----------+
              |
              v
         +----------+
         | Morph to |
         | color    |
         +----------+
              |
              v
         +----------+
         | Switch   |
         | to player|
         | pipe     |
         +----------+
```

### Pairing Code

```cpp
void handlePairing() {
  // 30 second timeout
  if (millis() - pairingStart >= 30000) {
    exitPairingMode();
    return;
  }
  
  // Build request
  uint8_t request[4] = {
    0xAA,                      // Magic
    INSTRUMENT_TYPE,           // 0x03 = Drum
    (uint8_t)(deviceId & 0xFF),
    (uint8_t)(deviceId >> 8)
  };
  
  // Send
  bool success = radio.write(request, 4);
  
  if (success && radio.isAckPayloadAvailable()) {
    uint8_t response[2];
    radio.read(response, 2);
    
    if (response[0] >= 1 && response[0] <= 4) {
      playerId = response[0];
      playerColor = response[1];
      saveSettings();
      // Animation and exit
    }
  }
  
  delay(100);  // Wait before retry
}
```

---

## Calibration

### Adjusting Sensitivity

In file `drum_controller.ino`:

```cpp
// Line 85: Minimum detection threshold
#define PIEZO_THRESHOLD_MIN   30
// Increase if getting false hits
// Decrease if not detecting soft hits

// Line 87: Time between hits (debounce)
#define PIEZO_COOLDOWN_MS     30
// Increase if getting double hits
// Decrease if missing fast hits

// Line 88: Velocity scale
#define PIEZO_VELOCITY_SCALE  4
// Increase for less sensitivity
// Decrease for more sensitivity
```

### Recommended Values

| Play Style | THRESHOLD | COOLDOWN | SCALE |
|------------|-----------|----------|-------|
| Casual | 40 | 40 | 4 |
| Normal | 30 | 30 | 4 |
| Competitive | 20 | 20 | 3 |

---

## Upload Code

### Requirements

1. Arduino IDE installed
2. **RF24 by TMRh20** library (Library Manager)
3. **Arduino Pro Micro** board or compatible

### Steps

1. Open `drum_controller.ino`
2. Select board: **Arduino Leonardo** or **SparkFun Pro Micro**
3. Select processor: **ATmega32U4 (5V, 16MHz)**
4. Select COM port
5. Click **Upload**

> The Pro Micro uses the same bootloader as Leonardo.

### Verify Functionality

1. LED should do white Wave on power up
2. If blinking red fast: radio problem
3. If Wave works: ready to pair

---

## Troubleshooting

| Problem | Probable Cause | Solution |
|---------|----------------|----------|
| LED blinks red fast | Radio not initializing | Check SPI and 3.3V adapter |
| Doesn't detect hits | Piezo poorly connected | Check soldering and 1M resistor |
| Constant false hits | Electrical noise | Add Zener, increase threshold |
| Double hit on each strike | Short cooldown | Increase PIEZO_COOLDOWN_MS |
| Missing fast hits | Long cooldown | Decrease PIEZO_COOLDOWN_MS |
| Pedal not working | Magnet poorly positioned | Adjust magnet-reed distance |
| Blue LED not working | Pin A3 no PWM | Normal, only ON/OFF |
| Won't pair | Hub not in pairing mode | Hold PAIR on Hub 5 sec first |
| Battery drains fast | Inefficient Step-Up | Check PFM module |

---

## Future Expansion

The multiplexer has 10 free channels (C6-C15):

| Channel | Possible Use |
|---------|--------------|
| C6 | Closed Hi-Hat |
| C7 | Open Hi-Hat |
| C8 | Ride |
| C9 | Additional Crash |
| C10-C15 | Extra pads, potentiometers |

---

## License

This code is part of the Guitar Hero Band Controller project. See [LICENSE](../LICENSE) for details.
