
#ifndef __AMSIPHEADERS_H__
#define __AMSIPHEADERS_H__

#define SIP_SCHEME_SIP          "sip"

#define SIP_HDR_FROM            "From"
#define SIP_HDR_TO              "To"
#define SIP_HDR_ROUTE           "Route"
#define SIP_HDR_CONTENT_TYPE    "Content-Type"
#define SIP_HDR_CONTACT         "Contact"

#define SIP_HDR_COL(_hdr)       _hdr ":"
#define SIP_HDR_COLSP(_hdr)     SIP_HDR_COL(_hdr) " "

#define CRLF                    "\r\n"
#define SIP_HDR_LEN(_hdr)       (sizeof(_hdr) - /*0-term*/1)

#endif /* __AMSIPHEADERS_H__ */
