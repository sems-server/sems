#include <string.h>
#include <assert.h>
#include <set>
#include <string>

#include "log.h"
#include  <AmSipHeaders.h>
#include <AmThread.h>
#include "parse_common.h"
#include "parse_extensions.h"

using std::string;
using std::set;

/**
 * some header's body =  [option-tag *(COMMA option-tag)
 *       token        =  1*(alphanum / "-" / "." / "!" / "%" / "*"
 *                             / "_" / "+" / "`" / "'" / "~" )
 */

int parse_extension(const char *begin, int len)
{
  if (len == SIP_HDR_LEN(SIP_EXT_100REL) &&
      memcmp(begin, SIP_EXT_100REL, len) == 0)
    return SIP_EXTENSION_100REL;
  if (len == SIP_HDR_LEN(SIP_EXT_TIMER) &&
      memcmp(begin, SIP_EXT_TIMER, len) == 0)
    return SIP_EXTENSION_TIMER;
  if (len == SIP_HDR_LEN(SIP_EXT_REPLACES) &&
      memcmp(begin, SIP_EXT_REPLACES, len) == 0)
    return SIP_EXTENSION_REPLACES;
  return 0;
}

bool parse_extensions(unsigned *extensions, const char *start, int len)
{
  const char *begin;
  const char *pos;
  int ext;
  unsigned mask = 0;

  enum {
    EAT_WS,
    OVER_TAG,
  } state = EAT_WS;

  begin = 0; //g++ happy

  for (pos = start; pos < start + len; pos ++) {
    switch (*pos) {
      case ' ':
      case '\t':
      case ',':
      case CR:
      case LF:
        if (state == OVER_TAG) {
          assert(begin);
          if ((ext = parse_extension(begin, pos - begin)))
            mask |= ext;
          // reset stuff
          state = EAT_WS;
          begin = 0;
        }
        break;

      default:
        if (! IS_TOKEN(*pos)) {
          INFO("invalid extensions header content <%.*s>: illegal char `%c'",
              len, start, *pos);
          return false;
        }
        if (state == EAT_WS) {
          state = OVER_TAG;
          begin = pos;
        }
    }
  }

  if (begin) {
    if ((ext = parse_extension(begin, pos - begin)))
      mask |= ext;
  }

  *extensions = mask;
  DBG("mask of parsed extensions: 0x%x.\n", mask);
  return true;
}

// --- Supported extensions registry (RFC 3261 Section 8.2.2.3) ---

static AmMutex supported_ext_mutex;
static set<string> supported_extensions;

void register_supported_extension(const string& option_tag)
{
    AmLock l(supported_ext_mutex);
    supported_extensions.insert(option_tag);
    DBG("Registered supported SIP extension: %s\n", option_tag.c_str());
}


bool is_extension_supported(const string& option_tag)
{
    AmLock l(supported_ext_mutex);
    bool found = supported_extensions.find(option_tag) != supported_extensions.end();
    return found;
}
string get_unsupported_extensions(const char* require_value, unsigned int len)
{
    string unsupported;
    const char* pos = require_value;
    const char* end = require_value + len;
    const char* tag_start = NULL;

    enum { EAT_WS, OVER_TAG } state = EAT_WS;

    for(; pos <= end; pos++) {
	// Treat end-of-string as a delimiter
	char ch = (pos < end) ? *pos : ',';

	switch(ch) {
	case ' ': case '\t': case ',': case CR: case LF:
	    if(state == OVER_TAG && tag_start) {
		// Found end of a tag
		const char* tag_end = pos;
		// Trim trailing whitespace
		while(tag_end > tag_start &&
		      (*(tag_end-1) == ' ' || *(tag_end-1) == '\t'))
		    tag_end--;

		if(tag_end > tag_start) {
		    string tag(tag_start, tag_end - tag_start);
		    if(!is_extension_supported(tag)) {
			if(!unsupported.empty())
			    unsupported += ", ";
			unsupported += tag;
		    }
		}
		state = EAT_WS;
		tag_start = NULL;
	    }
	    break;
	default:
	    if(state == EAT_WS) {
		// Starting a new option-tag: validate first character
		if(!IS_TOKEN(ch)) {
		    // Invalid character in option-tag: reject malformed header
		    return string();
		}
		state = OVER_TAG;
		tag_start = pos;
	    } else { // state == OVER_TAG
		// Continuing an option-tag: validate character
		if(!IS_TOKEN(ch)) {
		    // Invalid character in option-tag: reject malformed header
		    return string();
		}
	    }
	    break;
	}
    }

    return unsupported;
}
