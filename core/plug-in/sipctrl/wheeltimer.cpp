/*
 * Wheeltimer 
 * 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "AmThread.h"
#include "wheeltimer.h"

#include "log.h"


timer::~timer()
{
    DBG("timer::~timer(this=%p)\n",this);
}



wheeltimer* wheeltimer::_instance=NULL;

wheeltimer* wheeltimer::instance()
{
    if(!_instance)
	_instance = new wheeltimer();

    return _instance;
}

wheeltimer::wheeltimer()
    : wall_clock(0)
{
}

wheeltimer::~wheeltimer()
{
}

void wheeltimer::insert_timer(timer* t)
{
    //add new timer to user request list
    utimer_add_m.lock();
    utimer_add.push(t);
    utimer_add_m.unlock();
}

void wheeltimer::remove_timer(timer* t)
{
    //DBG("wheeltimer::remove_timer(%p)\n",t);

    //add new timer to user request list
    utimer_rem_m.lock();
    utimer_rem.push(t);
    utimer_rem_m.unlock();
}

void wheeltimer::run()
{
  struct timeval now,next_tick,diff,tick;

  tick.tv_sec = 0;
  tick.tv_usec = TIMER_RESOLUTION;
  
  gettimeofday(&now, NULL);
  timeradd(&tick,&now,&next_tick);

  while(true){

    gettimeofday(&now,NULL);

    if(timercmp(&now,&next_tick,<)){

      struct timespec sdiff,rem;
      timersub(&next_tick, &now,&diff);
      
      sdiff.tv_sec = diff.tv_sec;
      sdiff.tv_nsec = diff.tv_usec * 1000;

      if(sdiff.tv_nsec > 2000) // 2 ms 
	nanosleep(&sdiff,&rem);
    }
    //else {
    //printf("missed one tick\n");
    //}

    turn_wheel();
    timeradd(&tick,&next_tick,&next_tick);
  }
}



void wheeltimer::update_wheel(int wheel)
{
    // do not try do update wheel 0
    if(!wheel)
	return;
    
    for(;wheel;wheel--){

	int pos = (wall_clock >> (wheel*BITS_PER_WHEEL))
	    & ((1<<BITS_PER_WHEEL)-1);
	
	timer *t = (timer*)wheels[wheel][pos].next;
	while( t ) {
	    
	    timer* t1 = (timer*)t->next;
	    place_timer(t,wheel-1);
	    t = t1;
	}

	wheels[wheel][pos].next = NULL;
    }
}

void wheeltimer::turn_wheel()
{
    u_int32_t mask = ((1<<BITS_PER_WHEEL)-1); // 0x00 00 00 FF
    int i=0;
	
    //determine which wheel should be updated
    for(;i<WHEELS;i++){
	if((wall_clock & mask) ^ mask)
	    break;
	mask <<= BITS_PER_WHEEL;
    }

    //increment time
    wall_clock++;
		
    // Update existing timer entries
    update_wheel(i);
	
    // Check for timer insertion requests
    utimer_add_m.lock();
    while(!utimer_add.empty()) {
	place_timer(utimer_add.front());
	utimer_add.pop();
    }
    utimer_add_m.unlock();
	
    // Check for timer deletion requests
    utimer_rem_m.lock();
    while(!utimer_rem.empty()) {
	delete_timer(utimer_rem.front());
	utimer_rem.pop();
    }
    utimer_rem_m.unlock();
	
    //DBG("time = %d\n", wall_clock);
	
    //check for expired timer to process
    process_current_timers();
}

void wheeltimer::process_current_timers()
{
    timer *t = (timer *)wheels[0][wall_clock & 0xFF].next;
    
    while(t){

	t->cb(t,t->data1,t->data2);

	timer* t1 = (timer*)t->next;

	t->next = NULL;
	t->prev = NULL;

	t = t1;
    }
    
    wheels[0][wall_clock & 0xFF].next = NULL;
}

inline bool less_ts(unsigned int t1, unsigned int t2)
{
    // t1 < t2
    return (t1 - t2 > (unsigned int)(1<<31));
}

void wheeltimer::place_timer(timer* t)
{
    //DBG("place_timer: type=0x%x, expires=%i\n",
    //     t->type, t->expires);

    // TODO: use ts_less
    if(less_ts(t->expires,wall_clock)){

 	// t->cb(t->data);
	// delete t;
	
	// we put the late ones at the beginning of next wheel turn
	add_timer_to_wheel(t,0,((1<<BITS_PER_WHEEL)-1) & wall_clock);
	
 	return;
    }

    place_timer(t,WHEELS-1);
}

void wheeltimer::place_timer(timer* t, int wheel)
{
    unsigned int pos;
    unsigned int clock_mask = t->expires ^ wall_clock;

    for(; wheel; wheel--){

	if( (clock_mask >> (wheel*BITS_PER_WHEEL))
	    & ((1<<BITS_PER_WHEEL)-1) ) {

	    break;
	}
    }

    // we went down to wheel 0
    pos = (t->expires >> (wheel*BITS_PER_WHEEL)) & ((1<<BITS_PER_WHEEL)-1);
    add_timer_to_wheel(t,wheel,pos);
}

void wheeltimer::add_timer_to_wheel(timer* t, int wheel, unsigned int pos)
{
    t->next = wheels[wheel][pos].next;
    wheels[wheel][pos].next = t;

    if(t->next){
	((timer*)t->next)->prev = t;
    }

    t->prev = &(wheels[wheel][pos]);
}

void wheeltimer::delete_timer(timer* t)
{
    //DBG("delete_timer(%p)\n",t);

    //     if(!t->prev && !t->next){
    // 	DBG("deleting detached timer\n");
    //     }
    //     else {
    // 	DBG("t->prev = %p; t->next = %p\n",t->prev,t->next);
    //     }


    if(t->prev)
	t->prev->next = t->next;

    if(t->next)
	((timer*)t->next)->prev = t->prev;

    delete t;
}

