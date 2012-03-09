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

#include "AmBufferedAudio.h"

AmBufferedAudio::AmBufferedAudio(size_t output_buffer_size,
				 size_t low_buffer_thresh,
				 size_t full_buffer_thresh)
  : output_buffer_size(output_buffer_size), 
    low_buffer_thresh(low_buffer_thresh), full_buffer_thresh(full_buffer_thresh),
    r(0), w(0), eof(false), err_code(0)
{
  allocateBuffer();
}

AmBufferedAudio::~AmBufferedAudio() {
  releaseBuffer();
}

void AmBufferedAudio::allocateBuffer() {
  if (output_buffer_size != 0)
    output_buffer = new unsigned char[output_buffer_size];  
  else 
    output_buffer = NULL;
}

void AmBufferedAudio::releaseBuffer() {
  if (output_buffer)
    delete[] output_buffer;
}

// WARNING: do not call this while device is in use! buffer is not locked!
void AmBufferedAudio::setBufferSize(size_t _output_buffer_size, 
				    size_t _low_buffer_thresh,
				    size_t _full_buffer_thresh) {

  DBG("set output buffer size to %zd low thresh %zd, fill thresh %zd\n", 
      _output_buffer_size, _low_buffer_thresh, _full_buffer_thresh);

  bool reset_buffer = output_buffer_size != _output_buffer_size;
  output_buffer_size = _output_buffer_size;
  low_buffer_thresh = _low_buffer_thresh;
  full_buffer_thresh = _full_buffer_thresh;

  if (reset_buffer) {
    releaseBuffer();
    allocateBuffer();
  }
}

void AmBufferedAudio::clearBufferEOF() {
  eof = false;
  err_code = 0;
}

int AmBufferedAudio::get(unsigned long long system_ts, unsigned char* buffer, 
			 int output_sample_rate, unsigned int nb_samples) 
{
  if (!output_buffer_size) 
    return AmAudio::get(system_ts, buffer, output_sample_rate, nb_samples);

  if (w-r < low_buffer_thresh && !eof) {
    input_get_audio(system_ts);
  }
  
  size_t nget = PCM16_S2B(nb_samples * getSampleRate() / output_sample_rate);
  if (w-r < nget) 
    nget = w-r;

  if (!nget) {
    // empty buffer and input error
    if (eof)
      return err_code;

    // empty buffer but no input error
    return 0;
  }
 
  memcpy((unsigned char*)samples,&output_buffer[r],nget);
  r+=nget;

  int size = resampleOutput(samples,nget,getSampleRate(),output_sample_rate);
  memcpy(buffer, (unsigned char*)samples,size);

  return size;
}

void AmBufferedAudio::input_get_audio(unsigned int user_ts) {
  if (r && (r != w)) {
    // move contents to beginning of buffer
    memmove(output_buffer, &output_buffer[r], w-r);
    w -= r; 
    r = 0;
  }
  while (w < full_buffer_thresh) {
    int size = calcBytesToRead(PCM16_B2S(output_buffer_size - w));

//     DBG("calc %d bytes to read\n", size);
    
    // resync might be delayed until buffer empty     // (but output resync never happens)
    size = read(user_ts + PCM16_B2S(w-r),size);
//     DBG("read returned size = %d\n",size);
    if(size <= 0){
      err_code = size;
      eof = true;
      return;
    }
    
    size = decode(size);
    if(size < 0) {
//       DBG("decode returned %i\n",size);
      err_code = size;
      eof = true;
      return; 
    }

//     DBG("decode returned %i\n",size);
    size = downMix(size);
    
    if(size>0) {
      memcpy(&output_buffer[w],(unsigned char*)samples,size);
      w+=size;
    }
  }
}
