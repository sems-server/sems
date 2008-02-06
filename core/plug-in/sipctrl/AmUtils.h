/*
 * $Id: AmUtils.h 457 2007-09-24 17:37:04Z sayer $
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

#ifndef _AmUtils_h_
#define _AmUtils_h_

#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include <string>
using std::string;

#include <vector>

#define FIFO_PERM S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH

#define PARAM_HDR "P-App-Param"
#define APPNAME_HDR "P-App-Name"

/** @file AmUtils.h */

/** 
 * Convert an int to a string. 
 */
string int2str(int val);

/** 
 * Convert an unsigned int to a string using hexdecimal representation. 
 */
string int2hex(unsigned int val);

/** 
 * Convert an unsigned long to a string using hexdecimal representation. 
 */
string long2hex(unsigned long val);

/**
 * Convert a reversed hex string to uint.
 * @param str    [in]  string to convert.
 * @param result [out] result integer.
 * @return true if failed. 
 */
bool reverse_hex2int(const string& str, unsigned int& result);

/** 
 * Convert a string to an uint. 
 * @param str    [in]  string to convert.
 * @param result [out] result integer.
 * @return true if failed. 
 */
bool str2i(const string& str, unsigned int& result);

/** 
 * Internal version of preceeding 'str2i' method. 
 * @param str    [in,out] gets incremented until sep char or error occurs
 * @param result [out] result of the function
 * @param sep    [in] character seprating the number to convert and the next token
 */
bool str2i(char*& str, unsigned int& result, char sep = ' ');

/**
 * Parse code/reason line.
 * Syntax: "code reason"
 *
 * @param lbuf line     buffer to parse.
 * @param res_code_str  char[4]; Syntax: xxxx whereby 
 *                      each x is a between 0 and 9.
 * @param res_code      where to store the resulting integer 'code'.
 * @param res_msg       where to store the 'reason'.
 */
int parse_return_code(const char* lbuf, 
		      unsigned int& res_code, string& res_msg );

/**
 * Get a line from a file.
 * 
 * @param fifo_stream  file to read from.
 * @param str          [out] line buffer.
 * @param len          size of the line buffer.
 * 
 * @return -1 if buffer overruns, 
 *         else the line's length.
 */
int fifo_get_line(FILE* fifo_stream, char* str, size_t len);

/**
 * Gets one or more lines from a file.
 * It stops to read as soon as a '.' line
 * gets read or encounters EOF.
 * 
 * Syntax:
 * "[
 *   {one or more lines}
 * ]
 * ."
 * 
 * @param fifo_stream  file to read from.
 * @param str          [out] line buffer.
 * @param len          size of the line buffer.
 * 
 * @return -1 if buffer overruns, 
 *         else total length.
 */
int fifo_get_lines(FILE* fifo_stream, char* str, size_t len);

/**
 * Size of line buffer used within fifo_get_param.
 */
//#define MAX_LINE_SIZE  256

/**
 * Gets a line parameter from file pointer.
 * If line only contain '.', the parameter is empty.
 * Throws error string if failed.
 */
int fifo_get_param(FILE* fp, string& p, char* line_buf, unsigned int size);

/**
 * Gets a line from the message buffer.
 * @param msg_c [in,out] pointer to current message buffer.
 * @param str line buffer.
 * @param len line buffer size.
 * @return size read or -1 if something went wrong.
 */
int msg_get_line(char*& msg_c, char* str, size_t len);

/**
 * Gets [1..n] line(s) from the message buffer.
 * @param msg_c [in,out] pointer to current message buffer.
 * @param str line buffer.
 * @param len line buffer size.
 * @return size read or -1 if something went wrong.
 */
int msg_get_lines(char*& msg_c, char* str, size_t len);

/**
 * Size of line buffer used within msg_get_param.
 */
//#define MSG_LINE_SIZE 1024

/**
 * Gets a line parameter from message buffer.
 * If line only contain '.', the parameter is empty.
 * Throws error string if failed.
 * @param msg_c [in,out] pointer to current message buffer.
 * @param p [out] parameter to be filled.
 */
int msg_get_param(char*& msg_c, string& p, char* line_buf, unsigned int size);

/**
 * Tells if a file exists.
 * @param name file name.
 * @return true if file could be openned.
 */
bool file_exists(const string& name);

/**
 * @return A file name extracted from the given full path file name.
 */
string filename_from_fullpath(const string& path);

/**
 * @return A file extension extracted from the given full path file name.
 */
string file_extension(const string& path);

/**
 * @return new path resulting from the concatanation of path, 
 * suffix and eventually a slash between them
 */
string add2path(const string& path, int n_suffix, ...);

/**
 * Reads a line from file and stores it in a string.
 * @param f    file to read from.
 * @param p    [out] where to store the line.
 * @param lb   line buffer.
 * @param lbs  line buffer's size.
 */
#define READ_FIFO_PARAMETER(f,p,lb,lbs)\
    {\
        if( fifo_get_line(f,lb,lbs) !=-1 ){\
            if(!strcmp(".",lb))\
                lb[0]='\0';\
            DBG("%s= <%s>\n",#p,lb);\
            p  = lb;\
        }\
        else {\
	    throw string("could not read from FIFO: ") + string(strerror(errno));\
	} \
    } 

struct in_addr;
string get_addr_str(struct in_addr in);

string uri_from_name_addr(const string& name_addr);
string get_ip_from_name(const string& name);

#ifdef SUPPORT_IPV6
int inet_aton_v6(const char* name, struct sockaddr_storage* ss);
void set_port_v6(struct sockaddr_storage* ss, short port);
short get_port_v6(struct sockaddr_storage* ss);
#endif

/**
 * Creates and binds a unix socket to given path.
 * @param path path the new socket should be bound to.
 */
int create_unix_socket(const string& path);

/** \brief microseconds sleep using select */
#define sleep_us(nusecs) \
	{ \
	struct timeval tval; \
	tval.tv_sec=nusecs/1000000; \
	tval.tv_usec=nusecs%1000000; \
	select(0, NULL, NULL, NULL, &tval ); \
	}


int write_to_fifo(const string& fifo, const char * buf, unsigned int len);
int write_to_socket(int sd, const char* to_addr, const char * buf, unsigned int len);

string extract_tag(const string& addr);

/** returns true if key is in s_list, a list of items delimited by list_delim
 *skips whitespaces, too */
bool key_in_list(const string& s_list, const string& key, char list_delim = ',');

/** return string with trailing spaces and everything after ; including ; itself removed */
string strip_header_params(const string& hdr_string);

/** get a header parameter value */
string get_header_param(const string& hdr_string, const string& param_name);

/** get the value of key @param name from the list param_hdr*/
string get_header_keyvalue(const string& param_hdr, const string& name);

/** get the value of key @param name from P-Iptel-Param header in hdrs */
string get_session_param(const string& hdrs, const string& name);

/** support for thread-safe pseudo-random numbers  - init function */
void init_random();
/** support for thread-safe pseudo-random numbers  - make a random number function */
unsigned int get_random();

/** Explode string by a separator to a vector */
std::vector <string> explode(string s, string e);

/** add a directory to an environement variable */
void add_env_path(const char* name, const string& path);

#endif

// Local Variables:
// mode:C++
// End:
