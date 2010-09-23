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
/** @file AmSmtpClient.h */
#ifndef _AmSmtpClient_h_
#define _AmSmtpClient_h_

#include "AmMail.h"

#include <string>
#include <vector>
using std::string;
using std::vector;

/** 
 * SMTP Line buffer for commands and responses 
 * (not for data transmission!) 
 */
#define SMTP_LINE_BUFFER  512

/**
 * \brief SMTP client implementation
 * 
 */
class AmSmtpClient
{
  string         server_ip;
  unsigned short server_port;

  /** socket descriptor */
  int          sd;
  /** size of last response receved */
  unsigned int received;
  /** recv & scratch buffer */
  char         lbuf[SMTP_LINE_BUFFER];
  /** code of the last response */
  unsigned int res_code;
  /** code of the last response */
  //char         res_code_str[4]; // null-terminated
  /** textof the last response */
  string       res_msg;

  enum Status { st_None=0, st_Ok, st_Error, st_Unknown };
  /** Client status */
  Status status;

  /** @return true if failed */
  bool read_line();
  /** @return true if failed */
  bool get_response();
  /** @return true if failed */
  bool parse_response();
  /** @return true if failed */
  bool send_line(const string& cmd);
  /** @return true if failed */
  bool send_data(const vector<string>& hdrs, const AmMail& mail);
  /** @return true if failed */
  bool send_command(const string& cmd);
  /** @return true if failed */
  bool send_body(const vector<string>& hdrs, const AmMail& mail); 

public:
  AmSmtpClient();
  ~AmSmtpClient();

  /** @return true if failed */
  bool connect(const string& _server_ip, unsigned short _server_port);
  /** @return true if failed */
  bool send(const AmMail& mail);
  /** @return true if failed */
  bool disconnect();
  /** @return true if failed */
  bool close();
};

#endif

// Local Variables:
// mode:C++
// End:

