#!/usr/bin/env python3
"""
offline_test.py — record raw PCM audio (5 s) after MCU dupm of pre-recorded aduio in RAM,
tested on xiao nRF52840 sense. after save it as s .wav file

Usage:
    python offline_record.py COMXX outX.wav
"""

import sys
import time
import wave
import serial

SAMPLE_RATE = 16000
SAMPLE_WIDTH_BYTES = 2
CHANNELS = 1
OFFLINE_SECONDS = 4
OFFLINE_SAMPLES = SAMPLE_RATE * OFFLINE_SECONDS
OFFLINE_BYTES = OFFLINE_SAMPLES * SAMPLE_WIDTH_BYTES  # 96000

TRIM_SECONDS = 0.8 # old 0.5
TRIM_BYTES = int(SAMPLE_RATE * TRIM_SECONDS) * SAMPLE_WIDTH_BYTES

def main():
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <COM port, e.g. COMXX> <output.wav>")
        sys.exit(1)

    port_name = sys.argv[1]
    out_path = sys.argv[2]

    ser = serial.Serial(port_name, baudrate=115200, timeout=5)
    time.sleep(0.3)
    ser.reset_input_buffer()

    print(f"Offline capture start ({OFFLINE_SECONDS}s)")
    ser.write(b'O')

    # Give the firmware time to actually capture before read attempt
    time.sleep(OFFLINE_SECONDS + 1.0)

    print("Reading captured data...")
    data = bytearray()
    deadline = time.time() + 10  # timeout for the full dump to arrive
    while len(data) < OFFLINE_BYTES and time.time() < deadline:
        chunk = ser.read(OFFLINE_BYTES - len(data))
        if chunk:
            data.extend(chunk)

    ser.close()

    print(f"Received {len(data)} of {OFFLINE_BYTES} expected bytes.")

    if len(data) > TRIM_BYTES:
        data = data[TRIM_BYTES:]
    else:
        print("Warning: received less data than the trim length")

    wav_file = wave.open(out_path, "wb")
    wav_file.setnchannels(CHANNELS)
    wav_file.setsampwidth(SAMPLE_WIDTH_BYTES)
    wav_file.setframerate(SAMPLE_RATE)
    wav_file.writeframes(bytes(data))
    wav_file.close()

    print(f"Saved {out_path} ({len(data) / (SAMPLE_RATE * SAMPLE_WIDTH_BYTES):.2f}s)")


if __name__ == "__main__":
    main()
