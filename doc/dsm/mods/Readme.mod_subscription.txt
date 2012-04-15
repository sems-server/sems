SIP Specific Event Notification (RFC3265) - support for DSM

 subscription.create(info_struct)
  create new subscription. Parameters:
  $info_struct.domain    - SIP domain (RURI, From, To)
  $info_struct.user      - user (RURI, To)
  $info_struct.from_user - user (From)
  $info_struct.event     - Event package (e.g. "reg")

  $info_struct.pwd       - optional: password (for SIP auth)
  $info_struct.proxy     - optional: proxy URI (e.g. sip:10.0.0.1)
  $info_struct.accept    - optional: Accept content
  $info_struct.id        - optional: id (subscription ID to be used)
  $info_struct.expires   - optional: desired subscription expiration

 returns:
  $info_struct.handle   - handle for subscription (sub dialog ltag) 

 subscription.refresh(handle [, expires])
  refresh subscription

 subscription.remove(handle)
  unsubscribe & remove subscription



Example:
  set($r.domain="192.168.5.110");
  set($r.user="300");
  set($r.from_user="200");
  set($r.pwd="verysecret");
  -- set($r.proxy="sip:192.168.5.110");
  set($r.event="reg");
  -- set($r.id="115");
  -- set($r.expires="50");
  subscription.create(r);
  
  if test($r.handle!="") {
    log(1, $r.handle);
  } else {
    log(1, "subscription failed:");
    logVars();
  }
