# Nordic MCU repo
contains projects implemented on HW from "Seeeds nRF52840 Sense"

## programs include 
- blinky: test program for MCU
- blinky_lib: library for on board LED signaling
- microphone_ard + microphone_host/offline_record.py: two programs for voice recording and extraction to pc (includes unused zephyr program "microphone")
- voice_assistent: (NOT COMPLETED) program for command recognition and adequate responce, each command has to start with "name" (list of commands: "name", "status", "sleep", "hi", "one", "two", "three", "four")
- ML: folder including process from dataset adjustment through feature extraction to final learned model
