#include <zephyr/kernel.h>
#include <stdio.h>
#include "..\lib\blink_lib.hpp"

static blink_status led_status(7);

int main(void)
{
    if (!led_status.begin()) {
        printf("LED init failed\n");
        return 0;
    }


    while (true){
        k_msleep(2000);
        led_status.add_colour(blink_status::Colour::GREEN); // program working LED
    }
}
