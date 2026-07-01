"""
record.py — record raw PCM audio streamed over serial from the nRF52840
and save it as a proper .wav file.

Usage:
    python record.py COM5 out.wav            # record until Ctrl+C
    python record.py COM5 out.wav 5          # record for 5 seconds then auto-stop
"""

import sys
import time
import wave
import serial

# --- Audio format must match what the firmware sends ---
SAMPLE_RATE = 16000
SAMPLE_WIDTH_BYTES = 2   # 16-bit samples = 2 bytes each
CHANNELS = 1
CHUNK_SIZE = 4096


def main():
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <COM port, e.g. COM5> <output.wav> [duration-seconds]")
        sys.exit(1)

    port_name = sys.argv[1]
    out_path = sys.argv[2]
    duration = int(sys.argv[3]) if len(sys.argv) >= 4 else None

    ser = serial.Serial(port_name, baudrate=115200, timeout=0.5) # Open the serial/USB-CDC port and after 0.5 read.
    time.sleep(0.3) # let the firmware notice DTR and finish init
    ser.reset_input_buffer() # discard anything stale before we start

    wav_file = wave.open(out_path, "wb") # open in "wb" (write binary) mode gives us an object where we just call setparams()
    wav_file.setnchannels(CHANNELS)
    wav_file.setsampwidth(SAMPLE_WIDTH_BYTES)
    wav_file.setframerate(SAMPLE_RATE)

    total_bytes = 0

    try:
        ser.write(b'R') # sent as raw byte not string
        ack = ser.read(1)
        print("ack:", ack) # should print b'\xaa' if command was received
        print("Recording... Ctrl+C to stop" + (" (or timeout)" if duration else ""))

        start_time = time.time()

        while True:
            if duration is not None and (time.time() - start_time) >= duration:
                break

            chunk = ser.read(CHUNK_SIZE)
            if chunk:
                wav_file.writeframes(chunk)  # appends raw PCM bytes to the wav file
                total_bytes += len(chunk)

    except KeyboardInterrupt:
        # Ctrl+C raises this exception
        pass

    finally:
        # Send stop command, then drain the port for ~1 more second to
        # catch any audio the firmware was still sending when we stopped —
        # same reasoning as the C version's drain loop.
        ser.write(b'S')

        drain_start = time.time()
        while time.time() - drain_start < 1.0: # drain the port for ~1 more second to catch any audio the firmware was still sending
            chunk = ser.read(CHUNK_SIZE)
            if chunk:
                wav_file.writeframes(chunk)
                total_bytes += len(chunk)

        wav_file.close()
        ser.close()

    seconds = total_bytes / (SAMPLE_RATE * CHANNELS * SAMPLE_WIDTH_BYTES)
    print(f"Saved {total_bytes} bytes (~{seconds:.2f} s) to {out_path}")


if __name__ == "__main__":
    main()
