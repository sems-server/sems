#include "AmRingTone.h"
#include <math.h>

#include "log.h"

#define PI 3.14159

AmRingTone::AmRingTone(int length, int on, int off, int f, int f2)
  : AmAudio(),
    on_period(on), 
    off_period(off),
    freq(f),freq2(f2),
    length(length)
{
  if (on_period==0 && off_period==0)
    on_period = 1; // sanity
}

AmRingTone::~AmRingTone()
{}

int AmRingTone::read(unsigned int user_ts, unsigned int size)
{
  int ts_on = on_period*(SYSTEM_SAMPLECLOCK_RATE/1000);
  int ts_off = off_period*(SYSTEM_SAMPLECLOCK_RATE/1000);
  int t = user_ts % (ts_on + ts_off);

  if(length < 0)
    return -1;

  if(t >= ts_on){

    memset((unsigned char*)samples,0,size);

    return size;
  }

  short* s = (short*)((unsigned char*)samples);
  for(unsigned int i=0; i<PCM16_B2S(size); i++, s++, t++){

    if(t < ts_on){
      float fs = sin((float(t*freq)/(float)SYSTEM_SAMPLECLOCK_RATE)*2.0*PI)*15000.0;
      if(freq2 != 0)
	fs += sin((float(t*freq2)/(float)SYSTEM_SAMPLECLOCK_RATE)*2.0*PI)*15000.0;
      *s = (short)(fs);
    }
    else
      *s = 0;

  }

  if(length != 0){
    length -= PCM16_B2S(size) / (SYSTEM_SAMPLECLOCK_RATE/1000);
    if(!length)
      length--;
  }

  return size;
}

int AmRingTone::write(unsigned int user_ts, unsigned int size)
{
  return -1;
}
