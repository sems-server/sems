#ifndef _AmStats_h_
#define _AmStats_h_

#include <sys/types.h>
#include <math.h>

/** 
 * \brief math mean implementation 
 *
 * The mean of all previously stored values is calculated (no reset).
 */
class MeanValue
{
 protected:
  double cum_val;
  size_t n_val;

 public:
  MeanValue()
    : cum_val(0.0),
    n_val(0)
    {}

  void push(double val){
    cum_val += val;
    n_val++;
  }

  double mean(){
    if(!n_val) return 0.0;
    return cum_val / float(n_val);
  }
};

/** 
 * \brief math stddev implementation 
 *
 * The standard deviation of previously stored 
 * values is calculated.
 */
class StddevValue
{
 protected:
  double cum_val;
  double sq_cum_val;
  size_t n_val;

 public:
  StddevValue()
    : cum_val(0.0),
    sq_cum_val(0.0),
    n_val(0)
    {}

  void push(double val){
	
    cum_val += val;
    sq_cum_val += val*val;
    n_val++;
  }

  double stddev(){
    if(!n_val) return 0.0;
    return sqrt((n_val*sq_cum_val - cum_val*cum_val)/(n_val*(n_val-1)));
  }
};

/** 
 * \brief math mean implementation (n values)
 *
 * The mean of n previously stored values is calculated
 */
class MeanArray: public MeanValue
{
  double *buffer;
  size_t  buf_size;

  double  cum_val;
  size_t  n_val;

 public:
  MeanArray(size_t size)
    : buf_size(size),
    MeanValue()
    {
      buffer = new double[size];
    }

  ~MeanArray(){
    delete [] buffer;
  }

  void push(double val){

    cum_val -= buffer[n_val % buf_size];
    buffer[n_val % buf_size] = val;

    cum_val += val;
    n_val++;
  }

  double mean(){
    if(!n_val) return 0.0;
    return cum_val / double(n_val > buf_size ? buf_size : n_val);
  }
};

#endif
