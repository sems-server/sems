#include "mISDNNames.h"
//namespace mISDN { //this is hack to cover definition of dprint which is defined in mISDN and sems core
extern "C" {
#include <mISDNuser/mISDNlib.h>
#include <mISDNuser/net_l2.h>
#include <mISDNuser/l3dss1.h>
}
//}

const char* mISDNNames::isdn_prim[4] = {" REQUEST"," CONFIRM"," INDICATION"," RESPONSE" };
const char* mISDNNames::IE_Names[37] ={ "bearer_capability", "cause", "call_id", "call_state", "channel_id", "facility",
"progress", "net_fac", "notify", "display", "date", "keypad", "signal", "info_rate", "end2end_transit",
"transit_delay_sel", "pktl_bin_para","pktl_window", "pkt_size", "closed_userg", "connected_nr", "connected_sub",
"calling_nr", "calling_sub", "called_nr", "called_sub", "redirect_nr", "redirect_dn", "transit_net_sel",
"restart_ind", "llc", "hlc", "useruser", "more_data", "sending_complete", "congestion_level", "comprehension_required"};


const char* mISDNNames::Message(int i) {
	switch(i) {
	case CC_TIMEOUT:	return "TIMEOUT";
	case CC_SETUP: 		return "SETUP";
	case CC_SETUP_ACKNOWLEDGE: return "SETUP_ACK";
	case CC_PROCEEDING: 	return "PROCEEDING";
	case CC_ALERTING:	return "ALERTING";
	case CC_CONNECT: 	return "CONNECT";
	case CC_CONNECT_ACKNOWLEDGE: return "CONNECT_ACK";
	case CC_DISCONNECT: 	return "DISCONNECT";
	case CC_RELEASE: 	return "RELEASE";
	case CC_RELEASE_COMPLETE: return "RELEASE_COMP";
	case CC_INFORMATION: 	return "INFORMATION";
	case CC_PROGRESS: 	return "PROGRESS";
	case CC_NOTIFY: 	return "NOTIFY";
	case CC_SUSPEND: 	return "SUSPEND";
	case CC_SUSPEND_ACKNOWLEDGE: return "SUSPEND_ACK";
	case CC_SUSPEND_REJECT: return "SUSPEND_REJ";
	case CC_RESUME: 	return "RESUME";
	case CC_RESUME_ACKNOWLEDGE: return "RESUME_ACK";
	case CC_RESUME_REJECT: 	return "RESUME_REJ";
	case CC_HOLD: 		return "HOLD";
	case CC_HOLD_ACKNOWLEDGE: return "HOLD_ACK";
	case CC_HOLD_REJECT: 	return "HOLD_REJ";
	case CC_RETRIEVE: 	return "RETRIEVE";
	case CC_RETRIEVE_ACKNOWLEDGE: return "RETRIEVE_ACK";
	case CC_RETRIEVE_REJECT: return "RETRIEVE_REJ";
	case CC_FACILITY: 	return "FACILITY";
	case CC_STATUS: 	return "STATUS";
	case CC_RESTART: 	return "RESTART";
	case CC_RELEASE_CR: 	return "RELEASE_CR";
	case CC_NEW_CR: 	return "NEW_CR";
	case DL_ESTABLISH: 	return "DL_ESTABLISH";
	case DL_RELEASE: 	return "DL_RELEASE";
	case PH_ACTIVATE: 	return "PH_ACTIVATE";
	case PH_DEACTIVATE: 	return "PH_DEACTIVATE";
	case MGR_SHORTSTATUS: 	return "MGR_SHORTSTATUS";
	}
	return "ERROR";
}
const char* mISDNNames::NPI(int i) {
        switch(i) {
        case 0x00: return "Unknown";
        case 0x01: return "ISDN/Tel E.164";
        case 0x03: return "Data X.121 ";
        case 0x04: return "Telex F.69";
        case 0x08: return "National";
        case 0x09: return "Private";
        case 0x0F: return "Reserved";
        }
        return "ERROR";
}
const char* mISDNNames::TON(int i) {
        switch(i) {
        case 0x00: return "Unknown";
        case 0x01: return "International";
        case 0x02: return "National";
        case 0x03: return "NetworkSpec";
        case 0x04: return "Subscriber";
        case 0x06: return "abbreviated";
        case 0x07: return "Reserved";
        }
	return "ERROR";
}
const char* mISDNNames::Presentation(int i) {
        switch(i) {
        case 0x00: return "Presentation Allowed";
        case 0x01: return "Presentation Restricted";
        case 0x02: return "Number not available";
        case 0x03: return "Reserved";
        }
	return "ERROR";
}
const char* mISDNNames::Screening(int i) {
        switch(i) {
        case 0x00: return "User-privided not screened";
        case 0x01: return "User-privided verified and passed";
        case 0x02: return "User-privided verified and failed";
        case 0x03: return "Network provided";
        }
	return "ERROR";
}


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
