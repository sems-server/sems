#include "arg_conversion.h"
#include "jsonArg.h"
#include "AmUtils.h"

using namespace std;

#define CSTR_LABEL   's'
#define ARRAY_LABEL  'a'
#define STRUCT_LABEL 'x'

static string arg2string(const AmArg &a)
{
  string s;
  char tmp[32];
  const char *p;

  switch (a.getType()) {
    case AmArg::CStr:
      p = a.asCStr();
      sprintf(tmp, "%c%zd/", CSTR_LABEL, strlen(p));
      s = tmp;
      s += p;
      return s;

    case AmArg::Array:
      sprintf(tmp, "%c%zd/", ARRAY_LABEL, a.size());
      s = tmp;
      for (size_t i = 0; i < a.size(); i ++) s += arg2string(a[i]);
      return s;

    case AmArg::Struct:
      sprintf(tmp, "%c%zd/", STRUCT_LABEL, a.size());
      s = tmp;
      for (AmArg::ValueStruct::const_iterator it = a.asStruct()->begin();
           it != a.asStruct()->end(); ++it) {
        sprintf(tmp, "%zd/", it->first.size());
        s += tmp;
        s += it->first;
        s += arg2string(it->second);
      }
      return s;

    default:
      throw string("arg2string not fully implenmented!");
  }

  return "???";
}

static bool read_len(const char *&src, int &len, int &dst)
{
  int i;
  dst = 0;
  for (i = 0; i < len; i++) {
    if ((src[i] >= '0') && (src[i] <= '9')) {
      dst = 10 * dst + (src[i] - '0');
      continue;
    }
    if (src[i] == '/') break;
    return false; // not our length separator
  }
  if (i == len) return false; // separator is missing
  if (i == 0) return false; // no number there
  len -= i + 1;
  src += i + 1;
  return true;
}

static bool read_string(const char *&src, int &len, string &dst)
{
  int l;
  if (!read_len(src, len, l)) return false;
  if (l <= len) {
    dst.assign(src, l);
    len -= l;
    src += l;
    return true;
  }
  return false;
}

static bool string2arg(const char *&src, int &len, AmArg &dst)
{
  string s;
  int cnt;

  if (len < 1) return false;

  switch (src[0]) {

    case CSTR_LABEL:
      if (!read_string(++src, --len, s)) return false;
      dst = s;
      return true;

    case ARRAY_LABEL:
      dst.assertArray();
      if (!read_len(++src, --len, cnt)) return false;
      for (int i = 0; i < cnt; i++) {
        dst.push(AmArg());
        if (!string2arg(src, len, dst.get(dst.size() - 1))) return false;
      }
      return true;

    case STRUCT_LABEL:
      dst.assertStruct();
      if (!read_len(++src, --len, cnt)) return false;
      for (int i = 0; i < cnt; i++) {
        if (!read_string(src, len, s)) return false;
        dst[s] = AmArg();
        if (!string2arg(src, len, dst[s])) return false;
      }
      return true;

    default:
      DBG("unknown label '%c'\n", src[0]);
      return false;
  }
}


#define ESCAPE_CHAR '?' // use % instead?

string arg2username(const AmArg &a)
{
  string encoded = arg2string(a);

  // encode the string using characters allowed in username
  static const char *allowed =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "-_.!~*'"
    "&=+$,;/";

  string res;
  for (size_t i = 0; i < encoded.size(); i++) {
    if (strchr(allowed, encoded[i])) res += encoded[i];
    else {
      res += ESCAPE_CHAR;
      res += char2hex(encoded[i], true);
    }
  }

  string json_vars = arg2json(a);
  DBG("encoding variables: '%s'\n", json_vars.c_str());
  DBG("encoded variables: '%s'\n", res.c_str());

  return res;
}

bool username2arg(const string &src, AmArg &dst)
{
  string encoded(src);

  size_t pos = encoded.find(ESCAPE_CHAR);
  while (pos != string::npos) {
    if (pos + 2 >= encoded.size()) return false;
    unsigned int c;
    if (reverse_hex2int(string() + encoded[pos + 2] + encoded[pos + 1], c)) {
      DBG("%c%c does not convert from hex\n", encoded[pos + 1], encoded[pos + 2]);
      return false;
    }
    encoded.replace(pos, 3, 1, c);
    pos = encoded.find(ESCAPE_CHAR, pos + 1);
  }

  DBG("encoded variables: '%s'\n", encoded.c_str());

  const char *s = encoded.c_str();
  int len = encoded.size();
  bool res = string2arg(s, len, dst);
  if (res) {
    string json_vars = arg2json(dst);
    DBG("decoded variables: '%s'\n", json_vars.c_str());
  }
  else DBG("decoding failed\n");
  return res;
}
