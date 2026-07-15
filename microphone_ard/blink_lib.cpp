#include "blink_lib.hpp"

#define BLINK_DURATION_MS 150
#define BLINK_GAP_MS       100

// active-LOW LEDs
#define LED_PIN_R  LEDR
#define LED_PIN_G  LEDG
#define LED_PIN_B  LEDB

static void set_colour(blink_status::Colour c)
{
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

    digitalWrite(LED_PIN_R, r ? LOW : HIGH);
    digitalWrite(LED_PIN_G, g ? LOW : HIGH);
    digitalWrite(LED_PIN_B, b ? LOW : HIGH);
}

static void all_off()
{
    digitalWrite(LED_PIN_R, HIGH);
    digitalWrite(LED_PIN_G, HIGH);
    digitalWrite(LED_PIN_B, HIGH);
}

void blink_status::blink_thread_fn(blink_status *self)
{
    while (true) {
        self->process();
    }
}

bool blink_status::begin()
{
    pinMode(LED_PIN_R, OUTPUT);
    pinMode(LED_PIN_G, OUTPUT);
    pinMode(LED_PIN_B, OUTPUT);
    all_off();

    add_colour(Colour::RED);
    add_colour(Colour::GREEN);
    add_colour(Colour::BLUE);
    add_colour(Colour::YELLOW);
    add_colour(Colour::MAGENTA);
    add_colour(Colour::CYAN);
    add_colour(Colour::WHITE);

    _thread.start(mbed::callback(blink_thread_fn, this));

    return true;
}

bool blink_status::add_colour(Colour colour){
    _dq_mutex.lock();

    bool added = false;
    if (size() < _maxSize) {
        push_back(colour);
        added = true;
    }

    _dq_mutex.unlock();
    return added;
}

bool blink_status::add_priority_colour(Colour colour){
    _dq_mutex.lock();

    bool added = false;
    if (size() < _maxSize) {
        push_front(colour);
        added = true;
    }

    _dq_mutex.unlock();
    return added;
}

bool blink_status::get_dqueue_full(){
    _dq_mutex.lock();
    bool full = (size() >= _maxSize);
    _dq_mutex.unlock();
    return full;
}

void blink_status::process()
{
    _dq_mutex.lock();

    if (empty()) {
        _dq_mutex.unlock();
        rtos::ThisThread::sleep_for(std::chrono::milliseconds(10));
        return;
    }

    Colour c = front();
    pop_front();

    _dq_mutex.unlock();

    set_colour(c);
    rtos::ThisThread::sleep_for(std::chrono::milliseconds(BLINK_DURATION_MS));

    all_off();
    rtos::ThisThread::sleep_for(std::chrono::milliseconds(BLINK_GAP_MS));
}

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
