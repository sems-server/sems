#ifndef __EXTENDED_CC_INTERFACE
#define __EXTENDED_CC_INTERFACE

class SBCCallLeg;
class SBCCallProfile;

class ExtendedCCInterface {
  protected:
    ~ExtendedCCInterface() { }

  public:
    virtual void onStateChange(SBCCallLeg *call, SBCCallProfile *call_profile) = 0;
};

#endif
