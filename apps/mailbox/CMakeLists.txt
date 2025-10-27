set(mailbox_audio_files
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
    you_have.wav)
set(sems_module_name mailbox)
include(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

python_sources_compile(mailbox.py ivr)
python_sources_compile(mailbox_query.py ivr)
python_sources_compile(imap_mailbox/MailboxURL.py ivr/imap_mailbox)
python_sources_compile(imap_mailbox/__init__.py ivr/imap_mailbox)
python_sources_compile(imap_mailbox/imap4ext.py ivr/imap_mailbox)

sems_install_configs("mailbox;mailbox_query")
sems_install_audio("${sems_module_name}" "${${sems_module_name}_audio_files}")
