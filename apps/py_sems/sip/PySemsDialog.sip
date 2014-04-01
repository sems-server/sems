

class PySemsDialog
{
%TypeHeaderCode
#include "../PySemsDialog.h"
%End

public:

    AmSipDialog* dlg;

    PySemsDialog();

    virtual ~PySemsDialog();
%MethodCode
    fprintf(stderr,"------------- dealloc_PySemsDialog ----------\n");
%End

    void setStopped();

    virtual void onInvite(const AmSipRequest& /* req */);
    virtual void onSessionStart(const AmSipRequest& /* req */);
    virtual void onCancel(const AmSipRequest& /* req */);

    virtual void onBye(const AmSipRequest& /* req */);
    virtual void onDtmf(int /* event */, int /* duration_msec */);


    void setInput(AmAudioFile* /In/ /*in*/);
    void setOutput(AmAudioFile* /In/ /*out*/);
    void setInOut(AmAudioFile* /In/ /*in*/, AmAudioFile* /*out*/);

    virtual void process(AmEvent* /*In,Out*/);


private:
    PySemsDialog(const PySemsDialog&);
};
