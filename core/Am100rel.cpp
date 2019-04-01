#include "Am100rel.h"
#include "AmConfig.h"

#include "AmUtils.h"
#include "AmSipHeaders.h"
#include "AmSession.h"

#include "log.h"

Am100rel::Am100rel(AmSipDialog* dlg, AmSipDialogEventHandler* hdl)
  : reliable_1xx(AmConfig::rel100), rseq(0), rseq_confirmed(false),
    rseq_1st(0), dlg(dlg), hdl(hdl)
{
  // if (reliable_1xx)
  //   rseq = 0;
}

int  Am100rel::onRequestIn(const AmSipRequest& req)
{
  if (reliable_1xx == REL100_IGNORED)
    return 1;

  /* activate the 100rel, if needed */
  if (req.method == SIP_METH_INVITE) {
    switch(reliable_1xx) {
      case REL100_SUPPORTED: /* if support is on, enforce if asked by UAC */
        if (key_in_list(getHeader(req.hdrs, SIP_HDR_SUPPORTED, SIP_HDR_SUPPORTED_COMPACT),
              SIP_EXT_100REL) ||
            key_in_list(getHeader(req.hdrs, SIP_HDR_REQUIRE), 
              SIP_EXT_100REL)) {
          reliable_1xx = REL100_REQUIRE;
          DBG(SIP_EXT_100REL " now active.\n");
        }
        break;

      case REL100_REQUIRE: /* if support is required, reject if UAC doesn't */
        if (! (key_in_list(getHeader(req.hdrs,SIP_HDR_SUPPORTED, SIP_HDR_SUPPORTED_COMPACT),
              SIP_EXT_100REL) ||
            key_in_list(getHeader(req.hdrs, SIP_HDR_REQUIRE), 
              SIP_EXT_100REL))) {
          ERROR("'" SIP_EXT_100REL "' extension required, but not advertised"
            " by peer.\n");
	  AmBasicSipDialog::reply_error(req, 421, SIP_REPLY_EXTENSION_REQUIRED,
					SIP_HDR_COLSP(SIP_HDR_REQUIRE) 
					SIP_EXT_100REL CRLF);
          if (hdl) hdl->onFailure();
          return 0; // has been replied
        }
        break; // 100rel required

      case REL100_DISABLED:
        // TODO: shouldn't this be part of a more general check in SEMS?
        if (key_in_list(getHeader(req.hdrs,SIP_HDR_REQUIRE),SIP_EXT_100REL)) {
          AmBasicSipDialog::reply_error(req, 420, SIP_REPLY_BAD_EXTENSION,
					SIP_HDR_COLSP(SIP_HDR_UNSUPPORTED) 
					SIP_EXT_100REL CRLF);
          if (hdl) hdl->onFailure();
          return 0; // has been replied
        }
        break;

      default:
        ERROR("BUG: unexpected value `%d' for '" SIP_EXT_100REL "' switch.", 
          reliable_1xx);
#ifndef NDEBUG
        abort();
#endif
    } // switch reliable_1xx
  } else if (req.method == SIP_METH_PRACK) {
    if (reliable_1xx != REL100_REQUIRE) {
      WARN("unexpected PRACK received while " SIP_EXT_100REL " not active.\n");
      // let if float up
    } else if (rseq_1st<=req.rseq && req.rseq<=rseq) {
      if (req.rseq == rseq) {
        rseq_confirmed = true; // confirmed
      }
      // else: confirmation for one of the pending 1xx
      DBG("%sRSeq (%u) confirmed.\n", (req.rseq==rseq) ? "latest " : "", rseq);
    }
  }

  return 1;
}

int  Am100rel::onReplyIn(const AmSipReply& reply)
{
  if (reliable_1xx == REL100_IGNORED)
    return 1;

  if (dlg->getStatus() != AmSipDialog::Trying && 
      dlg->getStatus() != AmSipDialog::Proceeding && 
      dlg->getStatus() != AmSipDialog::Early && 
      dlg->getStatus() != AmSipDialog::Connected)
    return 1;

  if (100<reply.code && reply.code<200 && reply.cseq_method==SIP_METH_INVITE) {
    switch (reliable_1xx) {
    case REL100_SUPPORTED:
      if (key_in_list(getHeader(reply.hdrs, SIP_HDR_REQUIRE), 
          SIP_EXT_100REL))
        reliable_1xx = REL100_REQUIRE;
        // no break!
      else
        break;

    case REL100_REQUIRE:
      if (!key_in_list(getHeader(reply.hdrs,SIP_HDR_REQUIRE),SIP_EXT_100REL) ||
          !reply.rseq) {
        ERROR(SIP_EXT_100REL " not supported or no positive RSeq value in "
            "(reliable) 1xx.\n");
	dlg->bye();
        if (hdl) hdl->onFailure();
      } else {
        DBG(SIP_EXT_100REL " now active.\n");
        if (hdl) ((AmSipDialogEventHandler*)hdl)->onInvite1xxRel(reply);
      }
      break;

    case REL100_DISABLED:
      // 100rel support disabled
      break;
    default:
      ERROR("BUG: unexpected value `%d' for " SIP_EXT_100REL " switch.", 
          reliable_1xx);
#ifndef NDEBUG
      abort();
#endif
    } // switch reliable 1xx
  } else if (reliable_1xx && reply.cseq_method==SIP_METH_PRACK) {
    if (300 <= reply.code) {
      // if PRACK fails, tear down session
      dlg->bye();
      if (hdl) hdl->onFailure();
    } else if (200 <= reply.code) {
      if (hdl) 
	((AmSipDialogEventHandler*)hdl)->onPrack2xx(reply);
    } else {
      WARN("received '%d' for " SIP_METH_PRACK " method.\n", reply.code);
    }
    // absorbe the replys for the prack (they've been dispatched through 
    // onPrack2xx, if necessary)
    return 0;
  }
  return 1;
}

void Am100rel::onRequestOut(AmSipRequest& req)
{
  if (reliable_1xx == REL100_IGNORED || req.method!=SIP_METH_INVITE)
    return;

  switch(reliable_1xx) {
    case REL100_SUPPORTED:
      if (! key_in_list(getHeader(req.hdrs, SIP_HDR_REQUIRE), SIP_EXT_100REL))
        req.hdrs += SIP_HDR_COLSP(SIP_HDR_SUPPORTED) SIP_EXT_100REL CRLF;
      break;
    case REL100_REQUIRE:
      if (! key_in_list(getHeader(req.hdrs, SIP_HDR_REQUIRE), SIP_EXT_100REL))
        req.hdrs += SIP_HDR_COLSP(SIP_HDR_REQUIRE) SIP_EXT_100REL CRLF;
      break;
    default:
      ERROR("BUG: unexpected reliability switch value of '%d'.\n",
          reliable_1xx);
    case 0:
      break;
  }
}

void Am100rel::onReplyOut(AmSipReply& reply)
{
  if (reliable_1xx == REL100_IGNORED)
    return;

  if (reply.cseq_method == SIP_METH_INVITE) {
    if (100 < reply.code && reply.code < 200) {
      switch (reliable_1xx) {
        case REL100_SUPPORTED:
          if (! key_in_list(getHeader(reply.hdrs, SIP_HDR_REQUIRE), 
			    SIP_EXT_100REL))
            reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SUPPORTED) SIP_EXT_100REL CRLF;
          break;
        case REL100_REQUIRE:
          // add Require HF
          if (! key_in_list(getHeader(reply.hdrs, SIP_HDR_REQUIRE), 
			    SIP_EXT_100REL))
            reply.hdrs += SIP_HDR_COLSP(SIP_HDR_REQUIRE) SIP_EXT_100REL CRLF;
          // add RSeq HF
          if (getHeader(reply.hdrs, SIP_HDR_RSEQ).length())
            // already added (by app?)
            break;
          if (! rseq) { // only init rseq if 1xx is used
            rseq = (get_random() & 0x3ff) + 1; // start small (<1024) and non-0
            rseq_confirmed = false;
            rseq_1st = rseq;
          } else {
            if ((! rseq_confirmed) && (rseq_1st == rseq))
              // refuse subsequent 1xx if first isn't yet PRACKed
              throw AmSession::Exception(491, "first reliable 1xx not yet "
                  "PRACKed");
            rseq ++;
          }
          reply.hdrs += SIP_HDR_COLSP(SIP_HDR_RSEQ) + int2str(rseq) + CRLF;
          break;
        default:
          break;
      }
    } else if (reply.code < 300 && reliable_1xx == REL100_REQUIRE) { //code = 2xx
      if (rseq && !rseq_confirmed) 
        // reliable 1xx is pending, 2xx'ing not allowed yet
        throw AmSession::Exception(491, "last reliable 1xx not yet PRACKed");
    }
  }
}

void Am100rel::onTimeout(const AmSipRequest& req, const AmSipReply& rpl)
{
  if (reliable_1xx == REL100_IGNORED)
    return;

  INFO("reply <%s> timed out (not PRACKed).\n", rpl.print().c_str());
  if (100 < rpl.code && rpl.code < 200 && reliable_1xx == REL100_REQUIRE &&
      rseq == rpl.rseq && rpl.cseq_method == SIP_METH_INVITE) {
    INFO("reliable %d reply timed out; rejecting request.\n", rpl.code);
    if(hdl) hdl->onNoPrack(req, rpl);
  } else {
    WARN("reply timed-out, but not reliable.\n"); // debugging
  }
}


