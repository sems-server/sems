uri.parse(<uri>, <prefix>)
  splits <uri> in 
    <prefix>display_name
    <prefix>user
    <prefix>host
    <prefix>param

   example: 
    uri.parse(@remote_uri, remote_);
    uri.parse($PAI, pai_);

uri.getHeader(<header>, <dst>)
    get header from initial INVITE into variable <dst>

    example:
    	uri.getHeader(P-Asserted-Identity, PAI);

 