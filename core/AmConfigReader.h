#ifndef AmConfigReader_h
#define AmConfigReader_h

#include <string>
#include <map>
using std::string;
using std::map;


#define MAX_CONFIG_LINE 512
/**
 * \brief configuration file reader
 * 
 * Reads configuration file into internal map, 
 * which subsequently can be queried for the value of
 * specific configuration values.
 */

class AmConfigReader
{
    map<string,string> keys;

public:
    int  loadFile(const string& path);
    bool hasParameter(const string& param);
    const string& getParameter(const string& param, const string& defval = "");
    unsigned int getParameterInt(const string& param, unsigned int defval = 0);

    map<string,string>::const_iterator begin() const
    { return keys.begin(); }

    map<string,string>::const_iterator end() const
    { return keys.end(); }
};

#endif
