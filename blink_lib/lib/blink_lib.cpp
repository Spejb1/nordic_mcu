#include "blink_lib.hpp"


bool blink_status::begin(){
    for (uint8_t i = 0; i < dq.size(); i++)
    {
        add_colour(Colour[i]);
    }
    return true;
}

blink_status::~blink_status(){

}

bool blink_status::add_colour(enum Colour colour){
    if (dq.size() >= _maxSize) return false;
    dq.push_back(colour);
    return true;
}

bool blink_status::add_priority_colour(enum Colour colour){
    if(dq.size() >= _maxSize) return false;
    dq.push_front(colour);
    return true;
}

bool blink_status::get_dqueue_full(){
    if(dq.size() >= _maxSize) return true;
    return false;
}
