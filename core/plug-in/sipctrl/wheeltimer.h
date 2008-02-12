/**
 *
 *Wheeltimer
 *
 *
 *
 */

#ifndef _wheeltimer_h_
#define _wheeltimer_h_

#include "AmThread.h"

#include <queue>

#define BITS_PER_WHEEL 8
#define ELMTS_PER_WHEEL (1 << BITS_PER_WHEEL)

// 20 ms == 20000 us
#define TIMER_RESOLUTION 20000

// do not change
#define WHEELS 4

struct timer;
typedef void (*timer_cb)(timer*,unsigned int /*data1*/,void* /*data2*/);

class base_timer
{
public:
    base_timer* next;

    base_timer():next(0) {}
    virtual ~base_timer() {}
};

class timer: public base_timer
{
public:
    base_timer*  prev;
    u_int32_t    expires;

    unsigned int type;
    timer_cb     cb;
    unsigned int data1;
    void*        data2;
    

    timer() 
	: base_timer(),type(0),
	  prev(0), expires(0),cb(0),
	  data1(0),data2(0) 
    {}

    timer(unsigned int timer_type, unsigned int expires, timer_cb cb, int data1, void* data2)
	: base_timer(),type(timer_type),
	  prev(0), expires(expires), cb(cb), 
	  data1(data1),data2(data2)
    {}

    ~timer(); 
    
};


class wheeltimer: public AmThread
{
    static wheeltimer* _instance;

    //the timer wheel
    base_timer wheels[WHEELS][ELMTS_PER_WHEEL];

    //list with timer insertions requests
    std::queue<timer*> utimer_add;
    AmMutex    utimer_add_m;

    //list with timer deletions requests
    std::queue<timer*> utimer_rem;
    AmMutex    utimer_rem_m;

    //list with expired timers
    base_timer utimer_expired;

    wheeltimer();
    ~wheeltimer();

    void turn_wheel();
    void update_wheel(int wheel);

    void place_timer(timer* t);
    void place_timer(timer* t, int wheel);

    void add_timer_to_wheel(timer* t, int wheel, unsigned int pos);
    void delete_timer(timer* t);

    void process_current_timers();

protected:
    void run();
    void on_stop(){}

public:

    static wheeltimer* instance();

    //clock reference
    u_int32_t wall_clock; // 32 bits

    void clock_work();
    void insert_timer(timer* t);
    void remove_timer(timer* t);
};


#endif
