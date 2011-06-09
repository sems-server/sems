import mx.controls.Alert;
import flash.events.Event;
import flash.net.NetConnection;
import flash.net.NetStream;
import flash.media.Microphone;

// application states
private static const NOT_CONNECTED:uint = 1;
private static const CONNECTING:uint    = 2;
private static const CONNECTED:uint     = 3;
private static const STREAMING:uint     = 4;

private var g_state:uint = NOT_CONNECTED;

private var g_netConnection:NetConnection;
private var g_micStream:NetStream;
private var g_inStream:NetStream;


private function onCreationComplete(event:Event): void
{
    g_netConnection = new NetConnection();
    g_netConnection.addEventListener(NetStatusEvent.NET_STATUS, netStatusHandler);
}


// bConnect button has been clicked
private function onConnectClick(evt:Event): void 
{
    try {
	if(g_state != NOT_CONNECTED) {
	    if(g_micStream)
		g_micStream.close();
	    if(g_inStream)
		g_inStream.close();
	    g_netConnection.close();
	    g_state = NOT_CONNECTED;
	}
	else {
	    g_netConnection.connect(connectUrl.text);
	    g_state = CONNECTING;
	    lStatus.text = "status: connecting...";
	}
    }
    catch(err:Error){
	Alert.show("Error: " + err.message,"Connect error");
	g_state = NOT_CONNECTED;
    }

    if(g_state == NOT_CONNECTED) {
	bConnect.label = "Connect";
    }
    else {
	bConnect.label = "Disconnect";
    }
}

private function netStatusHandler(event:NetStatusEvent): void 
{
    switch(event.info.code){

	// status events:
    case "NetConnection.Connect.Success":
	lStatus.text = event.info.level + ": connected";
	g_state = CONNECTED;
	connectStreams();
	break;

    case "NetConnection.Connect.Closed":
	lStatus.text = event.info.level + ": disconnected";
	g_state = NOT_CONNECTED;
	break;

	// error events:
    case "NetConnection.Connect.Failed":
	lStatus.text = event.info.level + ": connection failed";
	g_state = NOT_CONNECTED;
	break;
    case "NetConnection.Connect.Rejected":
	lStatus.text = event.info.level + ": connection rejected";
	g_state = NOT_CONNECTED;
	break;

	// unkown event:
    default:
	lStatus.text = event.info.level + ": " 
	    + event.info.description + " [" 
	    + event.info.code + "]" ;
	break;
    }

    if(g_state != NOT_CONNECTED){
	bConnect.label = "Disconnect";
    }
    else {
	bConnect.label = "Connect";
    }
}

private function connectStreams():void
{
    if(!Microphone.isSupported)
	return; // no microphone!!!

    g_micStream = new NetStream(g_netConnection);
    g_micStream.addEventListener(NetStatusEvent.NET_STATUS, netStatusHandler);

    var micro:Microphone = Microphone.getMicrophone();
    
    micro.codec = SoundCodec.SPEEX;
    micro.setUseEchoSuppression(true);
    micro.setSilenceLevel(0);
    micro.encodeQuality = 8; // 0 - 10
    micro.gain = 50; // 0 - 100
    micro.framesPerPacket = 1; // default=2
    micro.rate = 8; // narrowband

    g_micStream.attachAudio(micro);
    g_micStream.publish("sip:music@iptel.org","live");

    g_inStream = new NetStream(g_netConnection);
    g_inStream.bufferTimeMax = 0.2;
    g_inStream.addEventListener(NetStatusEvent.NET_STATUS, netStatusHandler);
    g_inStream.play("sip:music@iptel.org","live");
}
