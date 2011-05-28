

db_reg_agent module   Readme         (c) 2011 Stefan Sayer

Purpose
-------

The db_reg_agent module allows SEMS to read SIP accounts from a database
and register the accounts to SIP a registrar. In that it serves a similar
purpose as the reg_agent/registrar_client modules, with the differences
that it reads accounts from mysql DB instead of the file system, and that
it is built to support many (up to several 10k) subscription. Additionally,
accounts may be added, changed and removed while SEMS is running; the
db_reg_agent then can be triggered via DI interface (XMLRPC/json-rpc) to
pick up the new registration.

Features
- configurable subscription query
- configurable desired expires
- configurable contact: global hostport setting, or per registration
- monitoring of registration status and contacts through DB
- flatten out re-register spikes by intelligently planning registration refresh
- ratelimiting (x new REGISTER requests per y seconds)
- seamless restart of SEMS server possible; registration status is restored from DB.

DI control functions
--------------------

 createRegistration(int subscriber_id, string user, string pass, string realm [, string contact])
 updateRegistration(int subscriber_id, string user, string pass, string realm [, string contact])
 removeRegistration(int subscriber_id)

After removing a registration by issuing removeRegistration, the subcriber entry will
be present with the status REMOVED. 

Registration status (registration_status column)
------------------------------------------------
 REG_STATUS_INACTIVE      0
 REG_STATUS_PENDING       1
 REG_STATUS_ACTIVE        2
 REG_STATUS_FAILED        3
 REG_STATUS_REMOVED       4

Configuring contact
-------------------
By default the contact is constructed of sip:<user>@<public_ip/sip_ip:port> .

In some settings it may be desirable to control the Contact that is registered. The
contact_hostport setting overrides the host:port part. Per-registration contact can be
specified by setting option db_read_contact=yes and providing a contact in the
subscriber.contact column, and passing it with createRegistration/updateRegistration DI
function.

Database 
--------
There may be two separate tables, subscriptions and registrations (status). SEMS inserts
and updates entries only in the registrations table, where the last registration status
is kept, both for monitoring purposes, and to enable seamless server restart or failover.

The query which selects subscription and registration status as join may be configured
as well as the name of the registration status table (registrations_table). By pointing
to the same table, they can be configured to use only one database table.

Clocks of SEMS host and DB host must be synchronized in order for restart without
massive re-registration to work.

Example tables structure below.

Optional fields
 - registrations.contacts depending on save_contacts option
 - subscriber.contact depending on db_read_contact option

CREATE TABLE IF NOT EXISTS `registrations` (
  `subscriber_id` int(11) NOT NULL,
  `registration_status` tinyint(1) NOT NULL DEFAULT '0',
  `last_registration` datetime,
  `expiry` datetime,
  `last_code` smallint(2),
  `last_reason` varchar(256),
  `contacts` varchar(512),
  PRIMARY KEY (`subscriber_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

CREATE TABLE IF NOT EXISTS `subscribers` (
  `subscriber_id` int(10) NOT NULL AUTO_INCREMENT,
  `user` varchar(256) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `pass` varchar(256) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `realm` varchar(256) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `contact` varchar(256) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  PRIMARY KEY (`subscriber_id`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=2 ;

