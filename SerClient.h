/*
 * $Id: SerClient.h,v 1.1.2.1 2005/04/13 10:57:09 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _SerClient_h_
#define _SerClient_h_

#include "AmServer.h"

#include <string>
#include <map>

using std::map;
using std::string;

// timeout in us (ms/1000)
#define SER_WRITE_TIMEOUT  250000 // 250 ms
// write retry interval in us
#define SER_WRITE_INTERVAL 50000  //  50 ms

// timeout in us (ms/1000)
#define SER_SIPREQ_TIMEOUT 5*60*1000*1000 // 5 minutes
#define SER_DBREQ_TIMEOUT  250000 // 250 ms
// read retry interval in us
#define SER_READ_INTERVAL  50000  // 50 ms

// C definition to ease stucture initialization
extern "C" {
    struct ser_client_cb_t {
	int (*open_reply)(const string& filename);
	int (*write)(int reply_fd, const char * buf, unsigned int len);
	int (*read)(int fd, char* buffer, int buf_size, int timeout);
    };
}

class SerClient;

struct SerClientData
{
    int     id;
    string  addr;
    char    msg_buf[MSG_BUFFER_SIZE];

    SerClientData(int id,const string& addr);
    virtual ~SerClientData() {};
};

class SerClient
{
    static SerClient* _client;

    ser_client_cb_t*        cbs;
    map<int,SerClientData*> datas;

    SerClientData* id2scd(int id);
    
protected:
    SerClient(){}
    ~SerClient() {}

//     virtual SerClientData* getClientData(const string& addr)=0;
//     virtual int  send(SerClientData* d, const string& msg, int timeout)=0;
//     virtual void close(SerClientData* d)=0;

public:
    static SerClient* getInstance();
    
    int  open();
    int  send(int id,const string& cmd,
	      const string& msg,int timeout);
    char* getResponseBuffer(int id);
    void close(int id);
};


int write_to_fifo(const string& fifo, const char * buf, unsigned int len);

#endif
// Local Variables:
// mode:C++
// End:
