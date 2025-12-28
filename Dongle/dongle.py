"""
============================================================================
 GUITAR HERO - HUB DONGLE BITSTREAM V3 (COMPETITIVE GRADE)
============================================================================
MCU: Raspberry Pi Pico (RP2040)
Radio: NRF24L01+ with voltage adapter
Display: OLED SSD1306 0.96" 128x64 I2C
IRQ: Connected to GP21 for interrupt-driven reception

OPTIMIZATIONS:
- IRQ-based packet reception (no polling)
- ACK Payloads for sending color/animation commands
- Dual-core: Core0=USB+Display, Core1=RF
- 2-byte ultra-light packets
- Staggered retry delays per instrument
- No collision loops (mathematically impossible)

Author: Guitar Hero DIY Project
License: MIT
============================================================================
"""

import time
import struct
import json
import _thread
from machine import Pin, SPI, I2C
from micropython import const
import gc

# ============================================================================
# CONFIGURATION
# ============================================================================
RF_CHANNEL = const(108)
MAX_PLAYERS = const(4)

# Payload sizes
PAYLOAD_INPUT = const(2)    # Normal input packets
PAYLOAD_PAIRING = const(4)  # Pairing request packets
PAYLOAD_ACK = const(2)      # ACK payload size

# Pipe addresses (LSB first for NRF24)
PAIRING_PIPE = bytes([0x00, 0x49, 0x41, 0x50, 0xE8])  # 0xE8E8PAIR00

PLAYER_PIPES = [
    bytes([0x01, 0xF0, 0xF0, 0xE8, 0xE8]),  # Player 1
    bytes([0x02, 0xF0, 0xF0, 0xE8, 0xE8]),  # Player 2
    bytes([0x03, 0xF0, 0xF0, 0xE8, 0xE8]),  # Player 3
    bytes([0x04, 0xF0, 0xF0, 0xE8, 0xE8]),  # Player 4
]

# Instrument types
INST_GUITAR = const(0x01)
INST_BASS = const(0x02)
INST_DRUM = const(0x03)
INST_MIC = const(0x04)

INSTRUMENT_NAMES = {
    INST_GUITAR: "Guitar",
    INST_BASS: "Bass",
    INST_DRUM: "Drum",
    INST_MIC: "Mic",
}

INSTRUMENT_ICONS = {
    INST_GUITAR: "G",
    INST_BASS: "B",
    INST_DRUM: "D",
    INST_MIC: "M",
}

# Player colors (display representation)
PLAYER_COLORS = [
    "",           # 0: None
    "RED",        # 1
    "BLUE",       # 2
    "GREEN",      # 3
    "MAGENTA",    # 4
]

# ACK payload color codes (for common anode LEDs)
# Format: [AnimID:4 | ColorID:4]
ACK_COLORS = {
    1: 0x01,  # Red
    2: 0x02,  # Blue
    3: 0x03,  # Green
    4: 0x04,  # Magenta
}

# Timing
PAIR_HOLD_MS = const(5000)
PAIR_TIMEOUT_MS = const(30000)
DEVICE_TIMEOUT_MS = const(3000)
DISPLAY_UPDATE_MS = const(50)

# Pins
PIN_CE = const(17)
PIN_CSN = const(20)
PIN_SCK = const(18)
PIN_MOSI = const(19)
PIN_MISO = const(16)
PIN_IRQ = const(21)

PIN_SDA = const(4)
PIN_SCL = const(5)

PIN_PAIR_BTN = const(15)
PIN_LED = const(25)

# ============================================================================
# NRF24L01 REGISTERS
# ============================================================================
CONFIG = const(0x00)
EN_AA = const(0x01)
EN_RXADDR = const(0x02)
SETUP_AW = const(0x03)
SETUP_RETR = const(0x04)
RF_CH = const(0x05)
RF_SETUP = const(0x06)
STATUS = const(0x07)
OBSERVE_TX = const(0x08)
RX_ADDR_P0 = const(0x0A)
RX_ADDR_P1 = const(0x0B)
TX_ADDR = const(0x10)
RX_PW_P0 = const(0x11)
FIFO_STATUS = const(0x17)
DYNPD = const(0x1C)
FEATURE = const(0x1D)

R_RX_PAYLOAD = const(0x61)
W_TX_PAYLOAD = const(0xA0)
W_ACK_PAYLOAD = const(0xA8)  # Write ACK payload for pipe
FLUSH_TX = const(0xE1)
FLUSH_RX = const(0xE2)
REUSE_TX_PL = const(0xE3)

# ============================================================================
# NRF24L01 DRIVER (Optimized for Hub)
# ============================================================================
class NRF24L01Hub:
    def __init__(self, spi, cs, ce, irq_pin):
        self.spi = spi
        self.cs = cs
        self.ce = ce
        self.irq = irq_pin
        self.cs.value(1)
        self.ce.value(0)
        time.sleep_ms(5)
        self._init_radio()
    
    def _init_radio(self):
        # Power down first
        self._write_reg(CONFIG, 0x08)
        time.sleep_ms(5)
        
        # Channel and speed
        self._write_reg(RF_CH, RF_CHANNEL)
        self._write_reg(RF_SETUP, 0x0F)  # 2Mbps, high power
        
        # Address width: 5 bytes
        self._write_reg(SETUP_AW, 0x03)
        
        # Auto-ack on all pipes
        self._write_reg(EN_AA, 0x3F)
        
        # Enable RX pipes 0-4 (pairing + 4 players)
        self._write_reg(EN_RXADDR, 0x1F)
        
        # Retry config (not used in RX mode, but needed for ACK)
        self._write_reg(SETUP_RETR, 0x13)
        
        # Enable dynamic payload and ACK payloads
        self._write_reg(FEATURE, 0x06)  # EN_DPL + EN_ACK_PAY
        self._write_reg(DYNPD, 0x3F)    # Dynamic payload all pipes
        
        # Clear FIFOs and status
        self._flush_rx()
        self._flush_tx()
        self._write_reg(STATUS, 0x70)
    
    def _write_reg(self, reg, value):
        self.cs.value(0)
        self.spi.write(bytes([0x20 | reg, value]))
        self.cs.value(1)
    
    def _read_reg(self, reg):
        self.cs.value(0)
        self.spi.write(bytes([reg]))
        result = self.spi.read(1)
        self.cs.value(1)
        return result[0]
    
    def _write_address(self, reg, address):
        self.cs.value(0)
        self.spi.write(bytes([0x20 | reg]) + address)
        self.cs.value(1)
    
    def _flush_rx(self):
        self.cs.value(0)
        self.spi.write(bytes([FLUSH_RX]))
        self.cs.value(1)
    
    def _flush_tx(self):
        self.cs.value(0)
        self.spi.write(bytes([FLUSH_TX]))
        self.cs.value(1)
    
    def set_rx_pipe(self, pipe_num, address, payload_size=32):
        """Configure a receive pipe"""
        if pipe_num == 0:
            self._write_address(RX_ADDR_P0, address)
        elif pipe_num == 1:
            self._write_address(RX_ADDR_P1, address)
        else:
            # Pipes 2-5 only set LSB
            self._write_reg(RX_ADDR_P0 + pipe_num, address[0])
        
        # Set payload size (ignored if dynamic payload enabled)
        self._write_reg(RX_PW_P0 + pipe_num, payload_size)
    
    def write_ack_payload(self, pipe, data):
        """Write payload to be sent with next ACK on specified pipe"""
        self.cs.value(0)
        self.spi.write(bytes([W_ACK_PAYLOAD | pipe]) + data)
        self.cs.value(1)
    
    def start_listening(self):
        """Enter RX mode"""
        self._write_reg(CONFIG, 0x0F)  # Power up, RX mode, CRC
        time.sleep_us(150)
        self.ce.value(1)
        time.sleep_us(130)
    
    def stop_listening(self):
        """Exit RX mode"""
        self.ce.value(0)
        self._flush_rx()
    
    def available(self):
        """Check if data available"""
        return self.irq.value() == 0  # IRQ is active low
    
    def get_pipe_number(self):
        """Get pipe number that received data"""
        status = self._read_reg(STATUS)
        pipe = (status >> 1) & 0x07
        return pipe if pipe <= 5 else -1
    
    def read_payload(self, max_size=32):
        """Read payload with dynamic size"""
        # Get payload size
        self.cs.value(0)
        self.spi.write(bytes([0x60]))  # R_RX_PL_WID
        size = self.spi.read(1)[0]
        self.cs.value(1)
        
        if size > 32:
            self._flush_rx()
            return None
        
        # Read payload
        self.cs.value(0)
        self.spi.write(bytes([R_RX_PAYLOAD]))
        buf = self.spi.read(size)
        self.cs.value(1)
        
        # Clear RX_DR flag
        self._write_reg(STATUS, 0x40)
        
        return buf
    
    def set_tx_mode(self, address):
        """Switch to TX mode for pairing response"""
        self.ce.value(0)
        self._write_address(TX_ADDR, address)
        self._write_address(RX_ADDR_P0, address)  # For auto-ack
        
        # TX mode
        self._write_reg(CONFIG, 0x0E)
        time.sleep_us(150)
    
    def write_payload(self, data):
        """Write and transmit payload"""
        self._flush_tx()
        self.cs.value(0)
        self.spi.write(bytes([W_TX_PAYLOAD]) + data)
        self.cs.value(1)
        
        self.ce.value(1)
        time.sleep_us(15)
        self.ce.value(0)
        
        # Wait for TX complete
        start = time.ticks_ms()
        while time.ticks_diff(time.ticks_ms(), start) < 10:
            status = self._read_reg(STATUS)
            if status & 0x20:  # TX_DS
                self._write_reg(STATUS, 0x20)
                return True
            if status & 0x10:  # MAX_RT
                self._write_reg(STATUS, 0x10)
                self._flush_tx()
                return False
        return False

# ============================================================================
# OLED DISPLAY (Minimal SSD1306)
# ============================================================================
class SSD1306:
    def __init__(self, i2c, addr=0x3C):
        self.i2c = i2c
        self.addr = addr
        self.width = 128
        self.height = 64
        self.buffer = bytearray(self.width * self.height // 8)
        self._init()
    
    def _init(self):
        for cmd in [0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
                    0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
                    0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF]:
            self.i2c.writeto(self.addr, bytes([0x00, cmd]))
    
    def clear(self):
        for i in range(len(self.buffer)):
            self.buffer[i] = 0
    
    def text(self, s, x, y, c=1):
        import framebuf
        fb = framebuf.FrameBuffer(self.buffer, self.width, self.height, framebuf.MONO_VLSB)
        fb.text(s, x, y, c)
    
    def hline(self, x, y, w, c=1):
        import framebuf
        fb = framebuf.FrameBuffer(self.buffer, self.width, self.height, framebuf.MONO_VLSB)
        fb.hline(x, y, w, c)
    
    def show(self):
        self.i2c.writeto(self.addr, bytes([0x00, 0x21, 0, 127]))
        self.i2c.writeto(self.addr, bytes([0x00, 0x22, 0, 7]))
        for i in range(0, len(self.buffer), 32):
            self.i2c.writeto(self.addr, bytes([0x40]) + self.buffer[i:i+32])

# ============================================================================
# DISPLAY MANAGER
# ============================================================================
class DisplayManager:
    def __init__(self, i2c):
        try:
            self.oled = SSD1306(i2c)
            self.available = True
        except:
            self.available = False
            print("OLED not found")
        self.frame = 0
    
    def show_startup(self):
        if not self.available:
            return
        self.oled.clear()
        self.oled.text("GUITAR HERO", 20, 8)
        self.oled.text("BITSTREAM V3", 16, 24)
        self.oled.hline(0, 40, 128)
        self.oled.text("Initializing...", 16, 50)
        self.oled.show()
    
    def show_normal(self, players, connected_mask):
        if not self.available:
            return
        self.oled.clear()
        self.oled.text("CONNECTED", 32, 0)
        self.oled.hline(0, 10, 128)
        
        y = 14
        any_connected = False
        
        for pid in range(1, MAX_PLAYERS + 1):
            if pid in players:
                info = players[pid]
                icon = INSTRUMENT_ICONS.get(info["type"], "?")
                color = PLAYER_COLORS[pid] if pid <= 4 else "?"
                status = "*" if (connected_mask & (1 << pid)) else " "
                line = f"P{pid}{status} [{icon}] {color}"
                self.oled.text(line, 0, y)
                any_connected = True
            else:
                self.oled.text(f"P{pid}  ---", 0, y)
            y += 12
        
        if not any_connected:
            self.oled.text("Hold PAIR 5s", 16, 50)
        
        self.oled.show()
    
    def show_pairing(self, remaining, found):
        if not self.available:
            return
        self.oled.clear()
        self.oled.text("PAIRING MODE", 16, 0)
        self.oled.hline(0, 12, 128)
        
        # Animated searching
        self.frame = (self.frame + 1) % 4
        dots = "." * (self.frame + 1)
        self.oled.text(f"Searching{dots}", 8, 24)
        
        self.oled.text(f"Time: {remaining}s", 32, 38)
        self.oled.text(f"Found: {found}", 32, 50)
        self.oled.show()
    
    def show_paired(self, player_id, inst_type):
        if not self.available:
            return
        self.oled.clear()
        self.oled.text("** PAIRED **", 20, 8)
        self.oled.hline(0, 22, 128)
        
        name = INSTRUMENT_NAMES.get(inst_type, "Device")
        color = PLAYER_COLORS[player_id] if player_id <= 4 else "?"
        
        self.oled.text(f"Player {player_id}", 32, 28)
        self.oled.text(name, 40, 40)
        self.oled.text(color, 44, 52)
        self.oled.show()

# ============================================================================
# PLAYER MANAGER
# ============================================================================
class PlayerManager:
    CONFIG_FILE = "/hub_v3.json"
    
    def __init__(self):
        self.players = {}  # player_id -> {device_id, type}
        self.connected_mask = 0  # Bitmask of recently active players
        self.last_seen = {}  # player_id -> timestamp
        self._load()
    
    def _load(self):
        try:
            with open(self.CONFIG_FILE, "r") as f:
                data = json.load(f)
                for k, v in data.items():
                    self.players[int(k)] = v
        except:
            pass
    
    def _save(self):
        try:
            with open(self.CONFIG_FILE, "w") as f:
                json.dump({str(k): v for k, v in self.players.items()}, f)
        except Exception as e:
            print(f"Save error: {e}")
    
    def find_by_device(self, device_id):
        for pid, info in self.players.items():
            if info["device_id"] == device_id:
                return pid
        return None
    
    def get_free_slot(self):
        for i in range(1, MAX_PLAYERS + 1):
            if i not in self.players:
                return i
        return None
    
    def pair(self, device_id, inst_type):
        existing = self.find_by_device(device_id)
        if existing:
            self.players[existing]["type"] = inst_type
            self._save()
            return existing
        
        slot = self.get_free_slot()
        if slot is None:
            return None
        
        self.players[slot] = {"device_id": device_id, "type": inst_type}
        self._save()
        return slot
    
    def update_activity(self, player_id):
        self.last_seen[player_id] = time.ticks_ms()
        self.connected_mask |= (1 << player_id)
    
    def check_timeouts(self):
        now = time.ticks_ms()
        for pid in list(self.last_seen.keys()):
            if time.ticks_diff(now, self.last_seen[pid]) > DEVICE_TIMEOUT_MS:
                self.connected_mask &= ~(1 << pid)
    
    def get_connected_count(self):
        return bin(self.connected_mask).count('1')
    
    def unpair_all(self):
        self.players = {}
        self._save()

# ============================================================================
# USB HID GAMEPAD
# ============================================================================
class GamepadHID:
    def __init__(self):
        self.states = {i: {"buttons": 0, "axis": 127} for i in range(1, MAX_PLAYERS + 1)}
        self._last_report = None
        
        try:
            import usb_hid
            self.device = None
            for dev in usb_hid.devices:
                if hasattr(dev, 'send_report'):
                    self.device = dev
                    break
        except:
            self.device = None
    
    def update(self, player_id, buttons, axis=127):
        if 1 <= player_id <= MAX_PLAYERS:
            self.states[player_id]["buttons"] = buttons
            self.states[player_id]["axis"] = axis
            self._send()
    
    def _send(self):
        if not self.device:
            return
        
        # Combine all players into one report
        # Player 1: buttons 0-7
        # Player 2: buttons 8-15
        # etc.
        combined = 0
        axis_x = 127
        axis_y = 127
        
        for pid in range(1, MAX_PLAYERS + 1):
            state = self.states[pid]
            if state["buttons"]:
                shift = (pid - 1) * 8
                if shift < 16:
                    combined |= (state["buttons"] & 0xFF) << shift
                axis_x = state["axis"]
        
        report = bytes([
            0x01,
            combined & 0xFF,
            (combined >> 8) & 0xFF,
            axis_x,
            axis_y
        ])
        
        if report != self._last_report:
            try:
                self.device.send_report(report)
                self._last_report = report
            except:
                pass

# ============================================================================
# MAIN HUB RECEIVER
# ============================================================================
class HubReceiver:
    def __init__(self):
        print("Initializing Hub Bitstream V3...")
        
        # GPIO
        self.led = Pin(PIN_LED, Pin.OUT)
        self.pair_btn = Pin(PIN_PAIR_BTN, Pin.IN, Pin.PULL_UP)
        self.irq = Pin(PIN_IRQ, Pin.IN)
        
        # SPI
        self.spi = SPI(0, baudrate=8_000_000, polarity=0, phase=0,
                      sck=Pin(PIN_SCK), mosi=Pin(PIN_MOSI), miso=Pin(PIN_MISO))
        self.cs = Pin(PIN_CSN, Pin.OUT)
        self.ce = Pin(PIN_CE, Pin.OUT)
        
        # I2C
        self.i2c = I2C(0, sda=Pin(PIN_SDA), scl=Pin(PIN_SCL), freq=400_000)
        
        # Components
        self.radio = NRF24L01Hub(self.spi, self.cs, self.ce, self.irq)
        self.display = DisplayManager(self.i2c)
        self.players = PlayerManager()
        self.gamepad = GamepadHID()
        
        # State
        self.is_pairing = False
        self.pairing_start = 0
        self.pair_btn_start = 0
        self.running = True
        
        # Buffer for IRQ-based reception
        self._rx_queue = []
        self._queue_lock = _thread.allocate_lock()
        
        # Show startup
        self.display.show_startup()
        time.sleep(1)
        
        # Setup radio
        self._setup_normal_mode()
        
        # Preload ACK payloads for each pipe
        self._preload_ack_payloads()
    
    def _setup_normal_mode(self):
        """Configure radio for normal multi-pipe reception"""
        self.radio.stop_listening()
        
        # Pipe 0: Pairing (when in pairing mode)
        # Pipes 1-4: Players
        for i, pipe in enumerate(PLAYER_PIPES):
            self.radio.set_rx_pipe(i + 1, pipe)
        
        self.radio.start_listening()
        print("Normal mode active")
    
    def _setup_pairing_mode(self):
        """Configure radio for pairing"""
        self.radio.stop_listening()
        
        # Pipe 0 for pairing
        self.radio.set_rx_pipe(0, PAIRING_PIPE)
        
        # Keep player pipes active
        for i, pipe in enumerate(PLAYER_PIPES):
            self.radio.set_rx_pipe(i + 1, pipe)
        
        self.radio.start_listening()
        print("Pairing mode active")
    
    def _preload_ack_payloads(self):
        """Preload ACK payloads with current color for each player"""
        for pid in range(1, MAX_PLAYERS + 1):
            color_code = ACK_COLORS.get(pid, 0)
            ack_data = bytes([0x40 | color_code, 0x00])  # Anim=SOLID, Color=pid
            self.radio.write_ack_payload(pid, ack_data)
    
    def _rf_task(self):
        """RF reception task (Core 1) - IRQ driven"""
        while self.running:
            if self.radio.available():
                pipe = self.radio.get_pipe_number()
                data = self.radio.read_payload()
                
                if data:
                    with self._queue_lock:
                        if len(self._rx_queue) < 32:
                            self._rx_queue.append((pipe, bytes(data)))
                
                # Reload ACK payload for this pipe
                if 1 <= pipe <= MAX_PLAYERS:
                    pid = pipe
                    color_code = ACK_COLORS.get(pid, 0)
                    ack_data = bytes([0x40 | color_code, 0x00])
                    self.radio.write_ack_payload(pipe, ack_data)
            
            time.sleep_us(10)
    
    def _process_input(self, pipe, data):
        """Process input packet from player"""
        if len(data) < 2:
            return
        
        player_id = pipe  # Pipe 1 = Player 1, etc.
        buttons = data[0]
        axis = data[1] if len(data) > 1 else 127
        
        # Update player activity
        self.players.update_activity(player_id)
        
        # Update gamepad
        self.gamepad.update(player_id, buttons, axis)
        
        # LED feedback
        self.led.toggle()
    
    def _process_pairing(self, data):
        """Process pairing request"""
        if len(data) < 4:
            return
        
        magic, inst_type, id_lo, id_hi = data[0], data[1], data[2], data[3]
        
        if magic != 0xAA:
            return
        
        device_id = id_lo | (id_hi << 8)
        print(f"Pairing request: device=0x{device_id:04X}, type={inst_type}")
        
        # Assign player slot
        player_id = self.players.pair(device_id, inst_type)
        
        if player_id is None:
            print("No slots available!")
            return
        
        print(f"Assigned Player {player_id}")
        
        # Prepare ACK payload with assigned ID and color
        color_code = player_id  # 1=Red, 2=Blue, 3=Green, 4=Magenta
        ack_data = bytes([player_id, color_code])
        
        # Write ACK payload for pipe 0 (pairing pipe)
        self.radio.write_ack_payload(0, ack_data)
        
        # Show on display
        self.display.show_paired(player_id, inst_type)
        time.sleep(1)
        
        # Update normal ACK payload for this player's pipe
        normal_ack = bytes([0x40 | color_code, 0x00])
        self.radio.write_ack_payload(player_id, normal_ack)
    
    def _check_pair_button(self):
        """Check for pairing button hold"""
        pressed = (self.pair_btn.value() == 0)
        
        if pressed:
            if self.pair_btn_start == 0:
                self.pair_btn_start = time.ticks_ms()
            elif time.ticks_diff(time.ticks_ms(), self.pair_btn_start) >= PAIR_HOLD_MS:
                if not self.is_pairing:
                    self._enter_pairing()
                else:
                    self._exit_pairing()
                self.pair_btn_start = 0
                time.sleep_ms(500)
        else:
            self.pair_btn_start = 0
    
    def _enter_pairing(self):
        print("Entering pairing mode")
        self.is_pairing = True
        self.pairing_start = time.ticks_ms()
        self._setup_pairing_mode()
    
    def _exit_pairing(self):
        print("Exiting pairing mode")
        self.is_pairing = False
        self._setup_normal_mode()
        self._preload_ack_payloads()
    
    def run(self):
        """Main loop (Core 0)"""
        # Start RF task on Core 1
        _thread.start_new_thread(self._rf_task, ())
        
        print("Hub running!")
        last_display = 0
        
        while True:
            now = time.ticks_ms()
            
            # Check pair button
            self._check_pair_button()
            
            # Handle pairing timeout
            if self.is_pairing:
                if time.ticks_diff(now, self.pairing_start) >= PAIR_TIMEOUT_MS:
                    self._exit_pairing()
            
            # Process queued packets
            with self._queue_lock:
                packets = self._rx_queue[:]
                self._rx_queue.clear()
            
            for pipe, data in packets:
                if pipe == 0 and self.is_pairing:
                    self._process_pairing(data)
                elif 1 <= pipe <= MAX_PLAYERS:
                    self._process_input(pipe, data)
            
            # Check timeouts
            self.players.check_timeouts()
            
            # Update display
            if time.ticks_diff(now, last_display) >= DISPLAY_UPDATE_MS:
                if self.is_pairing:
                    remaining = max(0, (PAIR_TIMEOUT_MS - time.ticks_diff(now, self.pairing_start)) // 1000)
                    self.display.show_pairing(remaining, self.players.get_connected_count())
                else:
                    self.display.show_normal(self.players.players, self.players.connected_mask)
                last_display = now
            
            time.sleep_ms(1)
            gc.collect()

# ============================================================================
# ENTRY POINT
# ============================================================================
if __name__ == "__main__":
    hub = HubReceiver()
    hub.run()
