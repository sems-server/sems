/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/** @file AmStats.h */
#ifndef _AmStats_h_
#define _AmStats_h_

#include <sys/types.h>
#include <math.h>
#include <string.h>
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

  virtual ~MeanValue() { }

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

 public:
  MeanArray(size_t size)
    : MeanValue(),
    buf_size(size)
    {
      buffer = new double[size];
      memset(buffer, 0, sizeof(buffer[0])*size);
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
