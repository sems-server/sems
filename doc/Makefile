

.PHONY: doc
doc:
	doxygen doxygen_proj

#	lynx -dump doxygen_doc/html/index.html >index.txt
#	lynx -dump doxygen_doc/html/howtostart_noproxy.html >Howtostart_noproxy.txt
#	lynx -dump doxygen_doc/html/howtostart_simpleproxy.html >Howtostart_simpleproxy.txt
#	lynx -dump doxygen_doc/html/howtostart_voicemail.html >Howtostart_voicemail.txt
#	lynx -dump doxygen_doc/html/AppDoc.html >Applications.txt
#	lynx -dump doxygen_doc/html/ZRTP.html >ZRTP.txt
#	lynx -dump doxygen_doc/html/Tuning.html >Tuning.txt
#	lynx -dump doxygen_doc/html/ComponentDoc.html >ComponentModules.txt


.PHONY: fulldoc
fulldoc:
	doxygen doxygen_fulldoc_proj

.PHONY: clean
clean:
	rm -Rf doxygen_doc doxygen_fulldoc 

.PHONY: txtdoc_clean
txtdoc_clean:
	rm -rf index.txt Howtostart_noproxy.txt Howtostart_simpleproxy.txt Howtostart_voicemail.txt Applications.txt ZRTP.txt Tuning.txt ComponentModules.txt

.PHONY: txtdoc
txtdoc: doc
	lynx -dump doxygen_doc/html/index.html >index.txt
	lynx -dump doxygen_doc/html/howtostart_noproxy.html >Howtostart_noproxy.txt
	lynx -dump doxygen_doc/html/howtostart_simpleproxy.html >Howtostart_simpleproxy.txt
	lynx -dump doxygen_doc/html/howtostart_voicemail.html >Howtostart_voicemail.txt
	lynx -dump doxygen_doc/html/AppDoc.html >Applications.txt
	lynx -dump doxygen_doc/html/ZRTP.html >ZRTP.txt
	lynx -dump doxygen_doc/html/Tuning.html >Tuning.txt
	lynx -dump doxygen_doc/html/ComponentDoc.html >ComponentModules.txt
