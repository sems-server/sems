/*
 * Copyright (C) 2008 iptego GmbH
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

#ifndef _AmPrecodedFile_H
#define _AmPrecodedFile_H

#include "AmPlugIn.h"
#include "AmSdp.h"
#include "amci/amci.h"
#include "AmAudioFile.h"
#include "AmAudio.h"

#include <string>
#include <map>

#define PRECODED_CODEC_ID   100 // could go into amci/codecs.h

struct precoded_payload_t : public amci_payload_t {
 public:
  string c_name;
  string format_parameters;
  unsigned int frame_ms;
  unsigned int frame_bytes;
  string filename;

  precoded_payload_t() {codec_id = PRECODED_CODEC_ID;}
};

class AmPrecodedFileFormat : public AmAudioFileFormat {

  precoded_payload_t& precoded_payload;
  amci_subtype_t subtype;

 public:
  AmPrecodedFileFormat(precoded_payload_t& precoded_payload);
  ~AmPrecodedFileFormat();
  amci_subtype_t*  getSubtype() { return &subtype; }

 protected:
  virtual int getCodecId();
};

class AmPrecodedRtpFormat : public AmAudioRtpFormat {
  precoded_payload_t& precoded_payload;
  
 public:
  AmPrecodedRtpFormat(precoded_payload_t& precoded_payload,
		      const vector<SdpPayload *>& payloads);
  ~AmPrecodedRtpFormat();

 protected:
  int getCodecId();
};

class AmPrecodedFileInstance
: public AmAudioFile {
 
  precoded_payload_t& precoded_payload;
  const vector<SdpPayload*>&  payloads;
  amci_inoutfmt_t m_iofmt;

 public:
 AmPrecodedFileInstance(precoded_payload_t& precoded_payload, 
			const vector<SdpPayload*>&  payloads);
 ~AmPrecodedFileInstance();

 AmPrecodedRtpFormat* getRtpFormat();

  int open();
  
 protected:
  AmAudioFileFormat* fileName2Fmt(const string& name, const string& subtype);
};

class AmPrecodedFile 
: public AmPayloadProviderInterface {

  std::map<int,precoded_payload_t>  payloads;

 public: 
  AmPrecodedFile();
  ~AmPrecodedFile();

  /** need to call after open() */
  void initPlugin();

  int open(const std::string& filename);

  amci_payload_t*  payload(int payload_id);
  int getDynPayload(const string& name, int rate, int encoding_param);

  void getPayloads(vector<SdpPayload>& pl_vec);

  AmPrecodedFileInstance* getFileInstance(int payload_id, const vector<SdpPayload*>&  m_payloads);
};

#endif
