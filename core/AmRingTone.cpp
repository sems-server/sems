#include "AmRingTone.h"
#include <math.h>

#include "log.h"

#define PI 3.14159

AmRingTone::AmRingTone(int length, int on, int off, int f, int f2)
  : AmAudio(),
    length(length),
    on_period(on), 
    off_period(off),
    freq(f),freq2(f2)
{}

AmRingTone::~AmRingTone()
{}

int AmRingTone::read(unsigned int user_ts, unsigned int size)
{
  int t = user_ts % ((on_period + off_period)<<3);

  if(length < 0)
    return -1;

  if(t >= on_period<<3){

    memset((unsigned char*)samples,0,size);

    return size;
  }

  short* s = (short*)((unsigned char*)samples);
  for(unsigned int i=0; i<PCM16_B2S(size); i++, s++, t++){

    if(t < on_period<<3){
      float fs = sin((float(t*freq)/(float)SYSTEM_SAMPLECLOCK_RATE)*2.0*PI)*15000.0;
      if(freq2 != 0)
	fs += sin((float(t*freq2)/(float)SYSTEM_SAMPLECLOCK_RATE)*2.0*PI)*15000.0;
      *s = (short)(fs);
    }
    else
      *s = 0;

  }

  if(length != 0){
    length -= PCM16_B2S(size) / 8;
    if(!length)
      length--;
  }

  return size;
}

int AmRingTone::write(unsigned int user_ts, unsigned int size)
{
  return -1;
}
