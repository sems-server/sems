
class AmSessionAudioConnector
{
%TypeHeaderCode
#include "../../../core/AmB2ABSession.h"
%End
public:
	AmSessionAudioConnector();

	~AmSessionAudioConnector();

	void block();

	void release();

	void waitReleased();
private:
	AmSessionAudioConnector(const AmSessionAudioConnector&);
};

class PySemsB2ABDialog
{
%TypeHeaderCode
#include "../PySemsB2ABDialog.h"
#include "../../../core/AmB2ABSession.h"
%End

public:

    AmSipDialog* dlg;

    PySemsB2ABDialog();

    virtual ~PySemsB2ABDialog();
%MethodCode
    fprintf(stderr,"------------- dealloc_PySemsB2ABDialog ----------\n");
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
    void setInOut(AmAudioFile* /In/ /*in*/, AmAudioFile* /In/ /*out*/);

    virtual void process(AmEvent* /*In,Out*/);
    
    void connectCallee(const string& /*remote_party*/,
	 	       const string& /*remote_uri*/,
		       const string& /*local_party*/,
		       const string& /*local_uri*/);

    void connectSession();
    void disconnectSession();

    int getCalleeStatus();

protected:
  virtual AmB2ABCalleeSession* createCalleeSession() /Factory/;

  virtual void onB2ABEvent(PySemsB2ABEvent* /TransferBack/);

  virtual void relayEvent(PySemsB2ABEvent* ev /Transfer/);	
private:
  PySemsB2ABDialog(const PySemsB2ABDialog&);
};

class AmB2ABCalleeSession 
{
%TypeHeaderCode
#include "../../../core/AmB2ABSession.h"
%End
public:
  AmB2ABCalleeSession(const string& /*other_local_tag*/, AmSessionAudioConnector* );

  void connectSession();
  void disconnectSession();

  void setInput(AmAudioFile* /In/ /*in*/);
  void setOutput(AmAudioFile* /In/ /*out*/);
  void setInOut(AmAudioFile* /In/ /*in*/, AmAudioFile* /In/ /*out*/);

private:
  AmB2ABCalleeSession(const AmB2ABCalleeSession& /* other_s*/);

};

class PySemsB2ABCalleeDialog : AmB2ABCalleeSession
{
%TypeHeaderCode
#include "../PySemsB2ABDialog.h"
#include "../../../core/AmB2ABSession.h"
%End
public:
  PySemsB2ABCalleeDialog(const string& /*other_local_tag*/, AmSessionAudioConnector* /* connector */);

protected:
  virtual void onPyB2ABEvent(PySemsB2ABEvent* /TransferBack/);

private:
  PySemsB2ABCalleeDialog(const PySemsB2ABCalleeDialog& /* other_s*/);
};

class PySemsB2ABEvent
{
%TypeHeaderCode
#include "../PySemsB2ABDialog.h"
%End
public:
  PySemsB2ABEvent(int /* ev_id */);
};

