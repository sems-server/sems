#!/usr/bin/perl
# DTMF testing script for sems ivr (perl version)
# The purpose of this is to illustrate how to write a perl script for sems ivr module
# It plays back the keys user enters and can be used for DTMF detection testing. 
my @keys=();
use Sys::Syslog qw(:DEFAULT setlogsock);

syslog('info', 'this is the beginning');
my $wav_path = '/usr/local/lib/sems/audio/';

# enable DTMF detection, it is disabled by default
ivr::enableDTMFDetection();

# setup callback functions
# It is important to setup the OnBYE callback. This gives the script a chance to exit
# gracefully. This allows files to be close, database updated, state synchronized.
# Otherwise the script is killed by the sems ivr module abruptly.
syslog('info', 'setting DTMF callback: '. ivr::setCallback('func_on_DTMF', 'onDTMF') );
syslog('info', 'setting BYE callback: '. ivr::setCallback('func_on_BYE', 'onBYE') );

# play initial greeting
syslog('info', 'filling media '. ivr::enqueueMediaFile($wav_path . 'thanks_calling_number_reader.wav', 0));

# save the user input to a wav file, use called user and domain as part of file name
# username part is retrived via ivr::getUser()
# domain is retrieved via ivr::getDomain()
ivr::startRecording('/tmp/record-(' . ivr::getUser(). '@'. ivr::getDomain(). ')-'. ivr::getTime(). '.wav');

# if not key input for 3 cycles, then exit. Each cycle is 5 seconds (or 5000 msec)
# It is important for the script to exist by itself. For example, it the caller UA crashes,
# it won't be able to send BYE, this instance of the script is going to be a orphan.
# This can easily to be a victim of DOS attack.
my $exit_flag=3;

# main loop
while($exit_flag>=0) {

	# sleep 5000 msec, there are other functions of seconds and useconds
	# it either sleeps for 5000 msec or interrupted by ivr::wakeUp();
	$sleep_time = ivr::msleep(5000);
	syslog('info', 'wake up after sleep '. $sleep_time . ' msec.\n');

	# user input key is saved in @keys array
	my $key = shift (@keys);

	# check if there is any key
	if (defined($key)) {
		syslog('info', 'A new key '. $key . " is pressed after $sleep_time msec");

		# play the key wav file
		ivr::enqueueMediaFile("$wav_path" . $key . '.wav', 0);

		# we get a key, restart timer
		$exit_flag=3;

	} else {
		# no key input, decrease exit counter
		$exit_flag--;
	}
}

# stop recording
ivr::stopRecording();

#syslog('info', 'processed in ' .ivr::yield() . 'usec');

syslog('info', 'this is the end');

# reset the callback function, recommended, avoid callback function called after script is over
syslog('info', 'setting DTMF callback: '. ivr::setCallback('', 'onDTMF') );
syslog('info', 'setting BYE callback: '. ivr::setCallback('', 'onBYE') );


# DTMF callback function, called when there is any key input
sub func_on_DTMF {
	# save the key to @keys
	push @keys, @_;
	syslog('info', "callback says: a key @_ is entered");
	# wakeup main script sleep function
	ivr::wakeUp();
}

# BYE callback function, called when caller hangs up
sub func_on_BYE {
	syslog('info', 'callback says: a bye in detected');
	# set flag for main loop
	$exit_flag=0;
	# wakeup main script sleep function
	ivr::wakeUp();
}

