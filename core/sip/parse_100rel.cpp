
#include <limits.h>

#include "log.h"
#include "AmSipHeaders.h"
#include "sip_parser.h"
#include "parse_common.h"
#include "parse_100rel.h"


#define EAT_WS(_c_, _end_) \
  do { \
    while ((_c_) < (_end_) && (*(_c_) == ' ' || *(_c_) == '\t')) \
      (_c_) ++; \
  } while (0)

#define READ_NUMBER(_no, _c_, _end_) \
  do { \
    register bool fin; \
    (_no) = 0; \
    for (fin = false; !fin && (_c_) < (_end_); ) { \
      switch (*(_c_)) { \
        case '0' ... '9': \
          if (UINT_MAX - (_no) < (unsigned)*(_c_)) { \
            INFO("not an uint32_t.\n"); \
            goto error; \
          } \
          (_no) = (_no) * 10 + *(_c_) - '0'; \
          break; \
        default: \
          fin = true; \
          continue; \
      } \
      (_c_) ++; \
    } \
  } while (0)

#define EAT_TOKEN(_c_, _end_) \
  do { \
    while ((_c_) < (_end_) && IS_TOKEN(*(_c_))) \
      (_c_) ++; \
  } while (0)


bool parse_rseq(unsigned *_rseq, const char *start, int len)
{
  unsigned rseq;
  const char *pos = start;
  const char *end = start + len;
  const char *sav;

  EAT_WS(pos, end);

  sav = pos;
  READ_NUMBER(rseq, pos, end);
  if (sav == pos)
    goto error;

  *_rseq = rseq;
  DBG("parsed sequence content: %u.\n", rseq);
  return true;

error:
  INFO("invalid content in sequence header content <%.*s>.\n", len, start);
  return false;
}


bool parse_rack(sip_rack *rack, const char *start, int len)
{
  const char *pos;
  const char *sav;
  const char *end = start + len;
  unsigned rseq, cseq;
  cstring method_str;
  cstring cseq_str;
  int method;

  pos = start;

  EAT_WS(pos, end);

  sav = pos;
  READ_NUMBER(rseq, pos, end);
  if (pos == sav)
    goto error;
  
  EAT_WS(pos, end);

  sav = pos;
  READ_NUMBER(cseq, pos, end);
  if (pos == sav)
    goto error;
  cseq_str.s = sav;
  cseq_str.len = pos - sav;

  EAT_WS(pos, end);

  sav = pos;
  EAT_TOKEN(pos, end);
  method_str.s = sav;
  method_str.len = pos - sav;

  if (parse_method(&method, method_str.s, method_str.len))
    goto error;

  DBG("parsed '" SIP_HDR_RSEQ "' header: %u %u <%.*s> <%.*s>.\n", rseq, cseq,
      cseq_str.len, cseq_str.s, method_str.len, method_str.s);

  rack->rseq = rseq;
  rack->cseq = cseq;
  rack->cseq_str.s = cseq_str.s;
  rack->cseq_str.len = cseq_str.len;
  rack->method = method;
  rack->method_str.s = method_str.s;
  rack->method_str.len = method_str.len;
  return true;

error:
  INFO("failed to parse <%.*s> as '" SIP_HDR_RSEQ "' header.\n", len, start);
  return false;
}
