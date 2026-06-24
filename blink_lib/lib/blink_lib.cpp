#include "blink_lib.hpp"

bool blink_status::begin(){
    new colour_queue[7];

    for (uint8_t i = 0; i < sizeof(colour_queue); i++)
    {
        add_colour(Colour[i]);
    }
    
}

blink_status::~blink_status(){
    free colour_queue;
}