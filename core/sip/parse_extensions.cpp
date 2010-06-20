#include <string.h>
#include <assert.h>

#include "log.h"
#include  <AmSipHeaders.h>
#include "parse_common.h"
#include "parse_extensions.h"

/**
 * some header's body =  [option-tag *(COMMA option-tag)
 *       token        =  1*(alphanum / "-" / "." / "!" / "%" / "*"
 *                             / "_" / "+" / "`" / "'" / "~" )
 */

int parse_extension(const char *begin, int len)
{
  // simple, for now: only one extension to parse here
  if (len == SIP_HDR_LEN(SIP_EXT_100REL) && 
      memcmp(begin, SIP_EXT_100REL, len) == 0)
    return SIP_EXTENSION_100REL;
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
