# Guitar Hero Band Controller

<p align="center">
  <img src="https://img.shields.io/badge/STATUS-IN_DEVELOPMENT-yellow?style=for-the-badge" alt="Status"/>
  <img src="https://img.shields.io/badge/OPEN_SOURCE-100%25-brightgreen?style=for-the-badge" alt="Open Source"/>
  <img src="https://img.shields.io/badge/PLAYERS-4_MAX-orange?style=for-the-badge" alt="Players"/>
  <img src="https://img.shields.io/badge/LATENCY-%3C2ms-00ff00?style=for-the-badge" alt="Latency"/>
</p>

<p align="center">
  <a href="#english">English</a> | <a href="#español">Español</a> | <a href="./LICENSE">License</a>
</p>

---

<a name="english"></a>

# English

## From the Community, For the Community

The Guitar Hero and Rock Band community is being forgotten. Controllers break, replacements are impossible to find, and corporations have moved on.

**But we haven't.**

This project exists because **the community deserves to enjoy these titles the way they were meant to be played**: with proper controllers. We're building the solution together.

> *"Take freely. Give back generously."*

---

## What Is This?

A complete wireless controller system for **Guitar Hero**, **Rock Band**, and similar rhythm games on PC:

- **Ultra-low latency**: < 2ms (better than many original controllers)
- **True wireless**: 2.4GHz with ~15 meters range
- **Plug and Play**: Dongle detected as standard USB gamepad
- **Multiplayer**: Up to 4 simultaneous instruments
- **100% Open Source**: Free for personal use

---

## System Architecture

```
┌─────────────────┐
│   GUITAR 1      │───┐
│   Arduino Nano  │   │
└─────────────────┘   │     ┌──────────────────────┐
                      │     │                      │
┌─────────────────┐   │     │   DONGLE HUB         │
│   GUITAR 2      │───┼────>│   Raspberry Pi Pico  │────> PC (USB)
│   Arduino Nano  │   │     │   + OLED Display     │
└─────────────────┘   │     │                      │
                      │     └──────────────────────┘
┌─────────────────┐   │
│   DRUMS         │───┘
│   Pro Micro     │
└─────────────────┘
```

---

## Documentation by Instrument

Each instrument has detailed documentation with component lists, wiring diagrams, and code.

| Instrument | English | Español |
|------------|---------|---------|
| **Guitar / Bass** | [README_EN.md](./strings/README_EN.md) | [README_ES.md](./strings/README_ES.md) |
| **Drums** | [README_EN.md](./Drum/README_EN.md) | [README_ES.md](./Drum/README_ES.md) |
| **Dongle Hub** | [README_EN.md](./Dongle/README_EN.md) | [README.md](./Dongle/README.md) |
| **Microphone** | [Microphone.md](./Microphone/Microphone.md) | Uses PC native mic |

---

## Quick Start

1. **Read the documentation** for the instrument you want to build
2. **Build the Dongle first** - It's the brain of the system
3. **Build your instrument** following the wiring diagrams
4. **Upload the code** with Arduino IDE / Thonny
5. **Pair**: PAIR on dongle (5 sec) + PAIR on instrument (3 sec)
6. **Play!**

---

## Enclosure Options

### Option A: Broken Controllers (Recommended)

Buy Guitar Hero or Rock Band controllers that are **electronically dead but with intact shells**:

- Search: "Guitar Hero not working", "Rock Band no dongle"
- Local marketplaces, thrift stores, garage sales
- You only need the shell, buttons, and mechanisms

### Option B: 3D Printing

- STL files coming soon in `/3D-Models`
- Material: PLA or PETG
- Infill: 20-30%

### Option C: DIY Creative

- Toy guitars from thrift stores
- Plastic boxes cut and adapted
- Wood/MDF with laser cutting

---

## License

<p align="center">
  <a href="./LICENSE"><img src="https://img.shields.io/badge/LICENSE-OPEN_HARDWARE-blue?style=for-the-badge" alt="License"/></a>
</p>

**Free for everyone who loves Guitar Hero, Rock Band, and rhythm games.**

| Use Case | Allowed |
|----------|---------|
| Personal use | Yes, completely free |
| Educational / Research | Yes |
| Building for yourself or friends | Yes |
| Selling handmade units | Yes, with credit |
| **Mass production for profit** | **Yes, but you MUST release everything you build** |

> **Want to make money with this?** Read the [LICENSE](./LICENSE) first.
> 
> The rule is simple: **if you benefit from open source, you contribute to open source.**

---

## Contributing

Contributions are welcome:
- Code improvements
- 3D enclosure designs
- Translations
- Photos/videos of finished builds

---

## Credits

Project developed for the Guitar Hero community.

*The rhythm never dies. Neither does the community that keeps it alive.*

---
---
---

<a name="español"></a>

# Español

## De la Comunidad, Para la Comunidad

La comunidad de Guitar Hero y Rock Band está siendo olvidada. Los controladores se rompen, los repuestos son imposibles de encontrar, y las corporaciones ya pasaron página.

**Pero nosotros no.**

Este proyecto existe porque **la comunidad merece disfrutar estos títulos como fueron diseñados para jugarse**: con los controladores adecuados. Estamos construyendo la solución juntos.

> *"Toma libremente. Devuelve generosamente."*

---

## Qué Es Esto?

Un sistema completo de controladores inalámbricos para **Guitar Hero**, **Rock Band**, y juegos de ritmo similares en PC:

- **Latencia ultra-baja**: < 2ms (mejor que muchos controladores originales)
- **Inalámbrico real**: 2.4GHz con ~15 metros de alcance
- **Plug and Play**: El dongle se detecta como gamepad USB estándar
- **Multijugador**: Hasta 4 instrumentos simultáneos
- **100% Open Source**: Libre para uso personal

---

## Arquitectura del Sistema

```
┌─────────────────┐
│   GUITARRA 1    │───┐
│   Arduino Nano  │   │
└─────────────────┘   │     ┌──────────────────────┐
                      │     │                      │
┌─────────────────┐   │     │   DONGLE HUB         │
│   GUITARRA 2    │───┼────>│   Raspberry Pi Pico  │────> PC (USB)
│   Arduino Nano  │   │     │   + Pantalla OLED    │
└─────────────────┘   │     │                      │
                      │     └──────────────────────┘
┌─────────────────┐   │
│   BATERÍA       │───┘
│   Pro Micro     │
└─────────────────┘
```

---

## Documentación por Instrumento

Cada instrumento tiene documentación detallada con lista de componentes, diagramas de conexión y código.

| Instrumento | English | Español |
|-------------|---------|---------|
| **Guitarra / Bajo** | [README_EN.md](./strings/README_EN.md) | [README_ES.md](./strings/README_ES.md) |
| **Batería** | [README_EN.md](./Drum/README_EN.md) | [README_ES.md](./Drum/README_ES.md) |
| **Dongle Hub** | [README_EN.md](./Dongle/README_EN.md) | [README.md](./Dongle/README.md) |
| **Micrófono** | [Microphone.md](./Microphone/Microphone.md) | Usa micrófono nativo del PC |

---

## Inicio Rápido

1. **Lee la documentación** del instrumento que quieres armar
2. **Arma el Dongle primero** - Es el cerebro del sistema
3. **Arma tu instrumento** siguiendo los diagramas de conexión
4. **Carga el código** con Arduino IDE / Thonny
5. **Empareja**: PAIR en dongle (5 seg) + PAIR en instrumento (3 seg)
6. **A jugar!**

---

## Opciones de Carcasa

### Opción A: Controladores Averiados (Recomendado)

Compra controladores de Guitar Hero o Rock Band que estén **muertos electrónicamente pero con carcasa intacta**:

- Buscar: "Guitarra Guitar Hero no funciona", "Rock Band sin dongle"
- Mercados locales, tiendas de segunda mano, ferias
- Solo necesitas la carcasa, botones y mecanismos

### Opción B: Impresión 3D

- Archivos STL próximamente en `/3D-Models`
- Material: PLA o PETG
- Relleno: 20-30%

### Opción C: DIY Creativo

- Guitarras de juguete de tiendas de segunda mano
- Cajas de plástico cortadas y adaptadas
- Madera/MDF con corte láser

---

## Licencia

<p align="center">
  <a href="./LICENSE"><img src="https://img.shields.io/badge/LICENCIA-OPEN_HARDWARE-blue?style=for-the-badge" alt="Licencia"/></a>
</p>

**Libre para todos los que aman Guitar Hero, Rock Band y los juegos de ritmo.**

| Caso de Uso | Permitido |
|-------------|-----------|
| Uso personal | Sí, completamente gratis |
| Educativo / Investigación | Sí |
| Construir para ti o amigos | Sí |
| Vender unidades artesanales | Sí, con crédito |
| **Producción en masa con fines de lucro** | **Sí, pero DEBES liberar todo lo que construyas** |

> **¿Quieres ganar dinero con esto?** Lee la [LICENCIA](./LICENSE) primero.
> 
> La regla es simple: **si te beneficias del código abierto, contribuyes al código abierto.**

---

## Contribuir

Las contribuciones son bienvenidas:
- Mejoras al código
- Diseños 3D para carcasas
- Traducciones
- Fotos/videos de builds terminados

---

## Créditos

Proyecto desarrollado para la comunidad de Guitar Hero.

*El ritmo nunca muere. Tampoco la comunidad que lo mantiene vivo.*

---

## Estructura del Proyecto / Project Structure

```
Guitar Hero/
├── README.md                    # This file / Este archivo
├── LICENSE                      # Project license / Licencia del proyecto
├── Dongle/
│   ├── README.md                # Español
│   ├── README_EN.md             # English
│   ├── dongle.py                # Main code
│   └── boot.py                  # USB HID config
├── strings/
│   ├── README_ES.md             # Español
│   ├── README_EN.md             # English
│   └── string_controller.ino    # Arduino code
├── Drum/
│   ├── README_ES.md             # Español
│   ├── README_EN.md             # English
│   └── drum_controller.ino      # Arduino code
├── Microphone/
│   └── Microphone.md            # Info
└── 3D-Models/                   # Coming soon / Próximamente
```

---

## Software Requirements / Requisitos de Software

| Software | Use / Uso | Link |
|----------|-----------|------|
| Arduino IDE 1.8+ | Program Nano & Pro Micro | [arduino.cc](https://www.arduino.cc/en/software) |
| RF24 Library | NRF24L01 communication | Library Manager |
| Thonny | Program Raspberry Pi Pico | [thonny.org](https://thonny.org/) |
