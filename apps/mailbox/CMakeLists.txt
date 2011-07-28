set (mailbox_audio_files
	and.wav
	beep.wav
	bye.wav
	default_en.wav
	first_msg.wav
	msg_deleted.wav
	msg_menu.wav
	msg_saved.wav
	new_msg.wav
	next_msg.wav
	no_msg.wav
	saved_msg.wav
	you_have.wav
)
SET(sems_module_name mailbox)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

PYTHON_SOURCES_COMPILE(mailbox.py ivr)
PYTHON_SOURCES_COMPILE(mailbox_query.py ivr)
PYTHON_SOURCES_COMPILE(imap_mailbox/MailboxURL.py ivr/imap_mailbox)
PYTHON_SOURCES_COMPILE(imap_mailbox/__init__.py ivr/imap_mailbox)
PYTHON_SOURCES_COMPILE(imap_mailbox/imap4ext.py ivr/imap_mailbox)

SEMS_INSTALL_CONFIGS("mailbox;mailbox_query")
SEMS_INSTALL_AUDIO("${sems_module_name}" "${${sems_module_name}_audio_files}")
