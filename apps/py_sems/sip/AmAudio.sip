
struct AmEvent
{
%TypeHeaderCode
#include "../../../core/AmEvent.h"
%End

    int event_id;
    bool processed;

    AmEvent(int /*event_id*/);
    virtual ~AmEvent();

%ConvertToSubClassCode
	if(dynamic_cast<AmAudioEvent*>(sipCpp)){
		sipClass = sipClass_AmAudioEvent;
	}
	else {
		sipClass = sipClass_AmEvent;
	}
%End

};

class AmAudioEvent: AmEvent
{

%TypeHeaderCode
#include "../../../core/AmAudio.h"
%End

public:
    enum EventType {
	
	noAudio, // Audio class has nothing to play and/or record anymore

	// Audio input & output have been cleared: 
	// !!! sent only from AmSession !!!
	cleared  
    };

    AmAudioEvent(int /*id*/);
};



class AmAudioFile
{

%TypeHeaderCode
#include "../../../core/AmAudioFile.h"
%End

public:
    AmAudioFile();
    ~AmAudioFile();

    enum OpenMode { 
	Read=1, 
	Write=2 
    };

    bool getLoop();
%MethodCode
	sipRes = sipCpp->loop.get();
%End

    void setLoop(bool);
%MethodCode
	sipCpp->loop.set(a0);
%End


    int open(const string& /*filename*/, 
	     OpenMode /*mode*/ = AmAudioFile::Read,
	     bool /*is_tmp*/ = false);

    void rewind();
    void close();
    int getDataSize();


private:
    AmAudioFile(const AmAudioFile&);

};









