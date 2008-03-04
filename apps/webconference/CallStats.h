#ifndef _WCC_CALL_STATS_H
#define _WCC_CALL_STATS_H

#include <string>
using std::string;

class WCCCallStats {

  string filename;

  unsigned int total;
  unsigned int failed;
  unsigned int seconds;

  int write_cnt;

  void save();
  void load();

 public:
 
 WCCCallStats(const string& stats_dir);
 ~WCCCallStats();

 /** return statistics summary */
 string getSummary();

 /** add a call - success and connect seconds */
 void addCall(bool success, unsigned int connect_t);
};

#endif
