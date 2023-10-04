#ifndef TIMEWHEEL_H
#define TIMEWHEEL_H
#include "TWTimer.h"

class TimeWheel
{
public:
    TimeWheel(/* args */);
    ~TimeWheel();

    TWTimer* add_timer(int timeout);
    void del_timer(TWTimer* timer);

    void tick();
public:
    /* data */
    static const int N=60;
    static const int SI=1;
    TWTimer* slots[N];
    int cur_slot;
};


#endif