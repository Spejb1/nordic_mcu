#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/audio/dmic.h>
#include <hal/nrf_pdm.h>
#include <stdio.h>
#include "..\lib\blink_lib.hpp"

#define DMIC_NODE  DT_NODELABEL(dmic_dev)
#define UART_NODE  DT_CHOSEN(zephyr_console)

#define SAMPLE_RATE      16000
#define BYTES_PER_SAMPLE 2
#define BLOCK_MS         100
#define BLOCK_SIZE       (SAMPLE_RATE * BYTES_PER_SAMPLE * BLOCK_MS / 1000) /* 3200 B */
#define NUM_BLOCKS       8 // 4 16

K_MEM_SLAB_DEFINE(mem_slab, BLOCK_SIZE, NUM_BLOCKS, 4);
K_SEM_DEFINE(tx_done_sem, 0, 1);

static const uint8_t *tx_buf;
static size_t tx_len;
static size_t tx_pos;

static const struct device *dmic_dev = DEVICE_DT_GET(DMIC_NODE);
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
				expecting_gain_byte = true; /* next byte is the gain value */
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
			uart_irq_tx_disable(dev); // nothing left to send
			k_sem_give(&tx_done_sem);
		}
	}
}

static void uart_send(const uint8_t *data, size_t len)
{
	tx_buf = data;
	tx_len = len;
	tx_pos = 0;
	uart_irq_tx_enable(uart_dev); // start transmission
	k_sem_take(&tx_done_sem, K_FOREVER);
}

static int dmic_start(void)
{
	struct pcm_stream_cfg stream = {
		.pcm_rate   = SAMPLE_RATE,
		.pcm_width  = 16,
		.block_size = BLOCK_SIZE,
		.mem_slab   = &mem_slab,
	};

	struct dmic_cfg cfg = {
		.io = {
			.min_pdm_clk_freq = 1000000,
			.max_pdm_clk_freq = 3500000,
			.min_pdm_clk_dc = 40,
			.max_pdm_clk_dc = 60,
		},
		.streams = &stream,
		.channel = {
			//.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT), // PDM_CHAN_RIGHT
			.req_num_chan = 1,
			.req_num_streams = 1,
		},
	};

	int ret = dmic_configure(dmic_dev, &cfg);
	if (ret) {
		return ret;
	}
	// nrf_pdm_gain_set(NRF_PDM0, NRF_PDM_GAIN_MAXIMUM, NRF_PDM_GAIN_MAXIMUM); // hardcode to be tuned
	// nrf_pdm_gain_set(NRF_PDM0, pdm_gain, pdm_gain);
	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
	if (ret) {
		return ret;
	}
	k_msleep(5);
	nrf_pdm_gain_set(NRF_PDM0, pdm_gain, pdm_gain);

	void *buffer;
	uint32_t size;
	for (int i = 0; i < 10; i++) {
		if (dmic_read(dmic_dev, 0, &buffer, &size, 2000) == 0) {
			k_mem_slab_free(&mem_slab, buffer);
		}
	}

	return 0; // ret
}

static void dmic_debug_measure(void) // AI test fnc for single min max values from microphone
{
	struct pcm_stream_cfg stream = {
		.pcm_rate   = SAMPLE_RATE,
		.pcm_width  = 16,
		.block_size = BLOCK_SIZE,
		.mem_slab   = &mem_slab,
	};
	struct dmic_cfg cfg = {
		.io = {
			.min_pdm_clk_freq = 1000000,
			.max_pdm_clk_freq = 3500000,
			.min_pdm_clk_dc = 40,
			.max_pdm_clk_dc = 60,
		},
		.streams = &stream,
		.channel = {
			//.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT),
			.req_num_chan = 1,
			.req_num_streams = 1,
		},
	};

	char msg[64];
	int len;

	int ret = dmic_configure(dmic_dev, &cfg);
	if (ret) {
		len = snprintf(msg, sizeof(msg), "configure err=%d\r\n", ret);
		uart_send((uint8_t *)msg, len);
		return;
	}

	//nrf_pdm_gain_set(NRF_PDM0, NRF_PDM_GAIN_MAXIMUM, NRF_PDM_GAIN_MAXIMUM);

	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
	k_msleep(5);
	nrf_pdm_gain_set(NRF_PDM0, pdm_gain, pdm_gain);
	if (ret) {
		len = snprintf(msg, sizeof(msg), "trigger start err=%d\r\n", ret);
		uart_send((uint8_t *)msg, len);
		return;
	}

	void *buffer;
	uint32_t size;

	for (int i = 0; i < 10; i++) { // errase fisrt 10 x 100 ms of samples
		if (dmic_read(dmic_dev, 0, &buffer, &size, 2000) == 0) {
			k_mem_slab_free(&mem_slab, buffer);
		}
	}

	ret = dmic_read(dmic_dev, 0, &buffer, &size, 2000);
	dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);

	if (ret == 0) {
		int16_t *samples = (int16_t *)buffer;
		int16_t min_s = samples[0], max_s = samples[0];
		for (uint32_t i = 1; i < size / 2; i++) {
			if (samples[i] < min_s) min_s = samples[i];
			if (samples[i] > max_s) max_s = samples[i];
		}
		len = snprintf(msg, sizeof(msg), "min=%d max=%d size=%u\r\n", min_s, max_s, size);
		k_mem_slab_free(&mem_slab, buffer);
	} else {
		len = snprintf(msg, sizeof(msg), "dmic_read err=%d\r\n", ret);
	}
	uart_send((uint8_t *)msg, len);
}

static blink_status led_status(7);

#ifdef __cplusplus
extern "C" {
#endif

int main(void)
{
	if (!device_is_ready(uart_dev) || !device_is_ready(dmic_dev)) {
		return 0;
	}

	if (!led_status.begin()) {
        printf("LED init failed\n");
        return 0;
    }

	uart_irq_callback_set(uart_dev, uart_cb);
	uart_irq_rx_enable(uart_dev);

	// wait for host to open the port
	uint32_t dtr = 0;
	while (!dtr) {
		uart_line_ctrl_get(uart_dev, UART_LINE_CTRL_DTR, &dtr);
		led_status.add_colour(blink_status::Colour::GREEN); // program working LED
		k_msleep(50);
	}


        while (true){
        
            if (cmd_pending) {
				cmd_pending = false;
				if (cmd_byte == 'R' && !recording) {
                	led_status.add_colour(blink_status::Colour::BLUE);
					uint8_t ack = 0xAA;
    				uart_send(&ack, 1);
					recording = (dmic_start() == 0);
				} else if (cmd_byte == 'S' && recording) {
                	led_status.add_colour(blink_status::Colour::RED);
					dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);
					recording = false;
				} else if (cmd_byte == 'M' && !recording) {
					dmic_debug_measure();
				} else if (cmd_byte == 'G') {
					nrf_pdm_gain_set(NRF_PDM0, pdm_gain, pdm_gain);
					char msg[32];
					int len = snprintf(msg, sizeof(msg), "gain set to %u\r\n", pdm_gain);
					uart_send((uint8_t *)msg, len);
				}
			}

			if (recording) {
				void *buffer;
				uint32_t size;

				if (dmic_read(dmic_dev, 0, &buffer, &size, 2000) == 0) {
					uart_send((uint8_t *)buffer, size);
					k_mem_slab_free(&mem_slab, buffer);
				}
			} else {
				k_msleep(10);
			}
            //k_msleep(2000);
            //led_status.add_colour(blink_status::Colour::GREEN); // program working LED
        }
	return 0;
}

#ifdef __cplusplus
}
#endif
