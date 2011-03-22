// quick hack to separate one call per callid from a logfile

#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
using namespace std;

std::vector<string> explode(const string& s, const string& delim, 
			    const bool keep_empty = false);

#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char *argv[])
{
  
  if (argc<4) {
    cerr << "usage: " << argv[0] << " <infile> <callid> <threadid_position>" << endl;
 
    cerr << " where <infile> is the log file to process" << endl;
    cerr << "       <callid> the Call-ID to extract" << endl;
    cerr << "       <threadid_position> the column of the thread ID in the "
      "log, starting with 0 (i.e. number of spaces before the thread ID)" << endl;
    exit(1);
  }

  long int log_offset = -1;
  char* endptr;
  log_offset = strtoll(argv[3], &endptr, 10); // offset of thread
  if ((errno == ERANGE && (log_offset == LLONG_MAX || log_offset == LLONG_MIN))
      || (errno != 0 && log_offset == 0) || endptr == argv[3] || log_offset<0) {
    cerr << "invalid thread column ID " << argv[3] << endl;
    exit(1);
  }

  if (log_offset<0) {
  }

  ifstream f;

  string fname = argv[1];
  string callid = argv[2];
  f.open(argv[1]);
  if (!f.good()) {
    cerr << "error opening file " << fname << endl; 
    exit(1);
  }



  string app_thread;
  string ltag;

  // threadid  log     
  map<string, string> udprecv_log;
  map<string, string> appthread_log;

  while (!f.eof()) {
    string s;
    getline(f,s);

    vector<string> v = explode(s, " ");
    
    if (v.size() < (unsigned)log_offset+1)
      continue;

    string t = v[log_offset + 0]; // thread
    string delim;
    if (v.size() > (unsigned)log_offset + 4) 
      delim = v[log_offset + 4]; // vv or ^^
    string ptype;
    if (v.size() > (unsigned)log_offset + 5) 
      ptype = v[log_offset + 5]; // M or S 

    // cout << "thread " << t << " delim " << delim << " ptype " << ptype << endl;

    if (delim == "vv") {
      // block starts
      if (ptype == "M") { // message received

	udprecv_log[t] = s+"\n"; // new thread block
      } else if (ptype == "S") { // app processing
#define GET_CALL_IDENT							\
	string call_ident;						\
	if (v.size() > (unsigned)log_offset + 6)					\
	  call_ident = v[log_offset + 6];				\
	call_ident = call_ident.substr(1, call_ident.length()-2);	\
	vector<string> ci_parts = explode(call_ident, "|", true);	\
	string ci_cid = ci_parts[0];					\
	string ci_ltag = ci_parts[1];

	GET_CALL_IDENT;

	if (!ci_cid.empty() && ci_cid != callid)
	  continue; // other call

	appthread_log[t] = s+"\n"; 	  
      } else {
	cerr << "unknown ptype " << ptype << " in '" << s << "'" << endl;
	continue;
      }
    } else if (delim == "^^") {
      // block ends
      if (ptype == "M") { // message received
	GET_CALL_IDENT;

	map<string, string>::iterator it=udprecv_log.find(t);
	if ((ci_cid == callid) && (it != udprecv_log.end())) {
	  cout << endl << endl << it->second << s+"\n";
	}

	if (it != udprecv_log.end()) {
	  udprecv_log.erase(it);
	}
      } else if (ptype == "S") { // app processing
	GET_CALL_IDENT;

	map<string, string>::iterator it=appthread_log.find(t);
	if ((ci_cid == callid) && (it != appthread_log.end())) {
	  cout << endl << endl << it->second << s+"\n";
	}

	if (it != appthread_log.end()) {
	  appthread_log.erase(it);
	}
 
      } else {
	cerr << "unknown ptype " << ptype << " in '" << s << "'" << endl;
	continue;
      }

    } else {
      map<string, string>::iterator it=udprecv_log.find(t);
      if (it != udprecv_log.end()) {
	it->second+=s+"\n";
	continue;
      }
      it=appthread_log.find(t);
      if (it != appthread_log.end()) {
	it->second+=s+"\n";
	continue;
      }
    }   
  }
}


// Explode string by a separator to a vector
// see http://stackoverflow.com/questions/236129/c-how-to-split-a-string
std::vector<string> explode(const string& s, const string& delim, 
			    const bool keep_empty) {
  vector<string> result;
  if (delim.empty()) {
    result.push_back(s);
    return result;
  }
  string::const_iterator substart = s.begin(), subend;
  while (true) {
    subend = search(substart, s.end(), delim.begin(), delim.end());
    string temp(substart, subend);
    if (keep_empty || !temp.empty()) {
      result.push_back(temp);
    }
    if (subend == s.end()) {
      break;
    }
    substart = subend + delim.size();
  }
  return result;
}


