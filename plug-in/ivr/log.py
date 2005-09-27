import ivr
import sys

# These are the same as in log.h
L_ERR  = 0
L_WARN = 1
L_INFO = 2
L_DBG  = 3

def log(level, msg, args):

	if args != None:
		tmp_msg = msg % args
	else:
		tmp_msg = msg
		
	ivr.log(level,"Ivr-Python: " + tmp_msg + "\n")


def error(msg, args=None):
	log(L_ERR, msg, args)

def warn(msg, args=None):
	log(L_WARN, msg, args)

def info(msg, args=None):
	log(L_INFO, msg, args)
	
def debug(msg, args=None):
	log(L_DBG, msg, args)


def stacktrace(tb):

	if tb: last_file = stacktrace(tb.tb_next)
	else: return

	f = tb.tb_frame.f_code.co_filename
	line = tb.tb_frame.f_lineno

	if f != last_file:
		error('File ' + `f` + ': line ' + `line`)
	else:
		error(', line ' + `line`)
	return f


def log_excepthook(exception, value, tb):

	error('********** Ivr-Python exception report ****************')
	error(str(exception) + ' raised: ' + str(value))
	stacktrace(tb)
	error('********** end of Ivr-Python exception report *********')



# init code
sys.excepthook = log_excepthook
debug("Python-Ivr logging started")
