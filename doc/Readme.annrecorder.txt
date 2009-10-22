"annrecorder" application
-------------------------

This application lets the user record a personal greeting file.

The caller is presented with the current greeting, and can type a
key to record a new one etc.

The greeting is stored by a msg_storage file storage, and can 
be used as personal greeting message, e.g. for auto-attendant or
away message.

Prompts are stored at 
msg storage domain = <param_domain>"-prompts"
msg storage user   = <param_user>
msg storage msgname= <param_type>

where 

App-Params
----------
 short long        description
 -----+-----------+-----------
 dom   Domain      param_domain
 usr   User        param_user
 typ   Type        param_type; defaults to "vm" (for: voicemail)
 lng   Language    used for finding default greeting
 did   DomainID    optional, overrides Domain above
 uid   UserID      optional, overrides User above

Flow Diagram
------------

     |
     v
E: GREETING
E: YOUR_PROMPT
E: <current greeting or 
    default greeting>
     |
     v                       
E: TO_RECORD <--------------+
     |                      |
     v                      |
+-------------+    timeout  \
|S_WAIT_START | -------------|-------+
|             |             /        |
+-------------+             |        |
     | any key              |        |
     |                      |        |
     v                      |        |
E: BEEP                     |        |
<start recording to /tmp/>  |        |
     |                      |        |
     | any key/timeout      |        |
     v                      |        |
E: YOUR_PROMPT              |        |
E: <recording from /tmp/>   |        |
E: CONFIRM                  |        |
     |                      |        |
     v                      |        |
+-------------+             |        |
|S_CONFIRM    | key != 1    |        |
|             |-------------+        |
+-------------+                      |
     | key 1 / timeout               |
     v                               |
E: NEW GREETING SET                  |
     |                               | 
     v                               |
E: BYE <-----------------------------+
     | 
     v
+-------------+
|S_BYE        |
|             |
+-------------+
     | empty
     v
   send BYE
   stop
     
      
   
