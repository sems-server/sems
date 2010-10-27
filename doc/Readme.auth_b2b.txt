
auth_b2b application 

-------------------------------------------------
This application has been obsoleted by the sbc
module and will be discontinued in the next version.
Please use the sbc module with the auth_b2b call
profile for the same functionality.
-------------------------------------------------


This module is a pure B2BUA application that does an identity 
change and authenticates on the second leg of the call, like 
this

Caller            SEMS auth_b2b                123@domainb
  |                     |                        |
  | INVITE bob@domaina  |                        |
  | From: alice@domaina |                        |
  | To: bob@domaina     |                        |
  | P-App-Param:u=user;d=domainb;p=passwd        |
  |-------------------->|                        |
  |                     |INVITE bob@domainb      |
  |                     |From: user@domainb      |
  |                     |To: bob@domainb         |
  |                     |----------------------->|
  |                     |                        |
  |                     |  407 auth required     |
  |                     |<---------------------- |
  |                     |                        |
  |                     |                        |
  |                     | INVITE w/ auth         |
  |                     |----------------------->|
  |                     |                        |
  |                     |  100 trying            |
  |  100 trying         |<---------------------- |
  |<--------------------|                        |
  |                     |                        |
  |                     |  200 OK                |
  |  200 OK             |<---------------------- |
  |<--------------------|                        |
  |                     |                        |
  | ACK                 |                        |
  |-------------------->| ACK                    |
  |                     |----------------------->|

App-Param:
  u - user in B leg
  d - domain in B leg
  p - password for auth in B leg (auth user=user)

Alternatively, the account (user/domain/pwd) can be set in auth_b2b.conf.