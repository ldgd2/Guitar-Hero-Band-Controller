# Arquitectura de Audio y Soporte de Microfono

---

# [ES] Espanol

## Restriccion de Hardware (RF)

> [!WARNING]
> **El microfono NO esta integrado en el receptor inalambrico del proyecto.**

### Por que no hay microfono en el receptor?

La arquitectura **Bitstream V3** de este proyecto esta disenada para una latencia competitiva extrema (<2ms).

| Problema | Descripcion |
|----------|-------------|
| **Saturacion RF** | El sistema transmite micro-paquetes de solo 2 bytes. Inyectar audio continuo saturaria el ancho de banda 2.4GHz inmediatamente. |
| **Colision de datos** | Implementar audio inalambrico colapsaria la comunicacion de guitarras y baterias, destruyendo la precision del juego. |
| **Costo innecesario** | Un modulo de audio dedicado seria costoso y ofreceria calidad inferior a cualquier microfono USB basico. |

---

## Solucion: World Tour Definitive Edition

> [!TIP]
> **No es necesario conectar el microfono al receptor de la guitarra.**

World Tour Definitive Edition posee una **capa de abstraccion de hardware (HAL)** superior que reconoce y gestiona independientemente cualquier dispositivo de entrada conectado a tu PC.

### Compatibilidad Universal

| Tipo | Ejemplos |
|------|----------|
| **Microfonos USB** | Cualquier marca (Blue Yeti, HyperX, etc.) |
| **Interfaces XLR** | Focusrite, Behringer, etc. |
| **Headsets Gaming** | Cualquier headset con microfono |
| **Microfono Laptop** | Integrado en la computadora |

---

## Como Configurar

### Opcion A: Desde el Launcher (Recomendado)

1. Abre el **GHWT:DE Launcher**
2. Ve a `Adjust Settings` > `Input` > `Mic and Vocal Settings`
3. Selecciona tu microfono de la lista

### Opcion B: Dentro del Juego

1. Ve a `Options` > scroll hasta `GHWT:DE Menu`
2. Entra en `Microphone Options`
3. En `Device`, presiona **Verde (A)** o **Select** para cambiar entre interfaces
4. Retrocede para guardar

> [!IMPORTANT]
> **Debes REINICIAR EL JUEGO para que los cambios surtan efecto.**

---
---

# [EN] English

## Hardware Restriction (RF)

> [!WARNING]
> **The microphone is NOT integrated into the wireless receiver of this project.**

### Why is there no microphone on the receiver?

The **Bitstream V3** architecture of this project is engineered for extreme competitive latency (<2ms).

| Problem | Description |
|---------|-------------|
| **RF Saturation** | The system transmits micro-packets of only 2 bytes. Injecting continuous audio would saturate the 2.4GHz bandwidth immediately. |
| **Data Collision** | Implementing wireless audio would collapse communication for guitars and drums, destroying gameplay precision. |
| **Unnecessary Cost** | A dedicated audio module would be expensive and offer lower quality than any basic USB microphone. |

---

## Solution: World Tour Definitive Edition

> [!TIP]
> **It is not necessary to connect the microphone to the guitar receiver.**

World Tour Definitive Edition features a superior **Hardware Abstraction Layer (HAL)** that recognizes and manages any input device connected to your PC independently.

### Universal Compatibility

| Type | Examples |
|------|----------|
| **USB Microphones** | Any brand (Blue Yeti, HyperX, etc.) |
| **XLR Interfaces** | Focusrite, Behringer, etc. |
| **Gaming Headsets** | Any headset with microphone |
| **Laptop Microphone** | Built-in computer mic |

---

## How to Configure

### Option A: Via Launcher (Recommended)

1. Open **GHWT:DE Launcher**
2. Go to `Adjust Settings` > `Input` > `Mic and Vocal Settings`
3. Select your microphone from the list

### Option B: In-Game

1. Go to `Options` > scroll to `GHWT:DE Menu`
2. Enter `Microphone Options`
3. On `Device`, press **Green (A)** or **Select** to cycle through interfaces
4. Go back to save

> [!IMPORTANT]
> **You must RESTART THE GAME for changes to take effect.**