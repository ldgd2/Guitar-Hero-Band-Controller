"""
============================================================================
BOOT.PY - USB HID Configuration for Raspberry Pi Pico
============================================================================
This file MUST be named 'boot.py' and placed in the root of the Pico.
It configures the Pico as a USB HID Gamepad at boot time.

Copy this file to the Pico BEFORE running dongle_receiver.py
============================================================================
"""

import usb_hid

# Standard Gamepad HID Report Descriptor
# 16 buttons + 2 axes (X = whammy, Y = unused)
GAMEPAD_REPORT_DESCRIPTOR = bytes([
    0x05, 0x01,        # Usage Page (Generic Desktop)
    0x09, 0x05,        # Usage (Gamepad)
    0xA1, 0x01,        # Collection (Application)
    0x85, 0x01,        #   Report ID (1)
    
    # 16 Buttons
    0x05, 0x09,        #   Usage Page (Button)
    0x19, 0x01,        #   Usage Minimum (Button 1)
    0x29, 0x10,        #   Usage Maximum (Button 16)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x10,        #   Report Count (16)
    0x81, 0x02,        #   Input (Data,Var,Abs)
    
    # X Axis (Whammy Bar) - 0 to 255
    0x05, 0x01,        #   Usage Page (Generic Desktop)
    0x09, 0x30,        #   Usage (X)
    0x15, 0x00,        #   Logical Minimum (0)
    0x26, 0xFF, 0x00,  #   Logical Maximum (255)
    0x75, 0x08,        #   Report Size (8)
    0x95, 0x01,        #   Report Count (1)
    0x81, 0x02,        #   Input (Data,Var,Abs)
    
    # Y Axis (Reserved for future use)
    0x09, 0x31,        #   Usage (Y)
    0x81, 0x02,        #   Input (Data,Var,Abs)
    
    0xC0               # End Collection
])

# Create gamepad device
gamepad = usb_hid.Device(
    report_descriptor=GAMEPAD_REPORT_DESCRIPTOR,
    usage_page=0x01,           # Generic Desktop
    usage=0x05,                # Gamepad
    report_ids=(1,),           # Report ID 1
    in_report_lengths=(5,),    # 1 byte report ID + 2 bytes buttons + 2 bytes axes
    out_report_lengths=(0,),   # No output reports
)

# Enable ONLY the gamepad (disable keyboard/mouse)
usb_hid.enable((gamepad,))

print("USB HID Gamepad configured!")
