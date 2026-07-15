#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <nrfx.h>
#include <drivers/nrfx_errors.h>
#include <nrfx_pdm.h>
#include <hal/nrf_pdm.h>
#include <stdio.h>
#include "..\lib\blink_lib.hpp"

static nrfx_pdm_t pdm_instance = NRFX_PDM_INSTANCE(DT_REG_ADDR(DT_NODELABEL(pdm0)));

#define UART_NODE  DT_CHOSEN(zephyr_console)

#define SAMPLE_RATE      16000
#define BYTES_PER_SAMPLE 2
#define BLOCK_MS         100
#define BLOCK_SIZE       (SAMPLE_RATE * BYTES_PER_SAMPLE * BLOCK_MS / 1000) // 3200 B
#define PDM_BUF_SAMPLES  (SAMPLE_RATE * BLOCK_MS / 1000)                    // 1600 samples
#define NUM_BLOCKS       4

// MIC_CLK = P1.00  -> nrfx absolute pin number 32 (P1.x = 32 + x)
// MIC_DIN = P0.16  -> nrfx absolute pin number 16 (P0.x = x)
#define PDM_CLK_PIN  32
#define PDM_DIN_PIN  16

static int16_t pdm_bufs[NUM_BLOCKS][PDM_BUF_SAMPLES];
static volatile bool buf_ready[NUM_BLOCKS];
static volatile uint8_t next_fill_idx = 0;
static volatile uint8_t next_send_idx = 0;

K_SEM_DEFINE(tx_done_sem, 0, 1);

static const uint8_t *tx_buf;
static size_t tx_len;
static size_t tx_pos;

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

static volatile bool recording = false;
static volatile bool cmd_pending = false;
static uint8_t cmd_byte;
static uint8_t pdm_gain = NRF_PDM_GAIN_DEFAULT;

static void uart_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	uint8_t c;
	static bool expecting_gain_byte = false;

	if (!uart_irq_update(dev)) {
		return;
	}
	if (uart_irq_rx_ready(dev)) {
		while (uart_fifo_read(dev, &c, 1) == 1) {
			if (expecting_gain_byte) {
				pdm_gain = c;
				expecting_gain_byte = false;
				cmd_byte = 'G';
				cmd_pending = true;
			} else if (c == 'G') {
				expecting_gain_byte = true;
			} else {
				cmd_byte = c;
				cmd_pending = true;
			}
		}
	}
	if (uart_irq_tx_ready(dev)) {
		if (tx_pos < tx_len) {
			int n = uart_fifo_fill(dev, &tx_buf[tx_pos], tx_len - tx_pos);
			if (n > 0) {
				tx_pos += n;
			}
		}
		if (tx_pos >= tx_len) {
			uart_irq_tx_disable(dev);
			k_sem_give(&tx_done_sem);
		}
	}
}

static void uart_send(const uint8_t *data, size_t len)
{
	tx_buf = data;
	tx_len = len;
	tx_pos = 0;
	uart_irq_tx_enable(uart_dev);
	k_sem_take(&tx_done_sem, K_FOREVER);
}

static void pdm_event_handler(nrfx_pdm_evt_t const *evt)
{
	if (evt->error != NRFX_PDM_NO_ERROR) {
		return;
	}

	if (evt->buffer_requested) {
		// Hand nrfx the next free buffer immediately so DMA never stalls
		nrfx_pdm_buffer_set(&pdm_instance, pdm_bufs[next_fill_idx], PDM_BUF_SAMPLES);
		next_fill_idx = (next_fill_idx + 1) % NUM_BLOCKS;
	}

	if (evt->buffer_released != NULL) {
		// Figures out which ring slot this was and mark it ready to send
		for (int i = 0; i < NUM_BLOCKS; i++) {
			if (evt->buffer_released == pdm_bufs[i]) {
				buf_ready[i] = true;
				break;
			}
		}
	}
}

static int pdm_start(void) // init of nrfx's PDM
{
	nrfx_pdm_config_t config = NRFX_PDM_DEFAULT_CONFIG(PDM_CLK_PIN, PDM_DIN_PIN);
	config.mode           = NRF_PDM_MODE_MONO;
	config.edge           = NRF_PDM_EDGE_LEFTFALLING;
	// config.clock_freq      = NRF_PDM_FREQ_1032K;
	// config.sample_rate     = NRFX_PDM_RATIO_64; // about 16 kHz output at this clock

	// IRQ connected the PDM
	IRQ_CONNECT(DT_IRQN(DT_NODELABEL(pdm0)), DT_IRQ(DT_NODELABEL(pdm0), priority), nrfx_pdm_irq_handler, NULL, 0);
	irq_enable(DT_IRQN(DT_NODELABEL(pdm0)));

	int err = nrfx_pdm_init(&pdm_instance, &config, pdm_event_handler);
	if (err != NRFX_SUCCESS) {
		char msg[48];
		int len = snprintf(msg, sizeof(msg), "pdm_init failed err=%d\r\n", err);
		uart_send((uint8_t *)msg, len);
		return -1;
	}

	nrf_pdm_gain_set(NRF_PDM0, pdm_gain, pdm_gain);

	err = nrfx_pdm_start(&pdm_instance);
	if (err != NRFX_SUCCESS) {
		char msg[48];
		int len = snprintf(msg, sizeof(msg), "pdm_start failed err=%d\r\n", err);
		uart_send((uint8_t *)msg, len);
		return -1;
	}

	// Discard the first several blocks (startup transient), same idea as before.
	for (int i = 0; i < 10; i++) {
		for (int j = 0; j < NUM_BLOCKS; j++) {
			buf_ready[j] = false;
		}
		k_msleep(BLOCK_MS);
	}

	return 0;
}

// Waits for the next ready buffer and reports real min/max from it
static void pdm_debug_measure(void)
{
	char msg[64];
	int len;

	int64_t start = k_uptime_get();
	while (!buf_ready[next_send_idx]) {
		if (k_uptime_get() - start > 2000) {
			len = snprintf(msg, sizeof(msg), "timeout waiting for buffer\r\n");
			uart_send((uint8_t *)msg, len);
			return;
		}
		k_msleep(5);
	}

	int16_t *samples = pdm_bufs[next_send_idx];
	int16_t min_s = samples[0], max_s = samples[0];
	for (int i = 1; i < PDM_BUF_SAMPLES; i++) {
		if (samples[i] < min_s) min_s = samples[i];
		if (samples[i] > max_s) max_s = samples[i];
	}
	len = snprintf(msg, sizeof(msg), "min=%d max=%d size=%u\r\n",
		       min_s, max_s, (unsigned)(PDM_BUF_SAMPLES * sizeof(int16_t)));

	buf_ready[next_send_idx] = false;
	next_send_idx = (next_send_idx + 1) % NUM_BLOCKS;

	uart_send((uint8_t *)msg, len);
}

static blink_status led_status(7);

#ifdef __cplusplus
extern "C" {
#endif

int main(void)
{
	if (!device_is_ready(uart_dev)) {
		return 0;
	}

	if (!led_status.begin()) {
		printf("LED init failed\n");
		return 0;
	}

	uart_irq_callback_set(uart_dev, uart_cb);
	uart_irq_rx_enable(uart_dev);

	uint32_t dtr = 0;
	while (!dtr) {
		uart_line_ctrl_get(uart_dev, UART_LINE_CTRL_DTR, &dtr);
		led_status.add_colour(blink_status::Colour::GREEN);
		k_msleep(50);
	}

	while (true) {

		if (cmd_pending) {
			cmd_pending = false;
			if (cmd_byte == 'R' && !recording) {
				led_status.add_colour(blink_status::Colour::BLUE);
				uint8_t ack = 0xAA;
				uart_send(&ack, 1);
				recording = (pdm_start() == 0);
			} else if (cmd_byte == 'S' && recording) {
				led_status.add_colour(blink_status::Colour::RED);
				nrfx_pdm_stop(&pdm_instance);
				nrfx_pdm_uninit(&pdm_instance);
				recording = false;
			} else if (cmd_byte == 'M' && !recording) {
				pdm_debug_measure();
			} else if (cmd_byte == 'G') {
				nrf_pdm_gain_set(NRF_PDM0, pdm_gain, pdm_gain);
				char msg[32];
				int len = snprintf(msg, sizeof(msg), "gain set to %u\r\n", pdm_gain);
				uart_send((uint8_t *)msg, len);
			}
		}

		if (recording) {
			// replaces dmic_read() - checks the ring buffer flag.
			if (buf_ready[next_send_idx]) {
				uint8_t *buffer = (uint8_t *)pdm_bufs[next_send_idx];
				uart_send(buffer, BLOCK_SIZE);
				buf_ready[next_send_idx] = false;
				next_send_idx = (next_send_idx + 1) % NUM_BLOCKS;
			} else {
				k_msleep(2);
			}
		} else {
			k_msleep(10);
		}
	}
	return 0;
}

#ifdef __cplusplus
}
#endif