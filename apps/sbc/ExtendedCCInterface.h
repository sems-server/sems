#ifndef __EXTENDED_CC_INTERFACE
#define __EXTENDED_CC_INTERFACE

class SBCCallLeg;
class SBCCallProfile;

struct InitialInviteHandlerParams
{
  string remote_party;
  string remote_uri;
  string from;
  const AmSipRequest *original_invite;
  AmSipRequest *modified_invite;

  InitialInviteHandlerParams(const string &to, const string &ruri, const string &_from,
      const AmSipRequest *original, AmSipRequest *modified):
      remote_party(to), remote_uri(ruri), from(_from),
      original_invite(original), modified_invite(modified) { }
};

class ExtendedCCInterface
{
  protected:
    ~ExtendedCCInterface() { }

  public:
    virtual void onStateChange(SBCCallLeg *call, SBCCallProfile *call_profile) { };

    /** called from A-leg onInvite with modified request (i.e. with call profile
     * driven replacements already done, FIXME: do we need original as well?)
     *
     * can be used for forking or handling INVITE with Replaces header */
    virtual void onInitialInvite(SBCCallLeg *call, SBCCallProfile *call_profile, InitialInviteHandlerParams &params) { }
};

#endif
