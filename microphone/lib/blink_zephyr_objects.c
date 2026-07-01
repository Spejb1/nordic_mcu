#include <zephyr/kernel.h>

K_MUTEX_DEFINE(dq_mutex);
K_THREAD_STACK_DEFINE(blink_stack, 512);
struct k_thread blink_thread_data;