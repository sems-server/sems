/*****************************************************************************
 * band-limited sinc resampling
 *
 * written by Stefan Keller <skeller@zahlenfresser.de>
 * placed into the public domain
 *****************************************************************************/

/*
As the sampling theorem shows, a bandlimited signal can be fully reconstructed
using a reconstruction low-pass.
This reconstruction at real point t across the time domain can be viewed as a 
superposition of an infinite number of sinc functions scaled by the sample
values and shifted in time to the sample time.
This resampling code takes 14 sample values (seven before the actual point,
seven after) and calculates the superposition.
*/

#include "resample.h"
#include <cstring>
#include <cmath>
#include <cstdlib>

#define MOVE_THRESH 160
#define FILTER_DELAY_SINC_MONO 7
#define FILTER_DELAY_SINC_STEREO 14
#define FILTER_DELAY_LIN_MONO 1
#define FILTER_DELAY_LIN_STEREO 2

#ifndef PI		/* Sometimes in math.h */
#define PI		3.14159265358979323846
#endif

bool ResampleSincMono::sinc_initialized = false;
/* wee need 16 (instead of 15) because of linear interpolation:
 * if currfrc == 255, currfrc+1 will point to the element after the last */
float ResampleSincMono::sinc[16][256];

//--- Resample --------------------------------------------------------------//

Resample::Resample() : filter_delay(0), current(0), curfrc(0), pad_samples(0)
{
}

int Resample::put_samples(signed short* samples, unsigned int num_samples)
{
	if(current >= MOVE_THRESH) {
		this->samples.erase(this->samples.begin(), this->samples.begin()+current-filter_delay);
		current = filter_delay;
	}

	for(unsigned i=0; i<num_samples; i++) {
		this->samples.push_back((float)samples[i]);
	}

	return num_samples;
}

//--- ResampleSincMono ------------------------------------------------------//

ResampleSincMono::ResampleSincMono()
{
	filter_delay = FILTER_DELAY_SINC_MONO;
	if(!sinc_initialized) {
		init_sinc();
		sinc_initialized = true;
	}
	current = filter_delay;
	samples.assign(filter_delay, 0.0);
}

ResampleSincMono::ResampleSincMono(bool do_pad, float max_rate)
{
	filter_delay = FILTER_DELAY_SINC_MONO;
	if(!sinc_initialized) {
		init_sinc();
		sinc_initialized = true;
	}
	if(do_pad) {
		pad_samples = (int)((float)filter_delay * max_rate + 0.5);
	}
	samples.assign(filter_delay, 0.0);
	current = filter_delay;
}


void ResampleSincMono::init_sinc()
{
	int i, k;

	// calculate sinc table
	for (i=0; i<15; i++) {
		for(k=0; k<256; k++) {
			if(i == 7 && k == 0) sinc[i][k] = 1.0;
			else sinc[i][k] = sin((i + k/256.0 - 7) * PI) / ((i + k/256.0 - 7) * PI);
		}
	}
	// apply hamming window
	for (i=0; i<15; i++) {
		for(k=0; k<256; k++) {
			sinc[i][k] *= (0.54 + 0.46 * cos(2.0*PI*(i + k/256.0 - 7) / 16));
		}
	}
	memset(&sinc[15], 0, sizeof(*sinc[15]));
}

inline float ResampleSincMono::upsample(float *src, unsigned int current, unsigned int curfrc)
{
	float sinc_val, curfloat;
	float samp;
	curfloat = (curfrc & 0xffffff) / 16777216.0;

	// linear interpolation between sinc values
	sinc_val = (sinc[14][(curfrc>>24)+1] - sinc[14][curfrc>>24]) * curfloat + sinc[14][curfrc>>24];
	samp = src[current-7]*sinc_val;

	sinc_val = (sinc[13][(curfrc>>24)+1] - sinc[13][curfrc>>24]) * curfloat + sinc[13][curfrc>>24];
	samp += src[current-6]*sinc_val;

	sinc_val = (sinc[12][(curfrc>>24)+1] - sinc[12][curfrc>>24]) * curfloat + sinc[12][curfrc>>24];
	samp += src[current-5]*sinc_val;

	sinc_val = (sinc[11][(curfrc>>24)+1] - sinc[11][curfrc>>24]) * curfloat + sinc[11][curfrc>>24];
	samp += src[current-4]*sinc_val;

	sinc_val = (sinc[10][(curfrc>>24)+1] - sinc[10][curfrc>>24]) * curfloat + sinc[10][curfrc>>24];
	samp += src[current-3]*sinc_val;

	sinc_val = (sinc[9][(curfrc>>24)+1] - sinc[9][curfrc>>24]) * curfloat + sinc[9][curfrc>>24];
	samp += src[current-2]*sinc_val;

	sinc_val = (sinc[8][(curfrc>>24)+1] - sinc[8][curfrc>>24]) * curfloat + sinc[8][curfrc>>24];
	samp += src[current-1]*sinc_val;

	sinc_val = (sinc[7][(curfrc>>24)+1] - sinc[7][curfrc>>24]) * curfloat + sinc[7][curfrc>>24];
	samp += src[current]*sinc_val;

	sinc_val = (sinc[6][(curfrc>>24)+1] - sinc[6][curfrc>>24]) * curfloat + sinc[6][curfrc>>24];
	samp += src[current+1]*sinc_val;

	sinc_val = (sinc[5][(curfrc>>24)+1] - sinc[5][curfrc>>24]) * curfloat + sinc[5][curfrc>>24];
	samp += src[current+2]*sinc_val;

	sinc_val = (sinc[4][(curfrc>>24)+1] - sinc[4][curfrc>>24]) * curfloat + sinc[4][curfrc>>24];
	samp += src[current+3]*sinc_val;

	sinc_val = (sinc[3][(curfrc>>24)+1] - sinc[3][curfrc>>24]) * curfloat + sinc[3][curfrc>>24];
	samp += src[current+4]*sinc_val;

	sinc_val = (sinc[2][(curfrc>>24)+1] - sinc[2][curfrc>>24]) * curfloat + sinc[2][curfrc>>24];
	samp += src[current+5]*sinc_val;

	sinc_val = (sinc[1][(curfrc>>24)+1] - sinc[1][curfrc>>24]) * curfloat + sinc[1][curfrc>>24];
	samp += src[current+6]*sinc_val;

	sinc_val = (sinc[0][(curfrc>>24)+1] - sinc[0][curfrc>>24]) * curfloat + sinc[0][curfrc>>24];
	samp += src[current+7]*sinc_val;

	return samp;
}

inline float ResampleSincMono::downsample(float *src, unsigned int current, unsigned int curfrc, unsigned long long sinc_increment, float scale)
{
	long long sinc_current;
	float samp;

	sinc_current = ((sinc_increment * curfrc)>>32) + ((long long)7<<32);
	sinc_current -= sinc_increment * 7;

	// todo: linear interpolation between sinc values
	samp = src[current+7]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current+6]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current+5]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current+4]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current+3]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current+2]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current+1]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current-1]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current-2]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current-3]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current-4]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current-5]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current-6]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current-7]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	// scale sinc factor:
	samp *= scale;

	return samp;
}

int ResampleSincMono::resample(signed short *dst, float rate, unsigned num_samples)
{
	float samp;

	unsigned long long current_fixed;
	long long increment_fixed;
	int done = 0;

	increment_fixed = (1.0/rate) * 4294967296.0;
	current_fixed = (((unsigned long long)current<<32)&0xFFFFFFFF00000000LL) | (curfrc & 0xffffffff);

	if(pad_samples) {
		if(num_samples < pad_samples) {
			pad_samples = 0;
			return 0;
		}
		for(unsigned i=0; i<pad_samples; i++) {
			*dst = 0; dst++;
		}
		done += pad_samples;
		num_samples -= pad_samples;
		pad_samples = 0;
	}

	if(rate < 1.0)  {	// downsampling, need to scale sinc apropriatly
		// calculate sinc increment
		// todo: with all tis crap here, a floating point increment
		//       (for the resampling as well) may be better (and faster)
		unsigned long long sinc_increment = rate * 4294967296.0;

		// this code stretches the sinc to fit the new cut-off frequency
		// (at 1/2 of our new fs, which is lower than 1/2 of the sample fs)
		// this stretching is done by calculating the sinc increment above
		// finally the new sinc has to be scaled to fit this new filter
		// (the def. for the lowpass is 2.0 * fc * sinc(...))
		// this is done at the end

		while(num_samples > 0) {
			unsigned int current = (current_fixed>>32) & 0xffffffff;
			unsigned int curfrc = current_fixed & 0xffffffff;

			if(current >= samples.size() - filter_delay) break;

			samp = ResampleSincMono::downsample(&samples[0], current, curfrc, sinc_increment, rate);

			// should never happen, but maybe because of rounding etc. it does:
			if(samp > 32767.0) samp = 32767.0;
			else if (samp < -32768.0) samp = -32768.0;

			*dst = (short)samp; dst++;

			current_fixed += increment_fixed;

			num_samples--; done++;
		}
	} else if (rate > 1.0 || this->curfrc) {
		while(num_samples > 0) {
			unsigned int current = (current_fixed>>32) & 0xffffffff;
			unsigned int curfrc = current_fixed & 0xffffffff;

			if(current >= samples.size() - filter_delay) break;

			samp = ResampleSincMono::upsample(&samples[0], current, curfrc);

			// should never happen, but maybe because of rounding etc. it does:
			if(samp > 32767.0) samp = 32767.0;
			else if (samp < -32768.0) samp = -32768.0;

			*dst = (short)samp; dst++;

			current_fixed += increment_fixed;

			num_samples--; done++;
		}
	} else {
		/* rate == 1.0 && curfrc == 0 */
		/* just copy */
		unsigned int current = this->current;
		while(num_samples > 0) {
			if(current >= samples.size() - filter_delay) break;

			*dst = (short)samples[current]; dst++;

			current++;

			num_samples--; done++;
		}
		current_fixed = (((unsigned long long)current<<32)&0xFFFFFFFF00000000LL);
	}

	this->current = (current_fixed>>32) & 0xffffffff;
	this->curfrc = current_fixed & 0xffffffff;

	return done;
}


//--- ResampleSincStereo ----------------------------------------------------//

ResampleSincStereo::ResampleSincStereo()
{
	filter_delay = FILTER_DELAY_SINC_STEREO;
	current = filter_delay;
	samples.assign(filter_delay, 0.0);
}

ResampleSincStereo::ResampleSincStereo(bool do_pad, float max_rate)
{
	filter_delay = FILTER_DELAY_SINC_STEREO;
	if(do_pad) {
		pad_samples = (int)((float)(filter_delay/2) * max_rate + 0.5);
	}
	samples.assign(filter_delay, 0.0);
	current = filter_delay;
}

inline float ResampleSincStereo::upsample(float *src, unsigned int current, unsigned int curfrc)
{
	float sinc_val, curfloat;
	float samp;
	curfloat = (curfrc & 0xffffff) / 16777216.0;

	// linear interpolation between sinc values
	sinc_val = (sinc[14][(curfrc>>24)+1] - sinc[14][curfrc>>24]) * curfloat + sinc[14][curfrc>>24];
	samp = src[current*2-14]*sinc_val;

	sinc_val = (sinc[13][(curfrc>>24)+1] - sinc[13][curfrc>>24]) * curfloat + sinc[13][curfrc>>24];
	samp += src[current*2-12]*sinc_val;

	sinc_val = (sinc[12][(curfrc>>24)+1] - sinc[12][curfrc>>24]) * curfloat + sinc[12][curfrc>>24];
	samp += src[current*2-10]*sinc_val;

	sinc_val = (sinc[11][(curfrc>>24)+1] - sinc[11][curfrc>>24]) * curfloat + sinc[11][curfrc>>24];
	samp += src[current*2-8]*sinc_val;

	sinc_val = (sinc[10][(curfrc>>24)+1] - sinc[10][curfrc>>24]) * curfloat + sinc[10][curfrc>>24];
	samp += src[current*2-6]*sinc_val;

	sinc_val = (sinc[9][(curfrc>>24)+1] - sinc[9][curfrc>>24]) * curfloat + sinc[9][curfrc>>24];
	samp += src[current*2-4]*sinc_val;

	sinc_val = (sinc[8][(curfrc>>24)+1] - sinc[8][curfrc>>24]) * curfloat + sinc[8][curfrc>>24];
	samp += src[current*2-2]*sinc_val;

	sinc_val = (sinc[7][(curfrc>>24)+1] - sinc[7][curfrc>>24]) * curfloat + sinc[7][curfrc>>24];
	samp += src[current*2]*sinc_val;

	sinc_val = (sinc[6][(curfrc>>24)+1] - sinc[6][curfrc>>24]) * curfloat + sinc[6][curfrc>>24];
	samp += src[current*2+2]*sinc_val;

	sinc_val = (sinc[5][(curfrc>>24)+1] - sinc[5][curfrc>>24]) * curfloat + sinc[5][curfrc>>24];
	samp += src[current*2+4]*sinc_val;

	sinc_val = (sinc[4][(curfrc>>24)+1] - sinc[4][curfrc>>24]) * curfloat + sinc[4][curfrc>>24];
	samp += src[current*2+6]*sinc_val;

	sinc_val = (sinc[3][(curfrc>>24)+1] - sinc[3][curfrc>>24]) * curfloat + sinc[3][curfrc>>24];
	samp += src[current*2+8]*sinc_val;

	sinc_val = (sinc[2][(curfrc>>24)+1] - sinc[2][curfrc>>24]) * curfloat + sinc[2][curfrc>>24];
	samp += src[current*2+10]*sinc_val;

	sinc_val = (sinc[1][(curfrc>>24)+1] - sinc[1][curfrc>>24]) * curfloat + sinc[1][curfrc>>24];
	samp += src[current*2+12]*sinc_val;

	sinc_val = (sinc[0][(curfrc>>24)+1] - sinc[0][curfrc>>24]) * curfloat + sinc[0][curfrc>>24];
	samp += src[current*2+14]*sinc_val;

	return samp;
}

inline float ResampleSincStereo::downsample(float *src, unsigned int current, unsigned int curfrc, unsigned long long sinc_increment, float scale)
{
	long long sinc_current;
	float samp;

	sinc_current = ((sinc_increment * curfrc)>>32) + ((long long)7<<32);
	sinc_current -= sinc_increment * 7;

	// todo: linear interpolation between sinc values
	samp = src[current*2+14]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2+12]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2+10]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2+8]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2+6]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2+4]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2+2]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2-2]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2-4]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2-6]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2-8]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2-10]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2-12]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	sinc_current += sinc_increment;
	samp += src[current*2-14]*sinc[(sinc_current>>32) & 0xffffffff][(sinc_current & 0xffffffff)>>24];

	// scale sinc factor:
	samp *= scale;

	return samp;
}

int ResampleSincStereo::resample(signed short *dst, float rate, unsigned num_samples)
{
	float samp_l;
	float samp_r;

	unsigned long long current_fixed;
	long long increment_fixed;
	int done = 0;

	increment_fixed = (1.0/rate) * 4294967296.0;
	current_fixed = (((unsigned long long)current<<32)&0xFFFFFFFF00000000LL) | (curfrc & 0xffffffff);

	if(pad_samples) {
		if(num_samples < pad_samples) {
			pad_samples = 0;
			return 0;
		}
		for(unsigned i=0; i<pad_samples; i++) {
			*dst = 0; dst++;
			*dst = 0; dst++;
		}
		done += pad_samples;
		num_samples -= pad_samples;
		pad_samples = 0;
	}

	if(rate < 1.0)  {	// downsampling, need to scale sinc apropriatly
		// calculate sinc increment
		// todo: with all tis crap here, a floating point increment
		//       (for the resampling as well) may be better (and faster)
		unsigned long long sinc_increment = rate * 4294967296.0;

		// this code stretches the sinc to fit the new cut-off frequency
		// (at 1/2 of our new fs, which is lower than 1/2 of the sample fs)
		// this stretching is done by calculating the sinc increment above
		// finally the new sinc has to be scaled to fit this new filter
		// (the def. for the lowpass is 2.0 * fc * sinc(...))
		// this is done at the end

		while(num_samples > 0) {
			unsigned int current = (current_fixed>>32) & 0xffffffff;
			unsigned int curfrc = current_fixed & 0xffffffff;

			if((current*2) >= samples.size() - filter_delay - 1) break;

			samp_l = ResampleSincStereo::downsample(&samples[0], current, curfrc, sinc_increment, rate);
			samp_r = ResampleSincStereo::downsample(&samples[1], current, curfrc, sinc_increment, rate);

			// should never happen, but maybe because of rounding etc. it does:
			if(samp_l > 32767.0) samp_l = 32767.0;
			else if (samp_l < -32768.0) samp_l = -32768.0;
			if(samp_r > 32767.0) samp_r = 32767.0;
			else if (samp_r < -32768.0) samp_r = -32768.0;

			*dst = (short)samp_l; dst++;
			*dst = (short)samp_r; dst++;

			current_fixed += increment_fixed;

			num_samples--; done++;
		}
	} else if (rate > 1.0 || this->curfrc) {
		while(num_samples > 0) {
			unsigned int current = (current_fixed>>32) & 0xffffffff;
			unsigned int curfrc = current_fixed & 0xffffffff;

			if((current*2) >= samples.size() - filter_delay - 1) break;

			samp_l = ResampleSincStereo::upsample(&samples[0], current, curfrc);
			samp_r = ResampleSincStereo::upsample(&samples[1], current, curfrc);

			// should never happen, but maybe because of rounding etc. it does:
			if(samp_l > 32767.0) samp_l = 32767.0;
			else if (samp_l < -32768.0) samp_l = -32768.0;
			if(samp_r > 32767.0) samp_r = 32767.0;
			else if (samp_r < -32768.0) samp_r = -32768.0;

			*dst = (short)samp_l; dst++;
			*dst = (short)samp_r; dst++;

			current_fixed += increment_fixed;

			num_samples--; done++;
		}
	} else {
		/* rate == 1.0 && curfrc == 0 */
		/* just copy */
		unsigned int current = this->current;
		while(num_samples > 0) {
			if((current*2) >= samples.size() - filter_delay - 1) break;

			*dst = (short)samples[current*2]; dst++;
			*dst = (short)samples[current*2+1]; dst++;

			current++;

			num_samples--; done++;
		}
		current_fixed = (((unsigned long long)current<<32)&0xFFFFFFFF00000000LL);
	}

	this->current = (current_fixed>>32) & 0xffffffff;
	this->curfrc = current_fixed & 0xffffffff;

	return done;
}

//--- ResampleLinMono -------------------------------------------------------//

ResampleLinMono::ResampleLinMono()
{
	filter_delay = FILTER_DELAY_LIN_MONO;
	current = 0;
}

ResampleLinMono::ResampleLinMono(bool do_pad, float max_rate)
{
	filter_delay = FILTER_DELAY_LIN_MONO;
	if(do_pad) {
		pad_samples = (int)((float)filter_delay * max_rate + 0.5);
	}
	current = 0;
}

inline float ResampleLinMono::updownsample(float *src, unsigned int current, unsigned int curfrc)
{
	float samp;

	samp = src[current] + (src[current+1] - src[current]) * (curfrc / 4294967296.0);

	return samp;
}

int ResampleLinMono::resample(signed short *dst, float rate, unsigned num_samples)
{
	float samp;

	unsigned long long current_fixed;
	long long increment_fixed;
	int done = 0;

	increment_fixed = (1.0/rate) * 4294967296.0;
	current_fixed = (((unsigned long long)current<<32)&0xFFFFFFFF00000000LL) | (curfrc & 0xffffffff);

	if(pad_samples) {
		if(num_samples < pad_samples) {
			pad_samples = 0;
			return 0;
		}
		for(unsigned i=0; i<pad_samples; i++) {
			*dst = 0; dst++;
		}
		done += pad_samples;
		num_samples -= pad_samples;
		pad_samples = 0;
	}

	if(rate < 1.0)  {	// downsampling, need to scale sinc apropriatly
		while(num_samples > 0) {
			unsigned int current = (current_fixed>>32) & 0xffffffff;
			unsigned int curfrc = current_fixed & 0xffffffff;

			if(current >= samples.size() - filter_delay) break;

			samp = ResampleLinMono::updownsample(&samples[0], current, curfrc);

			// should never happen, but maybe because of rounding etc. it does:
			if(samp > 32767.0) samp = 32767.0;
			else if (samp < -32768.0) samp = -32768.0;

			*dst = (short)samp; dst++;

			current_fixed += increment_fixed;

			num_samples--; done++;
		}
	} else if (rate > 1.0 || this->curfrc) {
		while(num_samples > 0) {
			unsigned int current = (current_fixed>>32) & 0xffffffff;
			unsigned int curfrc = current_fixed & 0xffffffff;

			if(current >= samples.size() - filter_delay) break;

			samp = ResampleLinMono::updownsample(&samples[0], current, curfrc);

			// should never happen, but maybe because of rounding etc. it does:
			if(samp > 32767.0) samp = 32767.0;
			else if (samp < -32768.0) samp = -32768.0;

			*dst = (short)samp; dst++;

			current_fixed += increment_fixed;

			num_samples--; done++;
		}
	} else {
		/* rate == 1.0 && curfrc == 0 */
		/* just copy */
		unsigned int current = this->current;
		while(num_samples > 0) {
			if(current >= samples.size() - filter_delay) break;

			*dst = (short)samples[current]; dst++;

			current++;

			num_samples--; done++;
		}
		current_fixed = (((unsigned long long)current<<32)&0xFFFFFFFF00000000LL);
	}

	this->current = (current_fixed>>32) & 0xffffffff;
	this->curfrc = current_fixed & 0xffffffff;

	return done;
}

//--- ResampleLinStereo -----------------------------------------------------//

ResampleLinStereo::ResampleLinStereo()
{
	filter_delay = FILTER_DELAY_LIN_STEREO;
	current = 0;
}

ResampleLinStereo::ResampleLinStereo(bool do_pad, float max_rate)
{
	filter_delay = FILTER_DELAY_LIN_STEREO;
	if(do_pad) {
		pad_samples = (int)((float)(filter_delay/2) * max_rate + 0.5);
	}
	current = 0;
}

inline float ResampleLinStereo::updownsample(float *src, unsigned int current, unsigned int curfrc)
{
	float samp;

	samp = src[current*2] + (src[current*2+2] - src[current*2]) * (curfrc / 4294967296.0);

	return samp;
}

int ResampleLinStereo::resample(signed short *dst, float rate, unsigned num_samples)
{
	float samp_l;
	float samp_r;

	unsigned long long current_fixed;
	long long increment_fixed;
	int done = 0;

	increment_fixed = (1.0/rate) * 4294967296.0;
	current_fixed = (((unsigned long long)current<<32)&0xFFFFFFFF00000000LL) | (curfrc & 0xffffffff);

	if(pad_samples) {
		if(num_samples < pad_samples) {
			pad_samples = 0;
			return 0;
		}
		for(unsigned i=0; i<pad_samples; i++) {
			*dst = 0; dst++;
			*dst = 0; dst++;
		}
		done += pad_samples;
		num_samples -= pad_samples;
		pad_samples = 0;
	}

	if(rate < 1.0)  {	// downsampling, need to scale sinc apropriatly
		while(num_samples > 0) {
			unsigned int current = (current_fixed>>32) & 0xffffffff;
			unsigned int curfrc = current_fixed & 0xffffffff;

			if((current*2) >= samples.size() - filter_delay - 1) break;

			samp_l = ResampleLinStereo::updownsample(&samples[0], current, curfrc);
			samp_r = ResampleLinStereo::updownsample(&samples[1], current, curfrc);

			// should never happen, but maybe because of rounding etc. it does:
			if(samp_l > 32767.0) samp_l = 32767.0;
			else if (samp_l < -32768.0) samp_l = -32768.0;
			if(samp_r > 32767.0) samp_r = 32767.0;
			else if (samp_r < -32768.0) samp_r = -32768.0;

			*dst = (short)samp_l; dst++;
			*dst = (short)samp_r; dst++;

			current_fixed += increment_fixed;

			num_samples--; done++;
		}
	} else if (rate > 1.0 || this->curfrc) {
		while(num_samples > 0) {
			unsigned int current = (current_fixed>>32) & 0xffffffff;
			unsigned int curfrc = current_fixed & 0xffffffff;

			if((current*2) >= samples.size() - filter_delay - 1) break;

			samp_l = ResampleLinStereo::updownsample(&samples[0], current, curfrc);
			samp_r = ResampleLinStereo::updownsample(&samples[1], current, curfrc);

			// should never happen, but maybe because of rounding etc. it does:
			if(samp_l > 32767.0) samp_l = 32767.0;
			else if (samp_l < -32768.0) samp_l = -32768.0;
			if(samp_r > 32767.0) samp_r = 32767.0;
			else if (samp_r < -32768.0) samp_r = -32768.0;

			*dst = (short)samp_l; dst++;
			*dst = (short)samp_r; dst++;

			current_fixed += increment_fixed;

			num_samples--; done++;
		}
	} else {
		/* rate == 1.0 && curfrc == 0 */
		/* just copy */
		unsigned int current = this->current;
		while(num_samples > 0) {
			if((current*2) >= samples.size() - filter_delay - 1) break;

			*dst = (short)samples[current*2]; dst++;
			*dst = (short)samples[current*2+1]; dst++;

			current++;

			num_samples--; done++;
		}
		current_fixed = (((unsigned long long)current<<32)&0xFFFFFFFF00000000LL);
	}

	this->current = (current_fixed>>32) & 0xffffffff;
	this->curfrc = current_fixed & 0xffffffff;

	return done;
}

Resample* ResampleFactory::createResampleObj(bool doPad, float maxRatio, interpolType interpol_type, sampleType sample_type)
{
	if(interpol_type == INTERPOL_LINEAR) {
		if(sample_type == SAMPLE_MONO) {
			return new ResampleLinMono(doPad, maxRatio);
		} else {
			return new ResampleLinStereo(doPad, maxRatio);
		}
	} else {
		if(sample_type == SAMPLE_MONO) {
			return new ResampleSincMono(doPad, maxRatio);
		} else {
			return new ResampleSincStereo(doPad, maxRatio);
		}
	}
}

