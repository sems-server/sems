.PHONY: doc
doc:
	make -C ../core doc
	lynx -dump doxygen_doc/html/index.html >index.txt
	lynx -dump doxygen_doc/html/howtostart_noproxy.html >Howtostart_noproxy.txt
	lynx -dump doxygen_doc/html/howtostart_simpleproxy.html >Howtostart_simpleproxy.txt
	lynx -dump doxygen_doc/html/howtostart_voicemail.html >Howtostart_voicemail.txt

	lynx -dump doxygen_doc/html/AppDoc.html >Applications.txt
	lynx -dump doxygen_doc/html/ZRTP.html >ZRTP.txt
	lynx -dump doxygen_doc/html/Tuning.html >Tuning.txt
	lynx -dump doxygen_doc/html/ComponentDoc.html >ComponentModules.txt


.PHONY: fulldoc
fulldoc:
	make -C ../core/ fulldoc

