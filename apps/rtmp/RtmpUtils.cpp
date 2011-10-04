#include "RtmpUtils.h"

// standard flash methods and params
SAVC(app);
SAVC(connect);
SAVC(flashVer);
SAVC(swfUrl);
SAVC(pageUrl);
SAVC(tcUrl);
SAVC(fpad);
SAVC(capabilities);
SAVC(audioCodecs);
SAVC(videoCodecs);
SAVC(videoFunction);
SAVC(objectEncoding);
SAVC(_result);
SAVC(_error);
SAVC(createStream);
SAVC(closeStream);
SAVC(deleteStream);
SAVC(getStreamLength);
SAVC(play);
SAVC(fmsVer);
SAVC(mode);
SAVC(level);
SAVC(code);
SAVC(description);
SAVC(secureToken);
SAVC(publish);
SAVC(onStatus);
SAVC(status);
SAVC(error);
const AVal av_NetStream_Play_Start = _AVC("NetStream.Play.Start");
const AVal av_Started_playing = _AVC("Started playing");
const AVal av_NetStream_Play_Stop = _AVC("NetStream.Play.Stop");
const AVal av_Stopped_playing = _AVC("Stopped playing");
SAVC(details);
SAVC(clientid);
SAVC(pause);

// custom methods and params
SAVC(dial);
SAVC(hangup);
SAVC(register);
SAVC(accept);
const AVal av_Sono_Call_Incoming = _AVC("Sono.Call.Incoming");
SAVC(uri);
const AVal av_Sono_Call_Status = _AVC("Sono.Call.Status");
SAVC(status_code);
