#ifndef _RESAMP_H
#define _RESAMP_H

#include <vector>

using std::vector;

class Resample
{
protected:
	int filter_delay;
	unsigned current;
	unsigned curfrc;
	unsigned pad_samples;
	vector<float> samples;

public:
	Resample();
	virtual ~Resample() {};

	/* put num_samples from memory pointed to by samples into state
	 * returns -1 on error (must be a realloc, memory error)
	 * number of samples put into state (currently always = num_samples) otherwise */
	virtual int put_samples(signed short* samples, unsigned int num_samples);


	/*
	 * resample by rate
	 *
	 * dst - pointer where to put at most num_samples (resampled) samples
	 * rate - rate at which to resample (e.g. 0.5 - half the original samplerate)
	 * num_samples - number of samples to put into dst (for stereo 1 sample means 2 samples values, left and right)
	 * rstate - resampling state
	 * 
	 * returns number of samples actually put into dst
	 *
	 */
	virtual int resample(signed short *dst, float rate, unsigned num_samples) = 0;
};

class ResampleSincMono : public Resample
{
protected:
	static float sinc[16][256];
	static bool sinc_initialized;

private:
	void init_sinc(void);
	static inline float upsample(float *src, unsigned int current, unsigned int curfrc);
	static inline float downsample(float *src, unsigned int current, unsigned int curfrc, unsigned long long sinc_increment, float scale);

public:
	ResampleSincMono();
	ResampleSincMono(bool do_pad, float max_rate);
	virtual ~ResampleSincMono() {};
	virtual int resample(signed short *dst, float rate, unsigned num_samples);
};

class ResampleSincStereo : public ResampleSincMono
{
private:
	static inline float upsample(float *src, unsigned int current, unsigned int curfrc);
	static inline float downsample(float *src, unsigned int current, unsigned int curfrc, unsigned long long sinc_increment, float scale);

public:
	ResampleSincStereo();
	ResampleSincStereo(bool do_pad, float max_rate);
	virtual ~ResampleSincStereo() {};
	virtual int resample(signed short *dst, float rate, unsigned num_samples);
};

class ResampleLinMono : public Resample
{
private:
	static inline float updownsample(float *src, unsigned int current, unsigned int curfrc);

public:
	ResampleLinMono();
	ResampleLinMono(bool do_pad, float max_rate);
	virtual ~ResampleLinMono() {};
	virtual int resample(signed short *dst, float rate, unsigned num_samples);
};

class ResampleLinStereo : public Resample
{
private:
	static inline float updownsample(float *src, unsigned int current, unsigned int curfrc);

public:
	ResampleLinStereo();
	ResampleLinStereo(bool do_pad, float max_rate);
	virtual ~ResampleLinStereo() {};
	virtual int resample(signed short *dst, float rate, unsigned num_samples);
};

class ResampleFactory
{
public:
	enum interpolType {
		INTERPOL_LINEAR,
		INTERPOL_SINC
	};

	enum sampleType {
		SAMPLE_MONO,
		SAMPLE_STEREO
	};

	static Resample* createResampleObj(bool doPad, float maxRatio, interpolType interpol_type, sampleType sample_type);
	static void destroyResampleObj(Resample *Obj) { delete Obj; };
};

#endif
