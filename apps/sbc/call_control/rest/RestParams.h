#ifndef __REST_PARAMS_H
#define __REST_PARAMS_H

#include <string>
#include "SBCCallProfile.h"

class RestParams {
  protected:
    map<string, string> params;
    bool ignore_errors;

    // parsing data
    void handleParamLine(const string &line, size_t begin, size_t end);
    void processData(const string &data); // handle retrieved data

    void retrieve(const std::string &url, std::string &dst); // retrieve data from given URL into dst

  public:
    /* retrieve data stored at given URL and process them internally
     * throws an exception if data can not be loaded
     * retrieve errors are ignored if _ignore_errors is set */
    RestParams(const std::string &url, bool _ignore_errors = true);
    
    // sets dst to value of given parameter if the parameter is set
    void getIfSet(const char *param_name, string &dst);
    void getIfSet(const char *param_name, bool &dst);
};

#endif
