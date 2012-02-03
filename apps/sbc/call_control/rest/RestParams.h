#ifndef __REST_PARAMS_H
#define __REST_PARAMS_H

#include <string>
#include "SBCCallProfile.h"

class RestParams {
  public:
    enum Format { JSON, XML, TEXT };

  protected:
    AmArg params;

    // parsing data
    void handleParamLine(const string &line, size_t begin, size_t end);
    bool readFromText(const string &data); // read content in text format
    bool readFromXML(const string &data); // read content in XML format
    bool readFromJson(const string &data); // read content in json format

    /* retrieve data from given URL into dst
     * can throw an exception if error occurs (for example libcurl can not be
     * initialized; non-ok reply/connect errors are not considered exceptional) */
    bool get(const std::string &url, std::string &dst);

  public:
    /* retrieve data from given URL and decode them using given format
     * can throw an exception if something strange happens (see get) */
    bool retrieve(const std::string &url, Format fmt = TEXT);
    
    // sets dst to value of given parameter if the parameter is set
    void getIfSet(const char *param_name, string &dst);
    void getIfSet(const char *param_name, bool &dst);
};

#endif
