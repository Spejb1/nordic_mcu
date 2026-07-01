#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

class blink_status
{
public:
  blink_status(size_t maxSize) : _maxSize(maxSize), _head(0), _tail(0), _count(0) {
    _buf = new Colour[maxSize];
  }
  ~blink_status() {
    delete[] _buf;
  }

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
    Colour  *_buf;
    size_t   _maxSize;
    size_t   _head;
    size_t   _tail;
    size_t   _count;

    size_t size()  const;
    bool   empty() const;
    bool   full()  const;
    Colour front() const;
    void   push_back(Colour c);
    void   push_front(Colour c);
    void   pop_front();
};