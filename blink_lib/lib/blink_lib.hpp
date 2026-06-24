#include <stdbool.h>

class blink_status
{
public:
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

  blink_status();
  ~blink_status();

  bool begin();
  bool add_colour(enum Colour);
  bool add_priority_colour(enum Colour);
  bool get_queue_full(void);

  private:
  
  bool queue_full;
};