/*
 * $Id: AmServer.h,v 1.10.2.1 2005/04/13 10:57:09 rco Exp $
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

#ifndef _AmServer_h_
#define _AmServer_h_

#include <stdio.h>
#include <sys/stat.h>

#include "AmCmd.h"
#include "AmThread.h"

#include <map>
using std::map;

#define FIFO_VERSION "0.3"

#define MAX_LINE_SIZE  256
#define MAX_BODY_SIZE 1024

#define MSG_BUFFER_SIZE 4096
#define MSG_BODY_SIZE   2048
#define MAX_MSG_ERR     5

/**
 * Derive your class from AmServerFct if you want
 * to implement a server function (FIFO).
 */
class AmFifoServerFct
{
public:
    virtual int execute(FILE* stream, const string& cmd)=0;
    virtual ~AmFifoServerFct(){}
};

/**
 * Derive your class from AmServerFct if you want
 * to implement a server function (Unix sockets).
 */
class AmUnServerFct
{
public:
    virtual int execute(char* msg_c, const string& cmd)=0;
    virtual ~AmUnServerFct(){}
};

/**
 * The FIFO Server.
 * 
 * Enables Ser to send request throught FIFO file.
 * 
 * Syntax for FIFO commands:<br>
 *   <pre>
 *   version  EOL // 'x.x'
 *   command  EOL // fct_name.{plugin_name,'bye'}
 *   method   EOL
 *   user     EOL 
 *   dstip    EOL // important for the RTP connection IP
 *   from     EOL
 *   to       EOL
 *   call-id  EOL
 *   from-tag EOL
 *   [to-tag] EOL
 *   cseq     EOL
 *   transid  EOL // Ser's tranction ID
 *   [body]   EOL
 *   .EOL
 *   EOL
 *   </pre>
 */

class AmFifoServer: public AmFifoServerFct
{
 private:
    /** 
     * Singleton pointer.
     * @see instance()
     */
    static AmFifoServer* _instance;

    /**
     */
    FILE*  fifo_stream;
    map<string,AmFifoServerFct*> fct_map;

    /**
     * Gets a line from the FIFO.
     * @param str line buffer.
     * @param len line buffer size.
     * @return size read or -1 if something went wrong.
     */
    int get_line(char* str, size_t len);
    /**
     * Gets [1..n] line(s) from the FIFO.
     * @param str line buffer.
     * @param len line buffer size.
     * @return size read or -1 if something went wrong.
     */
    int get_lines(char* str, size_t len);
    /**
     * Go to the next token (' ' separated).
     * @param str line buffer.
     * @return token start pointer (leading spaces are trimmed).
     */
    char* get_next_tok(char*& str);
    /**
     * Consume the next chars until it finds an empty line (from the FIFO).
     */
    void consume_request();
    /** Avoid external instantiation. @see instance(). */
    AmFifoServer();
    /** Avoid external instantiation. @see instance(). */
    ~AmFifoServer();

 public:
    /** Get a fifo server instance. */
    static AmFifoServer* instance();

    /** Initialize the FIFO server. 
     * Must be called before AmFifoServer::run()
     * @return 0 if everything OK.
     */
    int init(const char * fifo_name);

    /** Runs the fifo server. */
    void run();

    /** 
     * Register a server functions.
     *
     * Warning:
     *   You should call this function only
     *   from your module's onLoad method.
     *   Otherwise, stability can't be garantied...
     *
     * @return 0 if everything went OK.
     */
    int  registerFct(const string& name,AmFifoServerFct* fct); 

    /** @see AmFifoServerFct::execute */
    int  execute(FILE* stream, const string& cmd_str);
};

class AmUnServer: public AmThread, public AmUnServerFct
{
 private:
    /** 
     * Singleton pointer.
     * @see instance()
     */
    static AmUnServer* _instance;

    /**
     */
    string fifo_path;
    int    fifo_socket;

    map<string,AmUnServerFct*> fct_map;

    /** AmThread::on_stop() */
    void on_stop();

    /** Avoid external instantiation. @see instance(). */
    AmUnServer();
    /** Avoid external instantiation. @see instance(). */
    ~AmUnServer();

 public:
    /** Get a fifo server instance. */
    static AmUnServer* instance();

    /** Initialize the unix socket server. 
     * Must be called before AmFifoServer::run()
     * @return 0 if everything OK.
     */
    int init(const char * sock_name);

    /** Runs the unix socket server. */
    void run();

    /** 
     * Register a server functions.
     *
     * Warning:
     *   You should call this function only
     *   from your module's onLoad method.
     *   Otherwise, stability can't be garantied...
     *
     * @return 0 if everything went OK.
     */
    int  registerFct(const string& name,AmUnServerFct* fct); 

    /** @see AmUnServerFct::execute */
    int  execute(char* msg_c, const string& cmd_str);
};

#endif

// Local Variables:
// mode:C++
// End:



