uri.parse(<uri>, <prefix>)
  splits <uri> in 
    <prefix>display_name
    <prefix>user
    <prefix>host
    <prefix>port
    <prefix>headers
    <prefix>param

   example: 
    uri.parse(@remote_uri, remote_);
    uri.parse($PAI, pai_);

  * Sets $errno (general).

uri.parseNameaddr(<nameaddr>, <prefix>)
  splits <nameaddr> in 
    <prefix>display_name
    <prefix>user
    <prefix>host
    <prefix>port
    <prefix>headers
    <prefix>param

    <prefix>uri_param.<name> [= value]

   example: 
    uri.parseNameaddr(@remote_party, remote_);

  * Sets $errno (general).

uri.getHeader(<header>, <dst>)
    get header from initial INVITE into variable <dst>

    example:
    	uri.getHeader(P-Asserted-Identity, PAI);

 
