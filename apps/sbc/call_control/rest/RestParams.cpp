#include "RestParams.h"
#include "log.h"
#include <curl/curl.h>
#include <map>

using namespace std;

static void trim_spaces(string &s)
{
  size_t pos = s.find_last_not_of(" \t\r\n");
  if (pos != string::npos) {
    s.erase(pos + 1);
    pos = s.find_first_not_of(' ');
    if (pos != string::npos) s.erase(0, pos);
  }
  else s.erase(s.begin(), s.end());
}

void RestParams::getIfSet(const char *param_name, string &dst)
{
  map<string, string>::iterator i = params.find(param_name);
  if (i != params.end()) dst = i->second;
}

void RestParams::handleParamLine(const string &line, size_t begin, size_t end)
{
  size_t pos;

  pos = line.find('=', begin);
  if (pos == string::npos) return; // "=" not found, ignore this param
  if (pos >= end) return; // "=" found on another line, ignore this param

  string name(line.substr(begin, pos - begin));
  string value(line.substr(pos + 1, end - pos));

  trim_spaces(name);
  trim_spaces(value);

  if (name.size() > 0) {
    DBG("REST: param %s='%s'\n", name.c_str(), value.c_str());
    params[name] = value;
  }
}
    
void RestParams::processData(const string &data)
{
  // TODO: read as XML instead of this internal format
  size_t first = 0;
  size_t last;

  while (true) { 
    last = data.find('\n', first);
    if (last == string::npos) {
      handleParamLine(data, first, data.size());
      break;
    }
    else handleParamLine(data, first, last);
    first = last + 1;
  }
}

RestParams::RestParams(const string &url, bool _ignore_errors): 
  ignore_errors(_ignore_errors)
{
  string data;

  retrieve(url, data);
  processData(data);
}

static size_t store_data_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  string &mem = *((string *)userp);

  try {
    if (realsize > 0) mem.append((char *)contents, realsize);
  }
  catch (...) {
    ERROR("error while reading data from an URL\n");
    return 0;
  }
  return realsize;
}

void RestParams::retrieve(const string &url, string &data)
{
  // based on http://curl.haxx.se/libcurl/c/getinmemory.html
  CURL *curl_handle = curl_easy_init();

  data.clear();

  if (!curl_handle) {
    // this can't be ignored
    throw string("curl_easy_init() failed\n");
  }

  curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, store_data_cb);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&data);
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "REST-in-peace/0.1");
  CURLcode res = curl_easy_perform(curl_handle);
  
  curl_easy_cleanup(curl_handle);

  long code = 0;
  if (res == 0) curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &code);
  if ((res != 0) || (code < 200) || (code > 299)) {
    DBG("libcurl returned %d, response code: %ld\n", res, code);

    if (!ignore_errors) {
      string s("error returned when reading data from ");
      s += url;
      s += "\n";
      throw s;
    }

    data.clear(); // no data available
  }
}
