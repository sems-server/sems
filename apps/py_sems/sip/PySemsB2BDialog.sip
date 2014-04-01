

class PySemsB2BDialog
{
%TypeHeaderCode
#include "../PySemsB2BDialog.h"
%End

public:

    AmSipDialog* dlg;

    PySemsB2BDialog();

    virtual ~PySemsB2BDialog();
%MethodCode
    fprintf(stderr,"------------- dealloc_PySemsB2BDialog ----------\n");
%End

    void setStopped();
    virtual void onBeforeDestroy();

    virtual void onInvite(const AmSipRequest& /* req */);
    virtual void onCancel(const AmSipRequest& /* req */);
    virtual void onSessionStart();
    virtual void onEarlySessionStart();

    virtual void onBye(const AmSipRequest& /* req */);
    virtual void onDtmf(int /* event */, int /* duration_msec */);


    void setInput(AmAudioFile* /In/ /*in*/);
    void setOutput(AmAudioFile* /In/ /*out*/);
    void setInOut(AmAudioFile* /In/ /*in*/, AmAudioFile* /*out*/);

    virtual void process(AmEvent* /*In,Out*/);


private:
    PySemsB2BDialog(const PySemsB2BDialog&);
};
