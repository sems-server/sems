import ivr
import sys,logging

from com.iptel.log.Logger import *

# These are the same as in log.h
L_ERR  = 0
L_WARN = 1
L_INFO = 2
L_DBG  = 3

def fromPyLogLevel(py_level):

	level = 0
	if py_level >= logging.ERROR:
		level = L_ERR
	elif py_level >= logging.WARN:
		level = L_WARN
	elif py_level >= logging.INFO:
		level = L_INFO
	else:
		level = L_DBG

	return level
	

def toPyLogLevel(level):

	py_level = 0
	if level >= L_DBG:
		py_level = logging.DEBUG
	elif level >= L_INFO:
		py_level = logging.INFO
	elif level >= L_WARN:
		py_level = logging.WARN
	else:
		py_level = logging.ERROR

	return py_level


class SemsLogHandler(logging.Handler):

	def emit(self, record):

		msg = self.format(record)
		ivr.log(fromPyLogLevel(record.levelno),msg + '\n')


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
if not hasattr(Logger,"logger"):
	initLogger("sems",toPyLogLevel(ivr.SEMS_LOG_LEVEL))
	getLogger().addLogHandler(SemsLogHandler())

sys.excepthook = log_excepthook
debug("Python-Ivr logging started")
