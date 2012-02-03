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

static bool str2bool(const string &s)
{
  if (s.empty()) return true; // understand as just bool option which should be true
  if ((s == "yes") || (s == "1") || (s == "true")) return true;
  else return false;
}

////////////////////////////////////////////////////////////////////////////////

void RestParams::getIfSet(const char *param_name, string &dst)
{
  map<string, string>::iterator i = params.find(param_name);
  if (i != params.end()) dst = i->second;
}

void RestParams::getIfSet(const char *param_name, bool &dst)
{
  map<string, string>::iterator i = params.find(param_name);
  if (i != params.end()) dst = str2bool(i->second);
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
    
bool RestParams::readFromText(const string &data)
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
  return true;
}

bool RestParams::readFromJson(const string &data)
{
  return false; // not implemented yet
}

bool RestParams::readFromXML(const string &data)
{
  // TODO
  ERROR("REST: trying to decode XML data - not implemented yet!\n");
  return false;
}

bool RestParams::retrieve(const string &url, Format fmt)
{
  string data;
    
  DBG("REST: reading from url %s\n", url.c_str());

  if (!get(url, data)) return false;

  switch (fmt) {
    case TEXT: return readFromText(data);
    case JSON: return readFromJson(data);
    case XML: return readFromXML(data);
  }

  return false;
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

bool RestParams::get(const string &url, string &data)
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

  if (res != 0) {
    // really ignore these errors?
    DBG("libcurl returned error %d\n", res);
    return false;
  }

  long code = 0;
  curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &code);
  if ((code < 200) || (code > 299)) {
    DBG("non-ok response code when downloading data: %ld\n", code);
    return false;
  }

  return true;
}
