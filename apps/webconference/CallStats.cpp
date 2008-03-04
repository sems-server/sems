
#include "CallStats.h"

#include "log.h"
#include <fstream>
#include "AmUtils.h"

#define WRITE_INTERVAL 2

WCCCallStats::WCCCallStats(const string& stats_dir) 
  : total(0),
    failed(0),
    seconds(0)
{
  if (stats_dir.empty())
    filename = "";
  else
    filename = stats_dir+"/stats";

  load();
}

WCCCallStats::~WCCCallStats() { }

void WCCCallStats::save() {
  if (!filename.length())
    return;

  std::ofstream ofs(filename.c_str(), 
		    std::ios::out | std::ios::trunc);
  if (ofs.good()) {
    ofs << total << std::endl << failed << std::endl << seconds;
    ofs.close();
    DBG("saved statistics: %u total %u failed %u seconds (%u min)\n", 
	total, failed, seconds, seconds/60);
  } else {
    ERROR("opening/writing stats to '%s'\n", 
	  filename.c_str());
  }
}

void WCCCallStats::load() {
  if (!filename.length())
    return;

  std::ifstream ifs(filename.c_str(), std::ios::in);
  if (ifs.good()) {
    ifs >> total >> failed >> seconds;
    ifs.close();
    DBG("read statistics: %u total %u failed %u seconds (%u min)\n", 
	total, failed, seconds, seconds/60);
  } else {
    ERROR("opening/reading stats from '%s'\n", 
	  filename.c_str());
  }
}

void WCCCallStats::addCall(bool success, unsigned int connect_t) {
  total++;
  if (success) 
    seconds+=connect_t;
  else
    failed++;
  
  if (!((write_cnt++) % WRITE_INTERVAL)) {
    save();
  }
}

string WCCCallStats::getSummary() {
  return int2str(total) + " total/" +  
    int2str(total - failed) + " connect/"
    + int2str(seconds/60) +" min";    
}
