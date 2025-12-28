# Controlador de Guitarra / Bajo

<p align="center">
  <img src="https://img.shields.io/badge/MCU-Arduino_Nano-00979D?style=for-the-badge&logo=arduino&logoColor=white" alt="Arduino"/>
  <img src="https://img.shields.io/badge/LATENCY-%3C2ms-00ff00?style=for-the-badge" alt="Latency"/>
  <img src="https://img.shields.io/badge/TX_RATE-500Hz-blue?style=for-the-badge" alt="TX Rate"/>
</p>

Diseno ligero y rapido. Utiliza lectura directa de puertos para latencia cero.

---

## Lista de Componentes

### Cerebro

| Componente | Cantidad |
|------------|----------|
| Arduino Nano V3 (CH340) | 1 |

### Comunicacion

| Componente | Cantidad |
|------------|----------|
| Modulo NRF24L01 (antena en placa) | 1 |
| Adaptador de voltaje (zocalo 8 pines) | 1 | 

### Energia (Sistema Paralelo Recargable)

| Componente | Cantidad |
|------------|----------|
| Baterias Li-Ion 18650 | 2 |
| Soporte de bateria doble | 1 | 
| Modulo elevador Step-Up Mini PFM | 1 | 
| Modulo cargador TP4056 | 1 | 
| Interruptor deslizante ON/OFF | 1 |

### Controles (11 botones totales)

| Componente | Cantidad | Notas |
|------------|----------|-------|
| Botones arcade/mecanicos | 5 | Verde, Rojo, Amarillo, Azul, Naranja |
| Pulsadores (cualquiera)| 5 | Start, Select, Whammy, Up, Down |
| Pulsador  (cualquiera) | 1 | Pair/Vinculacion |

### Feedback Visual

| Componente | Cantidad | Notas |
|------------|----------|-------|
| LED RGB difuso 5mm o 10mm | 1 | **ANODO COMUN** |
| Resistencias 220 Ohm | 3 | Una por canal R, G, B |

---

## Diagrama de Conexiones

![Diagrama de conexiones del controlador](./resource/diagram/strings/stringDiagram.png)

### Mapa de Pines

| Pin Arduino | Funcion | Tipo |
|-------------|---------|------|
| D2 | Boton Verde | INPUT_PULLUP |
| D4 | Boton Rojo | INPUT_PULLUP |
| D7 | Boton Amarillo | INPUT_PULLUP |
| D8 | Boton Azul | INPUT_PULLUP |
| A3 | Boton Naranja | INPUT_PULLUP |
| A4 | Strum Up | INPUT_PULLUP |
| A5 | Strum Down | INPUT_PULLUP |
| A2 | Start | INPUT_PULLUP |
| A1 | Boton Pair | INPUT_PULLUP |
| A0 | Whammy Bar | Analogico |
| A7 | Monitor Bateria | Analogico |
| D3 | LED Rojo | PWM Output |
| D5 | LED Verde | PWM Output |
| D6 | LED Azul | PWM Output |
| D9 | NRF24 CE | Output |
| D10 | NRF24 CSN | Output |
| D11 | NRF24 MOSI | SPI |
| D12 | NRF24 MISO | SPI |
| D13 | NRF24 SCK | SPI |

---

## Sistema de Energia

![Diagrama de bateria](./resource/diagram/battery/batteryDiagram.png)

### Flujo de Energia

```
Baterias 18650 (x2 PARALELO)
         |
         v
    +---------+
    | TP4056  | <-- USB-C (Carga)
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

> **IMPORTANTE**: Las baterias van en PARALELO (3.7V), NO en serie (7.4V). En serie danarias el Arduino.

---

## LED RGB (Anodo Comun)

![Indicador LED](./resource/diagram/led/ledIndicator.png)

### Conexion

El LED RGB de anodo comun tiene la logica invertida:
- **255 = LED apagado**
- **0 = LED encendido al maximo**

```
         +-------- R (Catodo) --[220R]-- D3
    5V --+-------- G (Catodo) --[220R]-- D5
         +-------- B (Catodo) --[220R]-- D6
              ^
           Anodo (+)
```

### Colores por Jugador

| ID | Color | RGB (Display) | PWM (Anodo Comun) |
|----|-------|---------------|-------------------|
| P1 | Rojo | (255, 0, 0) | (0, 255, 255) |
| P2 | Azul | (0, 0, 255) | (255, 255, 0) |
| P3 | Verde | (0, 255, 0) | (255, 0, 255) |
| P4 | Magenta | (255, 0, 255) | (0, 255, 0) |

---

## Arquitectura del Codigo

### Librerias Requeridas

```cpp
#include <SPI.h>      // Comunicacion SPI con NRF24
#include <RF24.h>     // Driver del modulo radio (by TMRh20)
#include <EEPROM.h>   // Almacenamiento persistente
```

### Estructura de Datos en EEPROM

El controlador guarda su configuracion en la memoria EEPROM del Arduino:

| Direccion | Tamano | Contenido |
|-----------|--------|-----------|
| 0x00 | 2 bytes | Magic number (0x4748 = "GH") |
| 0x02 | 2 bytes | Device ID (unico por controlador) |
| 0x04 | 1 byte | Player ID (1-4, 0 = no emparejado) |
| 0x05 | 1 byte | Color asignado |

### Primer Encendido

```
1. Verifica si existe el magic number "GH" en EEPROM
   |
   +-- NO existe:
   |     - Genera un Device ID aleatorio (0x1000-0xFFFF)
   |     - Guarda en EEPROM
   |     - Inicia animacion "Wave" blanca
   |
   +-- SI existe:
         - Carga Device ID, Player ID y Color de EEPROM
         - Si tiene Player ID valido (1-4):
           - Configura pipe de radio correspondiente
           - Inicia animacion "Wave" con su color
         - Si no tiene Player ID:
           - Inicia animacion "Wave" blanca
```

---

## Protocolo de Comunicacion

### Estructura de Paquetes

**Paquete de Datos (Normal)**: 2 bytes

```
+------------------+------------------+
|     Byte 0       |     Byte 1       |
+------------------+------------------+
| Botones (bits)   | Whammy (0-255)   |
+------------------+------------------+

Byte 0 - Mapeo de bits:
  Bit 7: Start
  Bit 6: Orange
  Bit 5: Blue
  Bit 4: Yellow
  Bit 3: Red
  Bit 2: Green
  Bit 1: Strum Down
  Bit 0: Strum Up
```

**Paquete de Pairing**: 4 bytes

```
+--------+--------+--------+--------+
| Byte 0 | Byte 1 | Byte 2 | Byte 3 |
+--------+--------+--------+--------+
|  0xAA  |  Type  | ID Low | ID Hi  |
| Magic  | Instr. |   Device ID     |
+--------+--------+--------+--------+
```

**ACK Payload (del Hub)**: 2 bytes

```
+------------------+------------------+
|     Byte 0       |     Byte 1       |
+------------------+------------------+
| [Anim:4|Color:4] |    Reservado     |
+------------------+------------------+

Anim ID (4 bits superiores):
  0x0 = Sin cambio
  0x1 = Wave
  0x2 = Morph
  0x3 = Heartbeat
  0x4 = Solid

Color ID (4 bits inferiores):
  0x1 = Rojo (P1)
  0x2 = Azul (P2)
  0x3 = Verde (P3)
  0x4 = Magenta (P4)
```

---

## Flujo de Transmision (500 Hz)

```
loop() ejecuta cada 2ms (500 Hz):
         |
         v
+-------------------+
| readButtonsFast() |  Lee todos los botones
+--------+----------+
         |
         v
+-------------------+
|   readWhammy()    |  Lee potenciometro (0-255)
+--------+----------+
         |
         v
+-------------------+
| Construir packet  |  [buttons, whammy]
| de 2 bytes        |
+--------+----------+
         |
         v
+-------------------+
| radio.write()     |  Envia al Hub
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
| processAckPayload |  Recibe color/animacion
+-------------------+
```

---

## Sistema de Emparejamiento (Pairing)

### Como emparejar

1. **Mantener presionado** el boton PAIR por **3 segundos**
2. El LED inicia animacion **Wave** (blanco pulsante)
3. El controlador envia solicitudes al canal de pairing
4. Cuando el Hub responde, asigna:
   - Player ID (1-4)
   - Color correspondiente
5. El controlador guarda en EEPROM
6. LED hace **Morph** al color asignado
7. Listo para jugar

### Flujo de Pairing

```
[CONTROLADOR]                         [HUB]
     |                                   |
     | Boton PAIR 3 seg                  |
     |---------------------------------->|
     |                                   |
     | LED: Wave blanco                  |
     |                                   |
     | Paquete: [0xAA, Type, ID_Lo, ID_Hi]
     |---------------------------------->|
     |                                   |
     |        ACK: [PlayerID, Color]     |
     |<----------------------------------|
     |                                   |
     | Guardar en EEPROM                 |
     | LED: Morph -> Color               |
     |                                   |
     | Cambiar a pipe de jugador         |
     |---------------------------------->|
     |                                   |
     |       Transmision normal          |
     |<=================================>|
```

### Timeout

- Ventana de pairing: **30 segundos**
- Si no recibe respuesta, vuelve al estado anterior

---

## Sistema de Animaciones

### Tipos de Animacion

| ID | Nombre | Comportamiento | Cuando se usa |
|----|--------|----------------|---------------|
| ANIM_WAVE | Ola | Fade in/out suave (2s ciclo) | Buscando Hub / Pairing |
| ANIM_MORPH | Transicion | Cambio lineal de color (1.5s) | Al conectar |
| ANIM_HEARTBEAT | Latido | Pulso sutil cada 5s | Standby / Idle |
| ANIM_SOLID | Solido | Color fijo 100% | Jugando activamente |
| ANIM_DEAD_BATTERY | Alarma | Rojo fijo | Bateria < 3.4V |

### Maquina de Estados

```cpp
void updateAnimation() {
  switch (currentAnim) {
    case ANIM_WAVE:
      // Breathing: brillo 0% -> 100% -> 0% en 2 segundos
      break;
      
    case ANIM_MORPH:
      // Interpolacion lineal entre colores en 1.5s
      break;
      
    case ANIM_HEARTBEAT:
      // 80% brillo base + pulso sutil cada 5s
      break;
      
    case ANIM_SOLID:
      // Color fijo al 100%
      break;
      
    case ANIM_DEAD_BATTERY:
      // Rojo solido, bloquea todo
      break;
  }
}
```

---

## Monitoreo de Bateria

### Umbrales

| Voltaje | ADC (aprox) | Estado | Accion |
|---------|-------------|--------|--------|
| > 3.9V | > 400 | OK | Normal |
| 3.7V - 3.9V | 378-400 | Low | Advertencia |
| < 3.4V | < 348 | Critico | **DETIENE TRANSMISION** |

### Comportamiento Critico

Cuando la bateria cae bajo 3.4V:

1. `isBatteryCritical = true`
2. LED fijo en **ROJO**
3. **Deja de transmitir** (ahorra energia)
4. El loop solo muestra el LED rojo

> Esto protege las baterias 18650 de descarga profunda.

---

## Configuracion del Radio

```cpp
radio.setChannel(108);           // Canal fijo (evita WiFi)
radio.setDataRate(RF24_2MBPS);   // Velocidad maxima
radio.setPALevel(RF24_PA_HIGH);  // Potencia alta
radio.setPayloadSize(2);         // Paquetes ultraligeros
radio.setAutoAck(true);          // ACK automatico
radio.enableAckPayload();        // Recibir datos en ACK
radio.setCRCLength(RF24_CRC_8);  // Verificacion de errores
```

### Pipes de Comunicacion

| Pipe | Direccion | Uso |
|------|-----------|-----|
| Pairing | 0xE8E8PAIR00 | Canal publico de emparejamiento |
| Player 1 | 0xE8E8F0F001 | Comunicacion P1 (retry 250us) |
| Player 2 | 0xE8E8F0F002 | Comunicacion P2 (retry 500us) |
| Player 3 | 0xE8E8F0F003 | Comunicacion P3 (retry 750us) |
| Player 4 | 0xE8E8F0F004 | Comunicacion P4 (retry 1000us) |

> Los delays escalonados evitan colisiones entre instrumentos.

---

## Subir el Codigo

### Requisitos

1. Arduino IDE instalado
2. Libreria **RF24 by TMRh20** instalada

### Pasos

1. Abrir `string_controller.ino` en Arduino IDE
2. Seleccionar placa: **Arduino Nano**
3. Seleccionar procesador: **ATmega328P** o **ATmega328P (Old Bootloader)**
4. Seleccionar puerto COM correcto
5. Click en **Upload**

### Configurar tipo de instrumento

En la linea 30 del codigo, cambiar segun corresponda:

```cpp
#define INSTRUMENT_TYPE   INST_GUITAR   // Para guitarra
// o
#define INSTRUMENT_TYPE   INST_BASS     // Para bajo
```

---

## Solucion de Problemas

| Problema | Causa | Solucion |
|----------|-------|----------|
| LED parpadea rojo rapido | Radio no inicializa | Verificar conexiones SPI y adaptador de voltaje |
| LED rojo fijo | Bateria critica | Cargar las baterias |
| No empareja | Hub no esta en modo pairing | Mantener PAIR en Hub por 5 seg primero |
| Botones no responden | Conexion suelta | Verificar soldaduras con multimetro |
| Whammy no funciona | Potenciometro danado | Probar con otro potenciometro |

---

## Licencia

Este codigo es parte del proyecto Guitar Hero Band Controller. Ver [LICENSE](../LICENSE) para detalles.
