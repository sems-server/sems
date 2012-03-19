/*
 * Copyright (C) 2011 Raphael Coeffic
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

import mx.controls.Alert;
import flash.events.Event;
import flash.net.NetConnection;
import flash.net.NetStream;
import flash.media.Microphone;
import flash.media.MicrophoneEnhancedMode;

// application states
private static const NOT_CONNECTED:uint = 0;
private static const INCOMING_CALL:uint = 1;
private static const DIALING:uint       = 2;
private static const CONNECTING:uint    = 3;
private static const CONNECTED:uint     = 4;

// call states (from RtmpSession.cpp)
private static const CALL_NOT_CONNECTED:uint = 0;
private static const CALL_IN_PROGRESS:uint   = 1;
private static const CALL_CONNECTED:uint     = 2;
private static const CALL_DISCONNECTING:uint = 3;

// media streams request (from RtmpSession.cpp)
private static const CALL_CONNECT_STREAMS:uint = 4; 

[Bindable]
private var g_state:uint = NOT_CONNECTED;

[Bindable]
private var g_dial_state:uint = NOT_CONNECTED;

private var g_netConnection:NetConnection;
private var g_micStream:NetStream;
private var g_inStream:NetStream;


// bConnect button has been clicked
private function onConnectClick(evt:Event): void 
{
    try {
	if(g_state != NOT_CONNECTED) {
	    disconnectStreams();
	    g_netConnection.close();
	    g_state = NOT_CONNECTED;
	}
	else {
	    g_netConnection = new NetConnection();
	    g_netConnection.addEventListener(NetStatusEvent.NET_STATUS, netStatusHandler);

	    g_netConnection.connect(connectUrl.text);
	    g_state = CONNECTING;
	    lStatus.text = "status: connecting...";
	}
    }
    catch(err:Error){
	Alert.show("Error: " + err.message,"Connect error");
	g_state = NOT_CONNECTED;
	g_dial_state = NOT_CONNECTED;
    }
}

private function onDialFault(error:Object):void
{
    g_dial_state = NOT_CONNECTED;
    lStatus.text = "status: dial failed (" + error.code + ")";
}

// bDial button has been clicked
private function onDialClick(evt:Event): void 
{
    if(g_state != CONNECTED)
	return;

    if(g_dial_state == NOT_CONNECTED) {
	g_netConnection.call('dial',
			     new Responder(null,onDialFault),
			     dialUri.text);
	g_dial_state = DIALING;
	lStatus.text = "status: dialing...";
    }
    else {
	disconnectStreams();
	g_netConnection.call('hangup',null);
	lStatus.text = "status: hanging up...";
	g_dial_state = NOT_CONNECTED;
    }
}

private function onAcceptClick(evt:Event): void
{
    if(g_dial_state != INCOMING_CALL)
	return;

    g_dial_state = CONNECTING;
    g_netConnection.call('accept',null);
    lStatus.text = "status: accepted incoming call...";
}

private function onRegisterResult(res:Object): void
{
    lStatus.text = "onRegisterResult: " + String(res.uri);
}

private function onRegisterFault(error:Object): void
{
    lStatus.text = "onRegisterFault: " + error.code;
}

private function netStatusHandler(event:NetStatusEvent): void 
{
    switch(event.info.code){

	// status events:
    case "NetConnection.Connect.Success":
	lStatus.text = event.info.level + ": connected to server";
	g_state = CONNECTED;

	g_netConnection.call('register',
			     new Responder(onRegisterResult,onRegisterFault));
	break;

    case "NetConnection.Connect.Closed":
	lStatus.text = event.info.level + ": disconnected from server";
	g_state = NOT_CONNECTED;
	g_dial_state = NOT_CONNECTED;
	break;

	// error events:
    case "NetConnection.Connect.Failed":
	lStatus.text = event.info.level + ": connection to server failed";
	g_state = NOT_CONNECTED;
	g_dial_state = NOT_CONNECTED;
	break;
    case "NetConnection.Connect.Rejected":
	lStatus.text = event.info.level + ": connection to server rejected";
	g_state = NOT_CONNECTED;
	g_dial_state = NOT_CONNECTED;
	break;

    case "NetStream.Play.Start":
	//lStatus.text = event.info.level + ": " + event.info.description;
	break;

    case "NetStream.Play.Stop":
	//lStatus.text = event.info.level + ": " + event.info.description;
	disconnectStreams();
	break;

    case "Sono.Call.Status":
	//lStatus.text = event.info.level + ": " + event.info.status_code;
	switch(event.info.status_code){
	case CALL_NOT_CONNECTED:
	case CALL_DISCONNECTING:
	    g_dial_state = NOT_CONNECTED;
	    disconnectStreams();
	    lStatus.text = "status: call disconnected";
	    break;
	case CALL_IN_PROGRESS:
	    g_dial_state = DIALING;
	    lStatus.text = "status: dialing...";
	    break;
	case CALL_CONNECTED:
	    g_dial_state = CONNECTED;
	    lStatus.text = "status: call connected";
	    break;
	case CALL_CONNECT_STREAMS:
	    connectStreams();
	    break;
	}
	break;

    case "Sono.Call.Incoming":
	g_dial_state = INCOMING_CALL;
	lStatus.text = "status: incoming call...";
	break;

	// unkown event:
    default:
	lStatus.text = event.info.level + ": " 
	    + event.info.description + " [" 
	    + event.info.code + "]" ;
	break;
    }
}

private function connectStreams():void
{
    if(!Microphone.isSupported){
	//TODO: report error
	return; // no microphone!!!
    }

    g_micStream = new NetStream(g_netConnection);
    g_micStream.addEventListener(NetStatusEvent.NET_STATUS, netStatusHandler);

    var micro:Microphone = null;
    if(Microphone['getEnhancedMicrophone'] != undefined) {
	micro = Microphone['getEnhancedMicrophone'](-1);
	lStatus.text = "Enhanced mike";
    }
    else {
	micro = Microphone.getMicrophone(-1);
	lStatus.text = "Normal mike";
    }

    if(micro == null){
	//TODO: report error
	return;
    }

    micro.codec = SoundCodec.SPEEX;
    micro.setSilenceLevel(0);
    micro.encodeQuality = 8; // 0 - 10
    micro.gain = 50; // 0 - 100
    micro.framesPerPacket = 1; // default=2
    micro.rate = 16; // wideband
    
    g_micStream.attachAudio(micro);
    g_micStream.publish("dummy","live");

    g_inStream = new NetStream(g_netConnection);
    g_inStream.addEventListener(NetStatusEvent.NET_STATUS, netStatusHandler);
    
    g_inStream.play("dummy","live");
    g_dial_state = CONNECTED;
}

private function disconnectStreams():void
{
    if(g_micStream) { 
	g_micStream.close();
	g_micStream = null;
    }

    if(g_inStream) {
	g_inStream.close();
	g_inStream = null;
    }
}