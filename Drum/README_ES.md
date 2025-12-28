# Controlador de Bateria / Drums

<p align="center">
  <img src="https://img.shields.io/badge/MCU-Arduino_Pro_Micro-00979D?style=for-the-badge&logo=arduino&logoColor=white" alt="Pro Micro"/>
  <img src="https://img.shields.io/badge/SCAN_RATE-2000Hz-00ff00?style=for-the-badge" alt="Scan Rate"/>
  <img src="https://img.shields.io/badge/TX_RATE-500Hz-blue?style=for-the-badge" alt="TX Rate"/>
  <img src="https://img.shields.io/badge/PADS-5_Piezo-yellow?style=for-the-badge" alt="Pads"/>
</p>

Diseno robusto para manejar multiples sensores analogicos simultaneamente sin bloqueos.
Utiliza un multiplexor de 16 canales para leer todos los piezos con un solo pin analogico.

---

## Tabla de Contenidos

1. [Lista de Componentes](#lista-de-componentes)
2. [Diagramas de Conexion](#diagramas-de-conexion)
3. [Sistema de Energia](#sistema-de-energia)
4. [Sistema LED](#sistema-led)
5. [Multiplexor CD74HC4067](#multiplexor-cd74hc4067)
6. [Circuito de Piezos](#circuito-de-piezos)
7. [Pedal de Bombo](#pedal-de-bombo)
8. [Arquitectura del Codigo](#arquitectura-del-codigo)
9. [Protocolo de Comunicacion](#protocolo-de-comunicacion)
10. [Sistema de Emparejamiento](#sistema-de-emparejamiento)
11. [Calibracion](#calibracion)
12. [Subir el Codigo](#subir-el-codigo)
13. [Solucion de Problemas](#solucion-de-problemas)

---

## Lista de Componentes

### Cerebro

| Componente | Cantidad |
|------------|----------|
| Arduino Pro Micro (ATmega32U4) | 1 |

### Expansion

| Componente | Cantidad |
|------------|----------|
| Multiplexor CD74HC4067 (16 canales) | 1 |

### Comunicacion

| Componente | Cantidad |
|------------|----------|
| Modulo NRF24L01 (antena en placa) | 1 |
| Adaptador de voltaje (zocalo 8 pines) | 1 |

### Energia

| Componente | Cantidad |
|------------|----------|
| Baterias Li-Ion 18650 | 2 |
| Soporte de bateria doble (paralelo) | 1 |
| Modulo elevador Step-Up Mini PFM | 1 |
| Modulo cargador TP4056 (USB-C) | 1 |
| Interruptor ON/OFF | 1 |

### Sensores

| Componente | Cantidad |
|------------|----------|
| Discos piezoelectricos 27mm | 5 |
| Resistencias 1M Ohm | 5 |
| Diodos Zener 5.1V | 5 |
| Reed Switch + Iman (pedal) | 1 |

### Botones

| Componente | Cantidad |
|------------|----------|
| Pulsadores tactiles | 5 |

### Feedback Visual

| Componente | Cantidad |
|------------|----------|
| LED RGB difuso 5mm (anodo comun) | 1 |
| Resistencias 220 Ohm | 3 |

---

## Diagramas de Conexion

### Diagrama General

![Diagrama general del drum](./resource/diagram/drum/drumDiagram.png)

### Mapa de Pines Completo

| Pin Pro Micro | Funcion | Direccion | Notas |
|---------------|---------|-----------|-------|
| D2 | MUX S0 | OUTPUT | Control de canal bit 0 |
| D3 | MUX S1 | OUTPUT | Control de canal bit 1 |
| D4 | MUX S2 | OUTPUT | Control de canal bit 2 |
| D5 | MUX S3 / LED Rojo | OUTPUT | Compartido (PWM) |
| D6 | LED Verde | OUTPUT | PWM |
| D7 | Select | INPUT_PULLUP | Boton menu |
| D8 | Up | INPUT_PULLUP | Boton menu |
| D9 | NRF24 CE | OUTPUT | Chip Enable radio |
| D10 | NRF24 CSN | OUTPUT | Chip Select radio |
| D14/MISO | NRF24 MISO | INPUT | SPI |
| D15/SCLK | NRF24 SCK | OUTPUT | SPI |
| D16/MOSI | NRF24 MOSI | OUTPUT | SPI |
| A0 | MUX SIG | INPUT | Senal analogica del MUX |
| A1 | Down | INPUT_PULLUP | Boton menu |
| A2 | Pair | INPUT_PULLUP | Boton emparejamiento |
| A3 | LED Azul | OUTPUT | Digital (sin PWM) |
| A6 | Bateria | INPUT | Monitor de voltaje |

---

## Sistema de Energia

![Diagrama de energia](./resource/diagram/battery/batteryDiagram.png)

### Esquema de Conexion

```
+------------------+
|  BATERIAS 18650  |
|   (PARALELO)     |
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
|   (Cargador)     |
|                  |
|  USB-C <-- Carga |
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

> **IMPORTANTE**: Las baterias van en PARALELO (3.7V), NO en serie (7.4V).

### Monitoreo de Bateria

El pin A6 lee el voltaje a traves de un divisor:

```cpp
#define BATTERY_CRITICAL_ADC  348   // ~3.4V - STOP
#define BATTERY_LOW_ADC       378   // ~3.7V - Warning
```

Cuando el voltaje cae bajo 3.4V:
1. Se activa `isBatteryCritical = true`
2. LED fijo en ROJO
3. Se detiene toda transmision
4. Protege las baterias de descarga profunda

---

## Sistema LED

![Diagrama LED Drum](./resource/diagram/led/ledIndicatorDrum.png)

### Conexion del LED RGB (Anodo Comun)

En el Pro Micro, los pines PWM son limitados. La conexion es diferente al Nano:

```
                 Pro Micro
                +---------+
                |         |
     +5V -------+-- VCC   |
                |         |
     LED Anodo -+----+    |
     (comun)    |    |    |
                |    |    |
     R (catodo) +--[220R]--D5  (PWM)
                |    |    |
     G (catodo) +--[220R]--D6  (PWM)
                |    |    |
     B (catodo) +--[220R]--A3  (Digital, sin PWM)
                |         |
                +---------+
```

### Logica Invertida (Anodo Comun)

| Valor PWM | Estado LED |
|-----------|------------|
| 255 | Apagado |
| 0 | Maximo brillo |
| 128 | 50% brillo |

### Limitacion del Pin Azul

El pin A3 no tiene PWM en el Pro Micro. El codigo usa un umbral digital:

```cpp
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(PIN_LED_R_ALT, r);  // D5 - PWM
  analogWrite(PIN_LED_G, g);       // D6 - PWM
  digitalWrite(PIN_LED_B, (b > 127) ? HIGH : LOW);  // A3 - Digital
}
```

### Colores por Jugador

| ID | Color | RGB Display | PWM Anodo Comun |
|----|-------|-------------|-----------------|
| 0 | Blanco | (128,128,128) | (128,128,128) |
| P1 | Rojo | (255,0,0) | (0,255,255) |
| P2 | Azul | (0,0,255) | (255,255,0) |
| P3 | Verde | (0,255,0) | (255,0,255) |
| P4 | Magenta | (255,0,255) | (0,255,0) |

### Animaciones

| Animacion | Descripcion | Cuando |
|-----------|-------------|--------|
| ANIM_WAVE | Respiracion suave 2s | Buscando Hub |
| ANIM_MORPH | Transicion de color 1.5s | Al conectar |
| ANIM_HEARTBEAT | Pulso sutil cada 5s | Standby |
| ANIM_SOLID | Color fijo | Jugando |
| ANIM_DEAD_BATTERY | Rojo fijo | Bateria critica |
| ANIM_HIT_FLASH | Flash blanco breve | Al golpear pad |

---

## Multiplexor CD74HC4067

El multiplexor permite leer 16 entradas analogicas usando solo 5 pines.

![Diagrama del multiplexor](./resource/diagram/drum/muxDiagram.png)

### Conexion Fisica

```
CD74HC4067 (16-Channel MUX)
+---------------------------+
|                           |
|  VCC -------- 5V          |
|  GND -------- GND         |
|  EN  -------- GND         |  (Siempre habilitado)
|                           |
|  S0  -------- D2          |  Control bits
|  S1  -------- D3          |
|  S2  -------- D4          |
|  S3  -------- D5          |
|                           |
|  SIG -------- A0          |  Senal analogica
|                           |
|  C0  <------- Snare       |  Entradas de piezos
|  C1  <------- Tom1        |
|  C2  <------- Tom2        |
|  C3  <------- Tom3/HiHat  |
|  C4  <------- Cymbal      |
|  C5  <------- Kick        |
|  C6-C15 ----- (libre)     |  Expansion futura
|                           |
+---------------------------+
```

### Seleccion de Canal

El codigo selecciona cada canal con S0-S3:

```cpp
void selectMuxChannel(uint8_t channel) {
  digitalWrite(MUX_S0, channel & 0x01);        // Bit 0
  digitalWrite(MUX_S1, (channel >> 1) & 0x01); // Bit 1
  digitalWrite(MUX_S2, (channel >> 2) & 0x01); // Bit 2
  digitalWrite(MUX_S3, (channel >> 3) & 0x01); // Bit 3
}
```

| Canal | S3 | S2 | S1 | S0 | Pad |
|-------|----|----|----|----|-----|
| 0 | 0 | 0 | 0 | 0 | Snare |
| 1 | 0 | 0 | 0 | 1 | Tom1 |
| 2 | 0 | 0 | 1 | 0 | Tom2 |
| 3 | 0 | 0 | 1 | 1 | Tom3 |
| 4 | 0 | 1 | 0 | 0 | Cymbal |
| 5 | 0 | 1 | 0 | 1 | Kick |

### Lectura de Canal

```cpp
uint16_t readMuxChannel(uint8_t channel) {
  selectMuxChannel(channel);
  delayMicroseconds(5);  // Tiempo de estabilizacion
  return analogRead(MUX_SIG);
}
```

---

## Circuito de Piezos

Cada disco piezoelectrico necesita un circuito de proteccion.

![Diagrama de piezo](./resource/diagram/drum/piezoDiagram.png)

### Esquema por Cada Pad

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
       -------+-------> Al canal del MUX (C0-C5)
              |
             GND
```

### Funcion de Cada Componente

| Componente | Funcion |
|------------|---------|
| **Piezo 27mm** | Genera voltaje al ser golpeado |
| **1M Ohm** | Descarga el piezo entre golpes, evita lecturas fantasma |
| **Zener 5.1V** | Limita el voltaje maximo a 5.1V, protege el ADC |

> Los piezos pueden generar picos de hasta 50V. Sin el Zener, danarias el Arduino.

---

## Pedal de Bombo

Tienes dos opciones para el pedal:

### Opcion A: Reed Switch (Recomendado)

![Diagrama Reed Switch](./resource/diagram/drum/reedSwitchDiagram.png)

```
      PEDAL (con iman)
          |
          v
    +-----+-----+
    |   IMAN    |
    +-----+-----+
          |
          | (Se acerca al presionar)
          v
    +-----+-----+
    | REED      |
    | SWITCH    |----> Canal C5 del MUX
    +-----+-----+
          |
         GND
```

**Ventajas:**
- Sin rebotes
- No necesita circuito adicional
- Muy duradero

**Configuracion en codigo:**
El Reed Switch actua como boton digital. Cuando el iman se acerca, cierra el circuito.

### Opcion B: Piezo

Usa el mismo circuito que los pads (1M + Zener).

**Ventajas:**
- Detecta intensidad del golpe

**Desventajas:**
- Necesita mucho acolchado
- Mas propenso a falsos positivos

---

## Arquitectura del Codigo

### Librerias

```cpp
#include <SPI.h>      // Comunicacion SPI
#include <RF24.h>     // Driver NRF24L01 (by TMRh20)
#include <EEPROM.h>   // Almacenamiento persistente
```

### Constantes Principales

```cpp
#define INSTRUMENT_TYPE     0x03      // Tipo: Drum
#define RF_CHANNEL          108       // Canal de radio
#define SCAN_INTERVAL_US    500       // 0.5ms = 2000Hz scan
#define TX_INTERVAL_US      2000      // 2ms = 500Hz transmision
#define PIEZO_THRESHOLD_MIN 30        // Minimo para detectar golpe
#define PIEZO_COOLDOWN_MS   30        // Debounce entre golpes
```

### Buffer Circular

El sistema almacena golpes en un buffer circular para manejar hits simultaneos:

```cpp
struct DrumHit {
  uint8_t pad;        // Cual pad (0-5)
  uint8_t velocity;   // Fuerza (0-255)
  uint32_t timestamp; // Cuando ocurrio
};

volatile DrumHit hitBuffer[8];
volatile uint8_t hitBufferHead = 0;
volatile uint8_t hitBufferTail = 0;
```

```
Buffer circular de 8 slots:

   tail                    head
     |                      |
     v                      v
+----+----+----+----+----+----+----+----+
| H0 | H1 | H2 |    |    |    |    |    |
+----+----+----+----+----+----+----+----+
  0    1    2    3    4    5    6    7

Cuando esta lleno, descarta el mas antiguo.
```

### Loop Principal

```cpp
void loop() {
  uint32_t nowMicros = micros();
  uint32_t nowMillis = millis();
  
  // PRIORIDAD 1: Escanear piezos (2000Hz)
  if (nowMicros - lastScanTime >= SCAN_INTERVAL_US) {
    scanPiezos();
    lastScanTime = nowMicros;
  }
  
  // PRIORIDAD 2: Verificar bateria (cada 5s)
  if (nowMillis - lastBatteryCheck >= BATTERY_CHECK_MS) {
    // Leer ADC y verificar umbral
  }
  
  // PRIORIDAD 3: Boton de pairing
  checkPairButton();
  
  // PRIORIDAD 4: Modo pairing
  if (isPairing) {
    handlePairing();
    return;
  }
  
  // PRIORIDAD 5: Transmitir datos (500Hz)
  if (isConnected && (nowMicros - lastTxTime >= TX_INTERVAL_US)) {
    transmitData();
    lastTxTime = nowMicros;
  }
  
  // PRIORIDAD 6: Animacion LED (50 FPS)
  if (nowMillis - lastAnimFrame >= ANIMATION_FRAME_MS) {
    updateAnimation();
    lastAnimFrame = nowMillis;
  }
}
```

### Flujo de Escaneo de Piezos

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
          | readMux |  Lee valor analogico
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
               |  buffer  |  Agregar hit
               +----+-----+
                    |
                    v
               +----------+
               | pending  |  Marcar bit
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

## Protocolo de Comunicacion

### Estructura del Paquete

**Paquete de Datos (2 bytes)**

```
+---------------------------+---------------------------+
|          Byte 0           |          Byte 1           |
+---------------------------+---------------------------+
| Botones + Pads (bitmap)   |         Reservado         |
+---------------------------+---------------------------+

Byte 0 - Mapeo de bits:
  Bit 7: Start
  Bit 6: Select
  Bit 5: Up
  Bit 4: Down
  Bit 3: Cymbal
  Bit 2: Tom3/HiHat
  Bit 1: Tom2
  Bit 0: Tom1

(Snare y Kick se mapean internamente)
```

### Flujo de Transmision

```
transmitData() @ 500Hz
         |
         v
+------------------+
|   readButtons()  |  Lee Start, Select, Up, Down
+--------+---------+
         |
         v
+------------------+
| Combinar botones |
| + pendingPadHits |
+--------+---------+
         |
         v
+------------------+
|  radio.write()   |  Envia 2 bytes
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
| Procesar |
| color    |
+----------+
```

### ACK Payload (del Hub)

El Hub puede enviar datos en la respuesta ACK:

```
+---------------------------+---------------------------+
|          Byte 0           |          Byte 1           |
+---------------------------+---------------------------+
|    [AnimID:4][ColorID:4]  |         Reservado         |
+---------------------------+---------------------------+
```

Cuando recibe un nuevo color:

```cpp
if (radio.isAckPayloadAvailable()) {
  uint8_t ackData[2];
  radio.read(ackData, 2);
  
  uint8_t colorId = ackData[0] & 0x0F;
  if (colorId != 0 && colorId != playerColor) {
    playerColor = colorId;
    targetColor = PLAYER_COLORS[colorId];
    setAnimation(ANIM_MORPH);  // Transicion suave
  }
}
```

---

## Sistema de Emparejamiento

### Almacenamiento en EEPROM

| Direccion | Tamano | Contenido |
|-----------|--------|-----------|
| 0x00 | 2 bytes | Magic "GH" (0x4748) |
| 0x02 | 2 bytes | Device ID unico |
| 0x04 | 1 byte | Player ID (1-4) |
| 0x05 | 1 byte | Color asignado |

### Proceso de Pairing

```
1. Mantener PAIR 3 segundos
         |
         v
2. isPairing = true
   LED: Wave blanco
         |
         v
3. Cambiar radio a PAIRING_PIPE
   Payload size = 4 bytes
         |
         v
4. Enviar cada 100ms:
   [0xAA, 0x03, ID_Lo, ID_Hi]
         |
    +----+----+
    |         |
  timeout   respuesta
  (30s)        |
    |          v
    v    +----------+
  exit   | Leer ACK |
         | [PID,Col]|
         +----+-----+
              |
              v
         +----------+
         | Guardar  |
         | EEPROM   |
         +----------+
              |
              v
         +----------+
         | Morph a  |
         | color    |
         +----------+
              |
              v
         +----------+
         | Cambiar  |
         | a pipe   |
         | jugador  |
         +----------+
```

### Codigo de Pairing

```cpp
void handlePairing() {
  // Timeout de 30 segundos
  if (millis() - pairingStart >= 30000) {
    exitPairingMode();
    return;
  }
  
  // Construir solicitud
  uint8_t request[4] = {
    0xAA,                      // Magic
    INSTRUMENT_TYPE,           // 0x03 = Drum
    (uint8_t)(deviceId & 0xFF),
    (uint8_t)(deviceId >> 8)
  };
  
  // Enviar
  bool success = radio.write(request, 4);
  
  if (success && radio.isAckPayloadAvailable()) {
    uint8_t response[2];
    radio.read(response, 2);
    
    if (response[0] >= 1 && response[0] <= 4) {
      playerId = response[0];
      playerColor = response[1];
      saveSettings();
      // Animacion y salir
    }
  }
  
  delay(100);  // Esperar antes de reintentar
}
```

---

## Calibracion

### Ajustar Sensibilidad

En el archivo `drum_controller.ino`:

```cpp
// Linea 85: Umbral minimo de deteccion
#define PIEZO_THRESHOLD_MIN   30
// Aumentar si hay golpes falsos
// Reducir si no detecta golpes suaves

// Linea 87: Tiempo entre golpes (debounce)
#define PIEZO_COOLDOWN_MS     30
// Aumentar si hay golpes dobles
// Reducir si pierde golpes rapidos

// Linea 88: Escala de velocidad
#define PIEZO_VELOCITY_SCALE  4
// Aumentar para menos sensibilidad
// Reducir para mas sensibilidad
```

### Valores Recomendados

| Estilo de juego | THRESHOLD | COOLDOWN | SCALE |
|-----------------|-----------|----------|-------|
| Casual | 40 | 40 | 4 |
| Normal | 30 | 30 | 4 |
| Competitivo | 20 | 20 | 3 |

---

## Subir el Codigo

### Requisitos

1. Arduino IDE instalado
2. Libreria **RF24 by TMRh20** (Library Manager)
3. Placa **Arduino Pro Micro** o compatible

### Pasos

1. Abrir `drum_controller.ino`
2. Seleccionar placa: **Arduino Leonardo** o **SparkFun Pro Micro**
3. Seleccionar procesador: **ATmega32U4 (5V, 16MHz)**
4. Seleccionar puerto COM
5. Click **Upload**

> El Pro Micro usa el mismo bootloader que el Leonardo.

### Verificar Funcionamiento

1. LED debe hacer Wave blanco al encender
2. Si parpadea rojo rapido: problema con radio
3. Si Wave funciona: listo para emparejar

---

## Solucion de Problemas

| Problema | Causa Probable | Solucion |
|----------|----------------|----------|
| LED parpadea rojo rapido | Radio no inicializa | Verificar SPI y adaptador 3.3V |
| No detecta golpes | Piezo mal conectado | Verificar soldadura y 1M |
| Golpes falsos constantes | Ruido electrico | Agregar Zener, aumentar threshold |
| Golpe doble en cada hit | Cooldown corto | Aumentar PIEZO_COOLDOWN_MS |
| Pierde golpes rapidos | Cooldown largo | Reducir PIEZO_COOLDOWN_MS |
| Pedal no funciona | Iman mal posicionado | Ajustar distancia iman-reed |
| LED azul no funciona | Pin A3 sin PWM | Es normal, solo ON/OFF |
| No empareja | Hub no en modo pairing | Mantener PAIR en Hub 5 seg primero |
| Bateria se agota rapido | Step-Up ineficiente | Verificar modulo PFM |

---

## Expansion Futura

El multiplexor tiene 10 canales libres (C6-C15):

| Canal | Posible Uso |
|-------|-------------|
| C6 | Hi-Hat cerrado |
| C7 | Hi-Hat abierto |
| C8 | Ride |
| C9 | Crash adicional |
| C10-C15 | Pads extra, potenciometros |

---

