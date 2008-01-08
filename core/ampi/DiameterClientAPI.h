#ifndef _DIAMETER_CLIENT_API_H
#define _DIAMETER_CLIENT_API_H

#include "AmEvent.h"
 
#define 	AAA_AVP_FLAG_NONE               0x00
#define 	AAA_AVP_FLAG_MANDATORY          0x40
#define 	AAA_AVP_FLAG_RESERVED           0x1F
#define 	AAA_AVP_FLAG_VENDOR_SPECIFIC    0x80
#define 	AAA_AVP_FLAG_END_TO_END_ENCRYPT 0x20


// new_connection
//   string app_name
//   string server_ip
//   unsigned int server_port
//   string origin_host
//   string origin_realm
//   string origin_ip
//   unsigned int app_id
//   unsigned int vendor_id
//   string product_name


// sendRequest
//   string app_name
//   unsigned int command_code
//   unsigned int app_id
//   arg val
//   string sess_link

//   args:  array
//     [int avp_id, int flags, int vendor, blob data]

//  returns :
//    0    OK
//    != 0 error

// reply events : 
//  avps:
// [ code, flags, vendorId, type, data_blob ]

struct DiameterReplyEvent 
  : public AmEvent
{
  unsigned int commandCode;
  unsigned int applicationId;

  AmArg avps;

 DiameterReplyEvent(unsigned int commandCode, 
		    unsigned int applicationId, AmArg avps)
   : AmEvent(0), commandCode(commandCode), 
    applicationId(applicationId), avps(avps)
  { }
};

#endif
