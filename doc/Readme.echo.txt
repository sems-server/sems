Echo application

The echo application echos the voice back to the caller.

The echo application resides in the core/plug-in directory.
Its purpose is mainly testing the Media Server and the 
total network delay. 

If the echo module is compiled with the 
STAR_SWITCHES_PLAYOUTBUFFER option (enabled by default), the 
star key pressed on the phone switches between adaptive playout 
buffer and fifo playout buffer. Thus the effect of the adaptive 
playout buffering can easily be verified.

The echo application is also a test application for RFC4028 SIP
Session Timers. For this, enable_session_timer=yes needs to be
set in echo.conf
