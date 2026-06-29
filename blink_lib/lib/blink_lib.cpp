#include "blink_lib.hpp"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define BLINK_DURATION_MS 100
#define BLINK_GAP_MS 50

static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

//K_MUTEX_DEFINE(dq_mutex); // mutex declatration for locking queue r/w

extern struct k_mutex dq_mutex;
extern struct k_thread blink_thread_data;
extern k_thread_stack_t blink_stack[];

static void blink_thread_fn(void *arg1, void *arg2, void *arg3);
/*
K_THREAD_DEFINE(blink_tid,          // thread object name
                512,                // stack size in bytes
                blink_thread_fn,    // entry function
                NULL, NULL, NULL,   // arguments
                10,                 // priority 1 == HIGH
                0,                  // options
                K_NO_WAIT           // start delay
);
*/
static blink_status *s_instance = nullptr;

static void set_colour(blink_status::Colour c)
{
    // Default all off
    bool r = false, g = false, b = false;

    switch (c)
    {
        case blink_status::Colour::RED:     r=true;                 break;
        case blink_status::Colour::GREEN:           g=true;         break;
        case blink_status::Colour::BLUE:                    b=true; break;
        case blink_status::Colour::YELLOW:  r=true; g=true;         break;
        case blink_status::Colour::MAGENTA: r=true;         b=true; break;
        case blink_status::Colour::CYAN:            g=true; b=true; break;
        case blink_status::Colour::WHITE:   r=true; g=true; b=true; break;
    }

    gpio_pin_set_dt(&led_r, r ? 1 : 0);
    gpio_pin_set_dt(&led_g, g ? 1 : 0);
    gpio_pin_set_dt(&led_b, b ? 1 : 0);
}

static void all_off()
{
    gpio_pin_set_dt(&led_r, 0);
    gpio_pin_set_dt(&led_g, 0);
    gpio_pin_set_dt(&led_b, 0);
}

static void blink_thread_fn(void *, void *, void *) // main thread fnc, never returns
{
    while (s_instance == nullptr) { // Wait until begin() has been called
        k_msleep(10);
    }

    while (true) {
        s_instance->process();
    }
}
/*
blink_status::blink_status(size_t maxSize) 
    : _maxSize(maxSize), _head(0), _tail(0), _count(0) {
    _buf = new Colour[maxSize];

}
blink_status::~blink_status() {
    delete[] _buf;
}
*/
bool blink_status::begin()
{
    // Check all three GPIO devices are ready
    if (!gpio_is_ready_dt(&led_r) ||
        !gpio_is_ready_dt(&led_g) ||
        !gpio_is_ready_dt(&led_b)) {
        return false;
    }

    // init pins to OFF
    if (gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_ACTIVE) < 0 ||
        gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_ACTIVE) < 0 ||
        gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_ACTIVE) < 0) {
        return false;
    }

    s_instance = this;

    // test sequence
    add_colour(Colour::RED);
    add_colour(Colour::GREEN);
    add_colour(Colour::BLUE);
    add_colour(Colour::YELLOW);
    add_colour(Colour::MAGENTA);
    add_colour(Colour::CYAN);
    add_colour(Colour::WHITE);

    k_thread_create(&blink_thread_data,
                blink_stack,
                512,
                blink_thread_fn,
                NULL, NULL, NULL,
                10,       // priority
                0,        // options
                K_NO_WAIT
    );

    return true;
}

bool blink_status::add_colour(Colour colour){
    k_mutex_lock(&dq_mutex, K_FOREVER);

    bool added = false;
    if (size() < _maxSize) {
        push_back(colour);
        added = true;
    }

    k_mutex_unlock(&dq_mutex);
    return added;
}

bool blink_status::add_priority_colour(Colour colour){
    k_mutex_lock(&dq_mutex, K_FOREVER);

    bool added = false;
    if (size() < _maxSize) {
        push_front(colour);
        added = true;
    }

    k_mutex_unlock(&dq_mutex);
    return added;
}

bool blink_status::get_dqueue_full(){
    k_mutex_lock(&dq_mutex, K_FOREVER);
    bool full = (size() >= _maxSize);
    k_mutex_unlock(&dq_mutex);
    return full;
}

void blink_status::process() // attempt to pop from frot o queue
{
    k_mutex_lock(&dq_mutex, K_FOREVER);

    if (empty()) {
        k_mutex_unlock(&dq_mutex); // chill, nothing to pop
        k_msleep(10);
        return;
    }

    Colour c = front();
    pop_front();

    k_mutex_unlock(&dq_mutex);

    set_colour(c); // Blink the colour
    k_msleep(BLINK_DURATION_MS);

    all_off();
    k_msleep(BLINK_GAP_MS);
}

// deque helpers withoud <deque>
size_t blink_status::size()  const { return _count; }
bool   blink_status::empty() const { return _count == 0; }
bool   blink_status::full()  const { return _count >= _maxSize; }

blink_status::Colour blink_status::front() const { return _buf[_head]; }

void blink_status::push_back(Colour c) {
    _buf[_tail] = c;
    _tail = (_tail + 1) % _maxSize;
    _count++;
}

void blink_status::push_front(Colour c) {
    _head = (_head + _maxSize - 1) % _maxSize;
    _buf[_head] = c;
    _count++;
}

void blink_status::pop_front() {
    _head = (_head + 1) % _maxSize;
    _count--;
}