#pragma once
#include <stdbool.h>
#include <deque>
#include <stdint.h>

class blink_status
{
public:
  blink_status(size_t maxSize) : _maxSize(maxSize) {}
  ~blink_status();

  enum class Colour
  {
    RED,
    GREEN,
    BLUE,
    YELLOW,
    MAGENTA,
    CYAN,
    WHITE
  };

  bool begin();
  bool add_colour(enum Colour);
  bool add_priority_colour(enum Colour);
  bool get_dqueue_full(void);
  void process();

  private:
  std::deque<enum Colour> dq;
  size_t _maxSize;
};