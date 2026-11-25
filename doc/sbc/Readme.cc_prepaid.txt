Prepaid call control module
---------------------------

This module implements prepaid accounting with the credits stored in memory.
The credit of an account is the number of seconds the account can call.

When a call is started, the credit of the account ("uuid") is fetched, and a
call timer with that value is set, which is started when the call is connected.
When the call ends, the duration of the call (connect to end, rounded to
seconds) is subtracted from the credit.

 - if the account does not exist, the call is refused with 500
 - if the account has no credit left, the call is refused with
   402 Insufficient Credit

As the credits are kept in memory only, they are lost when the server is
restarted. The module can be used for testing, or as a starting point for
integration into custom billing systems.

Parameters (SBC profile)

uuid          - identifier of the account (mandatory)

DI functions

The credits are managed with the following DI functions, which can be called
e.g. through XMLRPC (xmlrpc2di module) or json-rpc (jsonrpc module):

  getCredit      <uuid>            returns [credit, found]
  setCredit      <uuid> <amount>   set the credit of an account; creates the
                                   account if it does not exist
  addCredit      <uuid> <amount>   add to the credit of an account
  subtractCredit <uuid> <amount>   subtract from the credit of an account;
                                   returns [credit, found]

Configuration example

sbc.conf:
  load_cc_plugins=cc_prepaid

SBC profile (account taken from P-Caller-Uuid header):
  call_control=prepaid
  prepaid_module=cc_prepaid
  prepaid_uuid=$H(P-Caller-Uuid)
