#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/audio/dmic.h>
#include <stdio.h>
#include "..\lib\blink_lib.hpp"

#define DMIC_NODE  DT_NODELABEL(dmic_dev)
#define UART_NODE  DT_CHOSEN(zephyr_console)

#define SAMPLE_RATE      16000
#define BYTES_PER_SAMPLE 2
#define BLOCK_MS         100
#define BLOCK_SIZE       (SAMPLE_RATE * BYTES_PER_SAMPLE * BLOCK_MS / 1000) /* 3200 B */
#define NUM_BLOCKS       4

K_MEM_SLAB_DEFINE(mem_slab, BLOCK_SIZE, NUM_BLOCKS, 4);

static const struct device *dmic_dev = DEVICE_DT_GET(DMIC_NODE);
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

static volatile bool recording = false;
static volatile bool cmd_pending = false;
static uint8_t cmd_byte;

static void uart_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	uint8_t c;

	if (!uart_irq_update(dev)) {
		return;
	}
	if (uart_irq_rx_ready(dev)) {
		while (uart_fifo_read(dev, &c, 1) == 1) {
			cmd_byte = c;
			cmd_pending = true;
		}
	}
}

static void uart_send(const uint8_t *data, size_t len)
{
	size_t sent = 0;

	while (sent < len) {
		int n = uart_fifo_fill(uart_dev, &data[sent], len - sent);

		if (n > 0) {
			sent += n;
		} else {
			k_msleep(1);
		}
	}
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
			.req_num_chan = 1,
			.req_num_streams = 1,
		},
	};

	int ret = dmic_configure(dmic_dev, &cfg);
	if (ret) {
		return ret;
	}
	return dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
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

	/* wait for host to actually open the port */
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
