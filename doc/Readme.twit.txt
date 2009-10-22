twitter app 
-----------

everyone does twitter, so does iptel.org... 

this is a simple ivr application to record a message and tweet the link to it.

It uses http://is.gd to shorten the link, and python twyt
(http://andrewprice.me.uk/projects/twyt/) to actually post the tweet. 
Install twyt with setup.py or copy the directory 'twyt' into /usr/lib/sems/ivr/.

You need to setup a web server to serve the files where they are recorded.

The twitter account, and optionally the tweet, may be sent either in the
INVITE request URI: 
 sip:twitter;twitter_user;twitter_password@host
or: 
 sip:twitter;twitter_user;twitter_password;tweet_text@host

or sent as P-App-Param header:
 P-App-Param: u=twitter_user;p=twitter_password;m=tweet_text
or
 P-App-Param: u=twitter_user;p=twitter_password

tweet_text may be url-encoded (hi%20there).

The maximum length of a message is 180s (it's about short nonsense, isn't it?).

sample messages: 
---------------

welcome_msg.wav : welcome! iptel does twitter. record your tweet and hit a key.

twit_account_msg.wav : welcome! iptel does twitter, but you need to set your account first. 
head over to iptel dot org slash service, and enter your account in the extras tab. 
you can also set the account in the call forwarding settings. see you!

twit_posting_msg.wav: ok, i try to tweet this now.

twit_ok_msg.wav: thanks, your tweet has been posted.

twit_err_msg.wav: sorry, i think your twitter account does not work.


Tweet from iptel.org: 
--------------------
Call sip:tweet@iptel.org to leave a tweet on iptel.org's feed: twitter.com/ipteldotorg. 

Set your twitter account in serweb "other" settings to leave tweets on your own feed by calling
the number 1010 (sip:1010@iptel.org) or the vanity number 8948 (twit). You can also call 
sip:twit;<twitter user>;<twitter pwd>;<tweet>@iptel.org from anywhere.

Have a look at http://iptel.org/service for details.