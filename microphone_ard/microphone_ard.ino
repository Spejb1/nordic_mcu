// Board package required: "Seeed nRF52 mbed-enabled Boards" for rtos
// Implemented on board: XIAO nRF52840 Sense
//
// Protocol:
//   'R'        -> start recording. Replies with one ack byte 0xAA (commented out)
//   'S'        -> stop recording (commented out)
//   'G' + byte -> set mic gain (0-80, matches PDM.setGain() range), replies with text
//   'O'        -> starts recording and storing voice in memory for 4s, then send

#include <PDM.h>
#include "blink_lib.hpp"

#define SAMPLE_RATE   16000
#define CHANNELS      1
#define BLOCK_SAMPLES 1600
#define BLOCK_BYTES   (BLOCK_SAMPLES * 2)
/*
#define NUM_BLOCKS 8
static int16_t pdm_bufs[NUM_BLOCKS][BLOCK_SAMPLES];
static volatile bool buf_ready[NUM_BLOCKS];
static volatile uint8_t fill_idx = 0;
static volatile uint8_t send_idx = 0;
*/
static bool recording = false;
static uint8_t pdm_gain = 50; // PDM.setGain() range is roughly 0-80 on this core

blink_status led_status(7);
static unsigned long last_heartbeat_ms = 0;
#define HEARTBEAT_INTERVAL_MS 1000

#define OFFLINE_SECONDS   4 // gives 72% dynamic mem usage; 5s tweaks
#define OFFLINE_SAMPLES   (SAMPLE_RATE * OFFLINE_SECONDS)
static int16_t offline_buf[OFFLINE_SAMPLES];
static volatile uint32_t offline_write_pos = 0;
static volatile bool offline_capturing = false;

void onPDMdata_offline() {
	int bytesAvailable = PDM.available();
	if (bytesAvailable <= 0 || !offline_capturing) return;

	int samplesAvailable = bytesAvailable / 2;
	uint32_t remaining = OFFLINE_SAMPLES - offline_write_pos;
	if ((uint32_t)samplesAvailable > remaining) {
		samplesAvailable = remaining;
	}
	if (samplesAvailable > 0) {
		PDM.read(&offline_buf[offline_write_pos], samplesAvailable * 2);
		offline_write_pos += samplesAvailable;
	}
	if (offline_write_pos >= OFFLINE_SAMPLES) {
		offline_capturing = false; // buffer full, stop accepting more
	}
}

static void offline_test() {
  led_status.add_priority_colour(blink_status::Colour::BLUE);
	PDM.onReceive(onPDMdata_offline);
	PDM.setGain(pdm_gain);
	offline_write_pos = 0;
	offline_capturing = true;

	if (!PDM.begin(CHANNELS, SAMPLE_RATE)) {
		return;
	}

	while (offline_capturing) {
		delay(10);
	}
	PDM.end();

	Serial.write((uint8_t *)offline_buf, OFFLINE_SAMPLES * 2);

	//PDM.onReceive(onPDMdata);  // restore normal handler for R/S/M afterward
  led_status.add_priority_colour(blink_status::Colour::RED);
}

void app_alive() {
  unsigned long now = millis();
  if (now - last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS) {
	  last_heartbeat_ms = now;
	  led_status.add_colour(blink_status::Colour::GREEN);
  }
}
/*
void onPDMdata() {
	int bytesAvailable = PDM.available();
	if (bytesAvailable <= 0) {
		return;
	}
	if (bytesAvailable > BLOCK_BYTES) {
		bytesAvailable = BLOCK_BYTES; // safety clamp
	}

	PDM.read(pdm_bufs[fill_idx], bytesAvailable);
	buf_ready[fill_idx] = true;
	fill_idx = (fill_idx + 1) % NUM_BLOCKS;
}

static void send_ack() {
	uint8_t ack = 0xAA;
	Serial.write(&ack, 1);
}

static void start_recording() {
	led_status.add_colour(blink_status::Colour::BLUE);
	send_ack();

	PDM.setGain(pdm_gain);
	if (!PDM.begin(CHANNELS, SAMPLE_RATE)) {
		char msg[32];
		int len = snprintf(msg, sizeof(msg), "PDM.begin failed\r\n");
		Serial.write((uint8_t *)msg, len);
		return;
	}

	for (int i = 0; i < NUM_BLOCKS; i++) {
		buf_ready[i] = false;
	}
	recording = true;
}

static void stop_recording() {
	led_status.add_colour(blink_status::Colour::RED);
	PDM.end();
	recording = false;
}
*/
void setup() {
	Serial.begin(115200);
  led_status.begin();
	while (!Serial) {
		; // wait for host to open the port
    app_alive();
	}
	// PDM.onReceive(onPDMdata);
}

void loop() {
	static bool expecting_gain_byte = false; // obsolete for 'O' or 'G'

	while (Serial.available() > 0) {
		uint8_t c = Serial.read();

		if (expecting_gain_byte) {
			pdm_gain = c;
			expecting_gain_byte = false;
			PDM.setGain(pdm_gain);
			//char msg[32];
			//int len = snprintf(msg, sizeof(msg), "gain set to %u\r\n", pdm_gain);
			//Serial.write((uint8_t *)msg, len);
		} else if (c == 'G') {
			expecting_gain_byte = true;
		} else if (c == 'R' && !recording) {
			//start_recording();
		} else if (c == 'S' && recording) {
			//stop_recording();
    } else if (c == 'O' && !recording) {
      offline_test();
		}
	}
  /*
	if (recording && buf_ready[send_idx]) {
		Serial.write((uint8_t *)pdm_bufs[send_idx], BLOCK_BYTES);
		buf_ready[send_idx] = false;
		send_idx = (send_idx + 1) % NUM_BLOCKS;
	}
  */
  app_alive();
}
