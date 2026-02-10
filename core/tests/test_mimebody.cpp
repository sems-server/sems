#include "fct.h"

#include "AmMimeBody.h"

#define CRLF "\r\n"

FCTMF_SUITE_BGN(test_mimebody) {

  FCT_TEST_BGN(multipart_boundary_at_end) {
    AmMimeBody body;
    string boundary = "boundary42";
    string payload = "v=0" CRLF "s=-";
    string content_type = "multipart/mixed; boundary=" + boundary;
    string body_content =
      "--" + boundary + CRLF
      "Content-Type: application/sdp" CRLF
      CRLF
      + payload + CRLF
      "--" + boundary + "--";

    fct_chk(!body.parse(content_type,
                        reinterpret_cast<const unsigned char*>(body_content.data()),
                        body_content.size()));
    fct_chk(body.getParts().size() == 1);

    const AmMimeBody* part = body.getParts().front();
    string part_payload(reinterpret_cast<const char*>(part->getPayload()), part->getLen());
    fct_chk(part->getCTStr() == "application/sdp");
    fct_chk(part_payload == payload);
  } FCT_TEST_END();

} FCTMF_SUITE_END();
