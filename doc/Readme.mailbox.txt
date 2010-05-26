Documentation for SEMS mailbox application

The mailbox application is a mailbox where callers can leave messages 
for offline or unavailable users and the users can dial in to check their
messages. It uses an IMAP server as backend to store the voice messages.

The mailbox comes with two applications which run in the ivr:

mailbox        -  the application to leave a message
mailbox_query  -  the application to listen to the messages


Configuration Parameters
========================
mailbox.conf:
   annoucement_file     -    prompt to be played to caller before 
                             message is recorded
   beep_file            -    beep to be played after prompt

mailbox_query.conf:
   wav_dir              -    directory which contains wav files for
                             menu

Session Parameters (In P-Iptel-Param headers)
=============================================

mailbox:

 Mailbox-URL      -  IMAP URL to the mailbox

mailbox_query:

 Mailbox-URL      -  IMAP URL to the mailbox



Example for ser.cfg
===================

# for all INVITEs that should go to mailbox
#
        # replace this with a function that loads imap url e.g. from DB
        # into $mailbox_url AVP
	avp_write("Mailbox-URL=imap://user:password@imapserver:143/INBOX",
		  "$mailbox_url");

	append_hf_value("P-App-Name","mailbox");
       	append_hf_value("P-App-Param","%$mailbox_url");

	# assume that SEMS is running at localhost:5080
	t_relay_to("udp:localhost:5080");
	break;
