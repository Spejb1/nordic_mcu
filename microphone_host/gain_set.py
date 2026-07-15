#!/usr/bin/env python3
"""
gain_sweep.py — sets PDM mic gain to a user defined value via the 'G' command

Arduino PDM.setGain() range on this core is 0-80 (capped at this values)

Usage:
    python gain_set.py COMXX XX
"""

import sys
import time
import serial

if len(sys.argv) < 3:
    print(f"usage: {sys.argv[0]} <COM port, e.g. COM17> <gain 0-80>")
    sys.exit(1)

port_name = sys.argv[1]
gain = int(sys.argv[2])

if not (0 <= gain <= 80):
    print("gain must be between 0 and 80")
    sys.exit(1)

ser = serial.Serial(port_name, baudrate=115200, timeout=2.5)
time.sleep(0.3)
ser.reset_input_buffer()

ser.write(bytes([ord('G'), gain]))
reply = ser.read(64)

print(f"gain={gain}: {reply.strip()}")

ser.close()