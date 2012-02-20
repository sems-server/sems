#ifndef _defs_h_
#define _defs_h_

#define SIP_SCHEME_SIP          "sip"

#define SIP_METH_INVITE         "INVITE"
#define SIP_METH_CANCEL         "CANCEL"
#define SIP_METH_PRACK          "PRACK"
#define SIP_METH_UPDATE         "UPDATE"
#define SIP_METH_INFO           "INFO"
#define SIP_METH_BYE            "BYE"
#define SIP_METH_ACK            "ACK"
#define SIP_METH_SUBSCRIBE      "SUBSCRIBE"
#define SIP_METH_NOTIFY         "NOTIFY"

#define SIP_HDR_FROM            "From"
#define SIP_HDR_TO              "To"
#define SIP_HDR_VIA             "Via"
#define SIP_HDR_CSEQ            "CSeq"
#define SIP_HDR_CALL_ID         "Call-ID"
#define SIP_HDR_ROUTE           "Route"
#define SIP_HDR_RECORD_ROUTE    "Record-Route"
#define SIP_HDR_CONTENT_TYPE    "Content-Type"
#define SIP_HDR_CONTENT_LENGTH  "Content-Length"
#define SIP_HDR_CONTACT         "Contact"
#define SIP_HDR_SUPPORTED       "Supported"
#define SIP_HDR_UNSUPPORTED     "Unsupported"
#define SIP_HDR_REQUIRE         "Require"
#define SIP_HDR_SERVER          "Server"
#define SIP_HDR_USER_AGENT      "User-Agent"
#define SIP_HDR_MAX_FORWARDS    "Max-Forwards"
#define SIP_HDR_P_ASSERTED_IDENTITY "P-Asserted-Identity"
#define SIP_HDR_P_PREFERRED_IDENTITY "P-Preferred-Identity"
#define SIP_HDR_REFER_TO        "Refer-To"
#define SIP_HDR_EXPIRES         "Expires"
#define SIP_HDR_SESSION_EXPIRES "Session-Expires"
#define SIP_HDR_MIN_SE          "Min-SE"
#define SIP_HDR_RSEQ            "RSeq"
#define SIP_HDR_RACK            "RAck"
#define SIP_HDR_AUTHORIZATION   "Authorization"
#define SIP_HDR_PROXY_AUTHORIZATION "Proxy-Authorization"
#define SIP_HDR_PROXY_AUTHENTICATE "Proxy-Authenticate"
#define SIP_HDR_WWW_AUTHENTICATE "WWW-Authenticate"
#define SIP_HDR_ALLOW            "Allow"
#define SIP_HDR_RETRY_AFTER      "Retry-After"

#define SIP_HDR_COL(_hdr)       _hdr ":"
#define SIP_HDR_COLSP(_hdr)     SIP_HDR_COL(_hdr) " "
#define COLSP                   ": "

#define CRLF                    "\r\n"
#define SIP_HDR_LEN(_hdr)       (sizeof(_hdr) - /*0-term*/1)

#define SIP_EXT_100REL          "100rel"

#define SIP_HDR_SESSION_EXPIRES_COMPACT "x"
#define SIP_HDR_SUPPORTED_COMPACT "k"

#define SIP_IS_200_CLASS(code)  ((code >= 200) && (code < 300))

#define SIP_APPLICATION_SDP     "application/sdp"

#define SIP_REPLY_SERVER_INTERNAL_ERROR "Server Internal Error"
#define SIP_REPLY_BAD_EXTENSION         "Bad Extension"
#define SIP_REPLY_EXTENSION_REQUIRED    "Extension Required"
#define SIP_REPLY_LOOP_DETECTED         "Loop Detected"
#define SIP_REPLY_NOT_EXIST             "Call Leg/Transaction Does Not Exist"
#define SIP_REPLY_PENDING               "Request Pending"
#define SIP_REPLY_NOT_ACCEPTABLE_HERE   "Not Acceptable Here"

#endif
