
#ifndef __AMSIPHEADERS_H__
#define __AMSIPHEADERS_H__

#define SIP_SCHEME_SIP          "sip"

#define SIP_METH_INVITE         "INVITE"
#define SIP_METH_PRACK          "PRACK"
#define SIP_METH_UPDATE         "UPDATE"

#define SIP_HDR_FROM            "From"
#define SIP_HDR_TO              "To"
#define SIP_HDR_ROUTE           "Route"
#define SIP_HDR_CONTENT_TYPE    "Content-Type"
#define SIP_HDR_CONTACT         "Contact"
#define SIP_HDR_SUPPORTED       "Supported"
#define SIP_HDR_REQUIRE         "Require"
#define SIP_HDR_SERVER          "Server"
#define SIP_HDR_USER_AGENT      "User-Agent"
#define SIP_HDR_MAX_FORWARDS    "Max-Forwards"
#define SIP_HDR_P_ASSERTED_IDENTITY "P-Asserted-Identity"
#define SIP_HDR_REFER_TO        "Refer-To"
#define SIP_HDR_EXPIRES         "Expires"
#define SIP_HDR_RSEQ            "RSeq"
#define SIP_HDR_RACK            "RAck"
#define SIP_HDR_COL(_hdr)       _hdr ":"
#define SIP_HDR_COLSP(_hdr)     SIP_HDR_COL(_hdr) " "

#define CRLF                    "\r\n"
#define SIP_HDR_LEN(_hdr)       (sizeof(_hdr) - /*0-term*/1)

#define SIP_EXT_100REL          "100rel"

#endif /* __AMSIPHEADERS_H__ */
