# Guitar / Bass Controller

<p align="center">
  <img src="https://img.shields.io/badge/MCU-Arduino_Nano-00979D?style=for-the-badge&logo=arduino&logoColor=white" alt="Arduino"/>
  <img src="https://img.shields.io/badge/LATENCY-%3C2ms-00ff00?style=for-the-badge" alt="Latency"/>
  <img src="https://img.shields.io/badge/TX_RATE-500Hz-blue?style=for-the-badge" alt="TX Rate"/>
</p>

Lightweight and fast design. Uses direct port reading for zero latency.

---

## Bill of Materials

### Brain

| Component | Quantity |
|-----------|----------|
| Arduino Nano V3 (CH340) | 1 |

### Communication

| Component | Quantity |
|-----------|----------|
| NRF24L01 Module (onboard antenna) | 1 |
| Voltage adapter (8-pin socket) | 1 | 

### Power (Parallel Rechargeable System)

| Component | Quantity |
|-----------|----------|
| Li-Ion 18650 Batteries | 2 |
| Dual battery holder | 1 | 
| Mini PFM Step-Up Booster Module | 1 | 
| TP4056 Charger Module | 1 | 
| ON/OFF Slide Switch | 1 |

### Controls (11 buttons total)

| Component | Quantity | Notes |
|-----------|----------|-------|
| Arcade/mechanical buttons | 5 | Green, Red, Yellow, Blue, Orange |
| Push buttons (any type) | 5 | Start, Select, Whammy, Up, Down |
| Push button (any type) | 1 | Pair/Binding |

### Visual Feedback

| Component | Quantity | Notes |
|-----------|----------|-------|
| 5mm or 10mm diffused RGB LED | 1 | **COMMON ANODE** |
| 220 Ohm Resistors | 3 | One per R, G, B channel |

---

## Wiring Diagram

![Controller wiring diagram](./resource/diagram/strings/stringDiagram.png)

### Pin Map

| Arduino Pin | Function | Type |
|-------------|----------|------|
| D2 | Green Button | INPUT_PULLUP |
| D4 | Red Button | INPUT_PULLUP |
| D7 | Yellow Button | INPUT_PULLUP |
| D8 | Blue Button | INPUT_PULLUP |
| A3 | Orange Button | INPUT_PULLUP |
| A4 | Strum Up | INPUT_PULLUP |
| A5 | Strum Down | INPUT_PULLUP |
| A2 | Start | INPUT_PULLUP |
| A1 | Pair Button | INPUT_PULLUP |
| A0 | Whammy Bar | Analog |
| A7 | Battery Monitor | Analog |
| D3 | Red LED | PWM Output |
| D5 | Green LED | PWM Output |
| D6 | Blue LED | PWM Output |
| D9 | NRF24 CE | Output |
| D10 | NRF24 CSN | Output |
| D11 | NRF24 MOSI | SPI |
| D12 | NRF24 MISO | SPI |
| D13 | NRF24 SCK | SPI |

---

## Power System

![Battery diagram](./resource/diagram/battery/batteryDiagram.png)

### Power Flow

```
18650 Batteries (x2 PARALLEL)
         |
         v
    +---------+
    | TP4056  | <-- USB-C (Charging)
    | B+ / B- |
    +----+----+
         | OUT+
         v
    +---------+
    | Switch  | ON/OFF
    +----+----+
         |
         v
    +---------+
    | Step-Up | Mini PFM
    | IN+ IN- |
    +----+----+
         | OUT+ (5V) / OUT- (GND)
         v
    +---------+
    | Arduino | Pin 5V / Pin GND
    +---------+
```

> **IMPORTANT**: Batteries go in PARALLEL (3.7V), NOT in series (7.4V). Series would damage the Arduino.

---

## RGB LED (Common Anode)

![LED Indicator](./resource/diagram/led/ledIndicator.png)

### Connection

Common anode RGB LED has inverted logic:
- **255 = LED off**
- **0 = LED at maximum brightness**

```
         +-------- R (Cathode) --[220R]-- D3
    5V --+-------- G (Cathode) --[220R]-- D5
         +-------- B (Cathode) --[220R]-- D6
              ^
           Anode (+)
```

### Player Colors

| ID | Color | RGB (Display) | PWM (Common Anode) |
|----|-------|---------------|-------------------|
| P1 | Red | (255, 0, 0) | (0, 255, 255) |
| P2 | Blue | (0, 0, 255) | (255, 255, 0) |
| P3 | Green | (0, 255, 0) | (255, 0, 255) |
| P4 | Magenta | (255, 0, 255) | (0, 255, 0) |

---

## Code Architecture

### Required Libraries

```cpp
#include <SPI.h>      // SPI Communication with NRF24
#include <RF24.h>     // Radio module driver (by TMRh20)
#include <EEPROM.h>   // Persistent storage
```

### EEPROM Data Structure

The controller saves its configuration in the Arduino's EEPROM memory:

| Address | Size | Content |
|---------|------|---------|
| 0x00 | 2 bytes | Magic number (0x4748 = "GH") |
| 0x02 | 2 bytes | Device ID (unique per controller) |
| 0x04 | 1 byte | Player ID (1-4, 0 = unpaired) |
| 0x05 | 1 byte | Assigned color |

### First Power On

```
1. Check if magic number "GH" exists in EEPROM
   |
   +-- Does NOT exist:
   |     - Generate random Device ID (0x1000-0xFFFF)
   |     - Save to EEPROM
   |     - Start white "Wave" animation
   |
   +-- EXISTS:
         - Load Device ID, Player ID and Color from EEPROM
         - If has valid Player ID (1-4):
           - Configure corresponding radio pipe
           - Start "Wave" animation with its color
         - If no Player ID:
           - Start white "Wave" animation
```

---

## Communication Protocol

### Packet Structure

**Data Packet (Normal)**: 2 bytes

```
+------------------+------------------+
|     Byte 0       |     Byte 1       |
+------------------+------------------+
| Buttons (bits)   | Whammy (0-255)   |
+------------------+------------------+

Byte 0 - Bit mapping:
  Bit 7: Start
  Bit 6: Orange
  Bit 5: Blue
  Bit 4: Yellow
  Bit 3: Red
  Bit 2: Green
  Bit 1: Strum Down
  Bit 0: Strum Up
```

**Pairing Packet**: 4 bytes

```
+--------+--------+--------+--------+
| Byte 0 | Byte 1 | Byte 2 | Byte 3 |
+--------+--------+--------+--------+
|  0xAA  |  Type  | ID Low | ID Hi  |
| Magic  | Instr. |   Device ID     |
+--------+--------+--------+--------+
```

**ACK Payload (from Hub)**: 2 bytes

```
+------------------+------------------+
|     Byte 0       |     Byte 1       |
+------------------+------------------+
| [Anim:4|Color:4] |    Reserved      |
+------------------+------------------+

Anim ID (upper 4 bits):
  0x0 = No change
  0x1 = Wave
  0x2 = Morph
  0x3 = Heartbeat
  0x4 = Solid

Color ID (lower 4 bits):
  0x1 = Red (P1)
  0x2 = Blue (P2)
  0x3 = Green (P3)
  0x4 = Magenta (P4)
```

---

## Transmission Flow (500 Hz)

```
loop() runs every 2ms (500 Hz):
         |
         v
+-------------------+
| readButtonsFast() |  Reads all buttons
+--------+----------+
         |
         v
+-------------------+
|   readWhammy()    |  Reads potentiometer (0-255)
+--------+----------+
         |
         v
+-------------------+
| Build 2-byte      |  [buttons, whammy]
| packet            |
+--------+----------+
         |
         v
+-------------------+
| radio.write()     |  Send to Hub
+--------+----------+
         |
    +----+----+
    |         |
  SUCCESS   FAIL
    |         |
    v         v
+-------+  +------------+
| Check |  | Heartbeat  |
| ACK   |  | animation  |
+---+---+  +------------+
    |
    v
+-------------------+
| processAckPayload |  Receive color/animation
+-------------------+
```

---

## Pairing System

### How to Pair

1. **Hold** the PAIR button for **3 seconds**
2. LED starts **Wave** animation (pulsing white)
3. Controller sends requests on pairing channel
4. When Hub responds, it assigns:
   - Player ID (1-4)
   - Corresponding color
5. Controller saves to EEPROM
6. LED does **Morph** to assigned color
7. Ready to play

### Pairing Flow

```
[CONTROLLER]                          [HUB]
     |                                   |
     | PAIR button 3 sec                 |
     |---------------------------------->|
     |                                   |
     | LED: White Wave                   |
     |                                   |
     | Packet: [0xAA, Type, ID_Lo, ID_Hi]|
     |---------------------------------->|
     |                                   |
     |        ACK: [PlayerID, Color]     |
     |<----------------------------------|
     |                                   |
     | Save to EEPROM                    |
     | LED: Morph -> Color               |
     |                                   |
     | Switch to player pipe             |
     |---------------------------------->|
     |                                   |
     |       Normal transmission         |
     |<=================================>|
```

### Timeout

- Pairing window: **30 seconds**
- If no response received, returns to previous state

---

## Animation System

### Animation Types

| ID | Name | Behavior | When Used |
|----|------|----------|-----------|
| ANIM_WAVE | Wave | Smooth fade in/out (2s cycle) | Searching Hub / Pairing |
| ANIM_MORPH | Transition | Linear color change (1.5s) | On connect |
| ANIM_HEARTBEAT | Heartbeat | Subtle pulse every 5s | Standby / Idle |
| ANIM_SOLID | Solid | Fixed color 100% | Actively playing |
| ANIM_DEAD_BATTERY | Alarm | Fixed red | Battery < 3.4V |

### State Machine

```cpp
void updateAnimation() {
  switch (currentAnim) {
    case ANIM_WAVE:
      // Breathing: brightness 0% -> 100% -> 0% in 2 seconds
      break;
      
    case ANIM_MORPH:
      // Linear interpolation between colors in 1.5s
      break;
      
    case ANIM_HEARTBEAT:
      // 80% base brightness + subtle pulse every 5s
      break;
      
    case ANIM_SOLID:
      // Fixed color at 100%
      break;
      
    case ANIM_DEAD_BATTERY:
      // Solid red, blocks everything
      break;
  }
}
```

---

## Battery Monitoring

### Thresholds

| Voltage | ADC (approx) | State | Action |
|---------|--------------|-------|--------|
| > 3.9V | > 400 | OK | Normal |
| 3.7V - 3.9V | 378-400 | Low | Warning |
| < 3.4V | < 348 | Critical | **STOPS TRANSMISSION** |

### Critical Behavior

When battery drops below 3.4V:

1. `isBatteryCritical = true`
2. LED fixed **RED**
3. **Stops transmitting** (saves power)
4. Loop only shows red LED

> This protects 18650 batteries from deep discharge.

---

## Radio Configuration

```cpp
radio.setChannel(108);           // Fixed channel (avoids WiFi)
radio.setDataRate(RF24_2MBPS);   // Maximum speed
radio.setPALevel(RF24_PA_HIGH);  // High power
radio.setPayloadSize(2);         // Ultralight packets
radio.setAutoAck(true);          // Automatic ACK
radio.enableAckPayload();        // Receive data in ACK
radio.setCRCLength(RF24_CRC_8);  // Error checking
```

### Communication Pipes

| Pipe | Address | Use |
|------|---------|-----|
| Pairing | 0xE8E8PAIR00 | Public pairing channel |
| Player 1 | 0xE8E8F0F001 | P1 communication (retry 250us) |
| Player 2 | 0xE8E8F0F002 | P2 communication (retry 500us) |
| Player 3 | 0xE8E8F0F003 | P3 communication (retry 750us) |
| Player 4 | 0xE8E8F0F004 | P4 communication (retry 1000us) |

> Staggered delays prevent collisions between instruments.

---

## Upload Code

### Requirements

1. Arduino IDE installed
2. **RF24 by TMRh20** library installed

### Steps

1. Open `string_controller.ino` in Arduino IDE
2. Select board: **Arduino Nano**
3. Select processor: **ATmega328P** or **ATmega328P (Old Bootloader)**
4. Select correct COM port
5. Click **Upload**

### Configure Instrument Type

On line 30 of the code, change as needed:

```cpp
#define INSTRUMENT_TYPE   INST_GUITAR   // For guitar
// or
#define INSTRUMENT_TYPE   INST_BASS     // For bass
```

---

## Troubleshooting

| Problem | Cause | Solution |
|---------|-------|----------|
| LED blinks red fast | Radio not initializing | Check SPI connections and voltage adapter |
| LED fixed red | Critical battery | Charge the batteries |
| Won't pair | Hub not in pairing mode | Hold PAIR on Hub for 5 sec first |
| Buttons not responding | Loose connection | Check solder joints with multimeter |
| Whammy not working | Damaged potentiometer | Test with another potentiometer |

---

## License

This code is part of the Guitar Hero Band Controller project. See [LICENSE](../LICENSE) for details.