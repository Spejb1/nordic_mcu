#include <zephyr/kernel.h>
#include<stdio.h>

uint8_t constexpr dqueue_size = 7;

int main(void)
{
    blink_led_status blink_status(dqueue_size);
    blink_led_status.begin();


    while (true){

    }
}
