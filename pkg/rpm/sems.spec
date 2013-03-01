Summary:	SIP Express Media Server, an extensible SIP media server
Name:		sems
Version:	1.6.0
Release:	1
URL:		http://www.iptel.org/sems
# svn -r 1095 export http://svn.berlios.de/svnroot/repos/sems/branches/1.0.0 sems-1.0.0
# tar cjvf sems-1.0.0.tar.bz2 sems-1.0.0
Source:		%{name}-%{version}-%{release}.tar.gz
License:	GPLv2+
Group:		Applications/Communications
# Enable OpenSER
#Patch0:		sems--openser_enable.diff
# Use external gsm instead of shipped one
#Patch2:		sems--external_gsm_lib.diff
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:	python >= 2.3
BuildRequires:	sip-devel
#BuildRequires:	libsamplerate-devel
#BuildRequires:	gsm-devel
#BuildRequires:	spandsp-devel
# TODO consider enabling flite support in apps/conference
#BuildRequires:	flite-devel
BuildRequires:	speex-devel
Requires(post):	/sbin/chkconfig
Requires(preun):/sbin/chkconfig
Requires(preun):/sbin/service

%description
SEMS (SIP Express Media Server) is very extensible and programmable
SIP media server for SER or OpenSER. The plug-in based SDK enables
you to extend SEMS and write your own applications and integrate new
codec. Voicemail, announcement and echo plug-ins are already included.
SEMS supports g711u, g711a, GSM06.10 and wav file.

%package	ivr
Summary:	IVR functionality for SEMS
Group:		Applications/Communications
Requires:	python >= 2.3
Requires:	%{name} = %{version}-%{release}

%description	ivr
IVR functionality for SEMS

%package	speex
Summary:	Speex support for SEMS
Group:		Applications/Communications
Requires:	%{name} = %{version}-%{release}

%description	speex
Speex support for SEMS

%package	gsm
Summary:	GSM support for SEMS
Group:		Applications/Communications
Requires:	%{name} = %{version}-%{release}

%description	gsm
GSM support for SEMS

%package python
Summary:	Python bindings for SEMS
Group:		Applications/Communications
Requires:	python >= 2.3
Requires:	%{name} = %{version}-%{release}

%description	python
Python bindings for SEMS

%package conf_auth
Summary:	conf_auth
Group:		Applications/Communications
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-ivr = %{version}-%{release}

%description conf_auth
Module conf_auth

%package mailbox
Summary:	mailbox
Group:		Applications/Communications
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-ivr = %{version}-%{release}

%description mailbox
Module mailbox

%package pin_collect
Summary:	Collects a PIN
Group:		Applications/Communications
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-ivr = %{version}-%{release}

%description pin_collect
This application collects a PIN and then transfers using a
(proprietary) REFER the call`

%prep
%setup -q
#rm -rf core/plug-in/gsm/gsm-1.0-pl10/
#%patch0 -p0 -b .openser_enable
#%patch2 -p0 -b .gsm_ext
iconv -f iso8859-1 -t UTF-8 apps/diameter_client/Readme.diameter_client > apps/diameter_client/Readme.diameter_client.utf8 && mv apps/diameter_client/Readme.diameter_client{.utf8,}
iconv -f iso8859-1 -t UTF-8 doc/Readme.voicebox > doc/Readme.voicebox.utf8 && mv doc/Readme.voicebox{.utf8,}

%build
make %{?_smp_mflags} EXTRA_CXXFLAGS="$RPM_OPT_FLAGS" TTS="y" exclude_modules="examples %{!?with_ilbc:ilbc} mp3" all

%install
rm -rf $RPM_BUILD_ROOT
export CFLAGS="$RPM_OPT_FLAGS"
#make install TTS="y" exclude_modules="examples %{!?with_ilbc:ilbc} mp3" \
#	DESTDIR=$RPM_BUILD_ROOT \
#	basedir= \
#	prefix=/usr \
#	modules-prefix= \
#       modules-dir=/usr/lib/sems/plug-in \
#        modules-target=/usr/lib/sems/plug-in \
#	ivr-modules-dir=/usr/lib/sems/ivr \
#	cfg-prefix=/ \
#	cfg-target=%{_sysconfdir}/sems/ \
#	doc-prefix= \
#	doc-dir=%{_docdir}/sems/ \
#	audio-prefix= \
#	audio-dir=/usr/lib/sems/audio/
make install TTS="y" exclude_modules="examples %{!?with_ilbc:ilbc} mp3" \
	DESTDIR=$RPM_BUILD_ROOT \
	prefix=/usr cfg-prefix=/ cfg-target=/etc/sems/
install -D -p -m755 pkg/rpm/sems.init $RPM_BUILD_ROOT/%{_sysconfdir}/init.d/sems

# Remove installed README
rm -rf $RPM_BUILD_ROOT%{_docdir}/sems
rm -rf $RPM_BUILD_ROOT%{_sysconfdir}/sems/default.template.sample
rm -rf $RPM_BUILD_ROOT%{_sysconfdir}/sems/sems.conf.default

%clean
rm -rf $RPM_BUILD_ROOT

%post
if [ $1 -eq 1 ]; then
	/sbin/chkconfig  --add sems || :
fi

%preun
if [ $1 -eq 0 ]; then
	/sbin/service sems stop >/dev/null 2>&1 || :
	/sbin/chkconfig --del sems || :
fi

%files
%defattr(-,root,root)
%dir %{_sysconfdir}/sems
%dir %{_sysconfdir}/sems/etc
##%config(noreplace) %{_sysconfdir}/sems/default.template
%config(noreplace) %{_sysconfdir}/sems/sems.conf
%config(noreplace) %{_sysconfdir}/sems/etc/ann_b2b.conf
%config(noreplace) %{_sysconfdir}/sems/etc/announce_transfer.conf
%config(noreplace) %{_sysconfdir}/sems/etc/announcement.conf
%config(noreplace) %{_sysconfdir}/sems/etc/annrecorder.conf
%config(noreplace) %{_sysconfdir}/sems/etc/app_mapping.conf
##%config(noreplace) %{_sysconfdir}/sems/etc/binrpcctrl.conf
##%config(noreplace) %{_sysconfdir}/sems/etc/call_timer.conf
%config(noreplace) %{_sysconfdir}/sems/etc/callback.conf
%config(noreplace) %{_sysconfdir}/sems/etc/click2dial.conf
%config(noreplace) %{_sysconfdir}/sems/etc/conference.conf
%config(noreplace) %{_sysconfdir}/sems/etc/early_announce.conf
%config(noreplace) %{_sysconfdir}/sems/etc/dsm.conf
%config(noreplace) %{_sysconfdir}/sems/etc/dsm_in_prompts.conf
%config(noreplace) %{_sysconfdir}/sems/etc/dsm_out_prompts.conf
##%config(noreplace) %{_sysconfdir}/sems/etc/gateway.conf
%config(noreplace) %{_sysconfdir}/sems/etc/ivr.conf
%config(noreplace) %{_sysconfdir}/sems/etc/msg_storage.conf
%config(noreplace) %{_sysconfdir}/sems/etc/precoded_announce.conf
##%config(noreplace) %{_sysconfdir}/sems/etc/py_sems.conf
%config(noreplace) %{_sysconfdir}/sems/etc/reg_agent.conf
##%config(noreplace) %{_sysconfdir}/sems/etc/sipctrl.conf
%config(noreplace) %{_sysconfdir}/sems/etc/stats.conf
##%config(noreplace) %{_sysconfdir}/sems/etc/sw_prepaid_sip.conf
##%config(noreplace) %{_sysconfdir}/sems/etc/unixsockctrl.conf
%config(noreplace) %{_sysconfdir}/sems/etc/voicebox.conf
%config(noreplace) %{_sysconfdir}/sems/etc/voicemail.conf
%config(noreplace) %{_sysconfdir}/sems/etc/webconference.conf
##%config(noreplace) %{_sysconfdir}/sems/etc/xmlrpc2di.conf
%config(noreplace) %{_sysconfdir}/sems/etc/auth_b2b.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/sems/etc/call_timer.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/sems/etc/codecfilter.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/sems/etc/default.template
%config(noreplace) %{_sysconfdir}/sems/etc/default.template.sample
%config(noreplace) %{_sysconfdir}/sems/etc/echo.conf
%config(noreplace) %{_sysconfdir}/sems/etc/mod_regex.conf
%config(noreplace) %{_sysconfdir}/sems/etc/monitoring.conf
%config(noreplace) %{_sysconfdir}/sems/etc/mwi.conf
%config(noreplace) %{_sysconfdir}/sems/etc/prepaid.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/sems/etc/refuse.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/sems/etc/replytranslate.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/sems/etc/rtmp.conf
%config(noreplace) %{_sysconfdir}/sems/etc/sbc.conf
%config(noreplace) %{_sysconfdir}/sems/etc/src_ipmap.conf
%config(noreplace) %{_sysconfdir}/sems/etc/sst_b2b.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/sems/etc/symmetricrtp.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/sems/etc/transparent.sbcprofile.conf



%doc README
%doc apps/examples/tutorial/cc_acc/Readme.cc_acc
%doc doc/figures
%doc doc/CHANGELOG
%doc doc/COPYING
#%doc doc/Configure-Sems-OpenSER-HOWTO
#%doc doc/Configure-Sems-Ser-HOWTO
%doc doc/Readme.ann_b2b.txt
%doc doc/Readme.announce_transfer.txt
%doc doc/Readme.announcement.txt
%doc doc/Readme.annrecorder.txt
%doc doc/Readme.auth_b2b.txt
%doc doc/Readme.call_timer.txt
%doc doc/Readme.callback.txt
%doc doc/Readme.click2dial.txt
%doc doc/Readme.conf_auth.txt
%doc doc/Readme.conference.txt
%doc doc/Readme.diameter_client.txt
%doc doc/Readme.early_announce.txt
%doc doc/Readme.echo.txt
%if 0%{?with_ilbc}
%doc doc/Readme.iLBC.txt
%endif
#%doc doc/Readme.mp3plugin
%doc doc/Readme.msg_storage.txt
%doc doc/Readme.reg_agent.txt
%doc doc/Readme.registrar_client.txt
%doc doc/Readme.sw_prepaid_sip.txt
%doc doc/Readme.uac_auth.txt
%doc doc/Readme.voicebox.txt
%doc doc/Readme.voicemail.txt
%doc doc/Readme.webconference.txt
#%doc doc/WHATSNEW

%{_sysconfdir}/init.d/sems

%{_sbindir}/sems
%{_sbindir}/sems-stats
%{_sbindir}/sems-get-callproperties
%{_sbindir}/sems-list-active-calls
%{_sbindir}/sems-list-calls
%{_sbindir}/sems-list-finished-calls
%{_sbindir}/sems-logfile-callextract
%{_sbindir}/sems-sbc-get-activeprofile
%{_sbindir}/sems-sbc-get-regex-map-names
%{_sbindir}/sems-sbc-list-profiles
%{_sbindir}/sems-sbc-load-callcontrol-modules
%{_sbindir}/sems-sbc-load-profile
%{_sbindir}/sems-sbc-reload-profile
%{_sbindir}/sems-sbc-reload-profiles
%{_sbindir}/sems-sbc-set-activeprofile
%{_sbindir}/sems-sbc-set-regex-map
%{_sbindir}/sems-sbc-teardown-call

%dir /usr/lib/sems
%dir /usr/lib/sems/audio
%dir /usr/lib/sems/audio/ann_b2b
%dir /usr/lib/sems/audio/announcement
%dir /usr/lib/sems/audio/announce_transfer
%dir /usr/lib/sems/audio/annrecorder
%dir /usr/lib/sems/audio/conference
%dir /usr/lib/sems/audio/voicebox
%dir /usr/lib/sems/audio/voicemail
%dir /usr/lib/sems/audio/webconference
%dir /usr/lib/sems/plug-in


/usr/lib/sems/audio/beep.wav
/usr/lib/sems/audio/default_en.wav
/usr/lib/sems/audio/annrecorder/beep.wav
/usr/lib/sems/audio/annrecorder/bye.wav
/usr/lib/sems/audio/annrecorder/confirm.wav
/usr/lib/sems/audio/annrecorder/greeting_set.wav
/usr/lib/sems/audio/annrecorder/to_record.wav
/usr/lib/sems/audio/annrecorder/welcome.wav
/usr/lib/sems/audio/annrecorder/your_prompt.wav
/usr/lib/sems/audio/conference/beep.wav
/usr/lib/sems/audio/conference/first_participant.wav
/usr/lib/sems/audio/voicebox/*.wav
/usr/lib/sems/audio/voicemail/default_en.wav
/usr/lib/sems/audio/voicemail/beep.wav
/usr/lib/sems/audio/webconference/*.wav

/usr/lib/sems/plug-in/adpcm.so
/usr/lib/sems/plug-in/ann_b2b.so
/usr/lib/sems/plug-in/announce_transfer.so
/usr/lib/sems/plug-in/announcement.so
/usr/lib/sems/plug-in/annrecorder.so
#/usr/lib/sems/plug-in/auth_b2b.so
#/usr/lib/sems/plug-in/binrpcctrl.so
#/usr/lib/sems/plug-in/call_timer.so
/usr/lib/sems/plug-in/callback.so
/usr/lib/sems/plug-in/cc_acc.so
/usr/lib/sems/plug-in/click2dial.so
/usr/lib/sems/plug-in/conference.so
/usr/lib/sems/plug-in/diameter_client.so
/usr/lib/sems/plug-in/dsm.so
/usr/lib/sems/plug-in/early_announce.so
/usr/lib/sems/plug-in/echo.so
%if 0%{?with_ilbc}
/usr/lib/sems/plug-in/ilbc.so
%endif
/usr/lib/sems/plug-in/l16.so
/usr/lib/sems/plug-in/msg_storage.so
/usr/lib/sems/plug-in/precoded_announce.so
/usr/lib/sems/plug-in/reg_agent.so
/usr/lib/sems/plug-in/registrar_client.so
#/usr/lib/sems/plug-in/sipctrl.so
/usr/lib/sems/plug-in/session_timer.so
/usr/lib/sems/plug-in/stats.so
#/usr/lib/sems/plug-in/sw_prepaid_sip.so
/usr/lib/sems/plug-in/uac_auth.so
#/usr/lib/sems/plug-in/unixsockctrl.so
/usr/lib/sems/plug-in/voicebox.so
/usr/lib/sems/plug-in/voicemail.so
/usr/lib/sems/plug-in/wav.so
/usr/lib/sems/plug-in/webconference.so
/usr/lib/sems/plug-in/cc_call_timer.so
/usr/lib/sems/plug-in/cc_ctl.so
/usr/lib/sems/plug-in/cc_pcalls.so
/usr/lib/sems/plug-in/cc_prepaid.so
/usr/lib/sems/plug-in/cc_prepaid_xmlrpc.so
/usr/lib/sems/plug-in/cc_rest.so
/usr/lib/sems/plug-in/cc_syslog_cdr.so
/usr/lib/sems/plug-in/ilbc.so
/usr/lib/sems/plug-in/isac.so
/usr/lib/sems/plug-in/monitoring.so
/usr/lib/sems/plug-in/mwi.so
/usr/lib/sems/plug-in/sbc.so


##/usr/lib/sems/dsm/mod_dlg.so
##/usr/lib/sems/dsm/mod_sys.so
##/usr/lib/sems/dsm/mod_uri.so
##/usr/lib/sems/dsm/inbound_call.dsm
##/usr/lib/sems/dsm/outbound_call.dsm
/usr/lib/sems/dsm/inbound_call.dsm
/usr/lib/sems/dsm/mod_conference.so
/usr/lib/sems/dsm/mod_dlg.so
/usr/lib/sems/dsm/mod_groups.so
/usr/lib/sems/dsm/mod_monitoring.so
/usr/lib/sems/dsm/mod_py.so
/usr/lib/sems/dsm/mod_regex.so
/usr/lib/sems/dsm/mod_subscription.so
/usr/lib/sems/dsm/mod_sys.so
/usr/lib/sems/dsm/mod_uri.so
/usr/lib/sems/dsm/mod_utils.so
/usr/lib/sems/dsm/outbound_call.dsm


%files conf_auth
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/sems/etc/conf_auth.conf
%doc doc/Readme.conf_auth.txt
/usr/lib/sems/ivr/conf_auth.pyc

%files gsm
%defattr(-,root,root)
/usr/lib/sems/plug-in/gsm.so

%files ivr
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/sems/etc/ivr.conf
%doc doc/Readme.ivr.txt
%dir /usr/lib/sems/ivr
##/usr/lib/sems/plug-in/ivr.so
##/usr/lib/sems/plug-in/log.pyc

%files mailbox
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/sems/etc/mailbox.conf
%config(noreplace) %{_sysconfdir}/sems/etc/mailbox_query.conf
%doc doc/Readme.mailbox.txt
%dir /usr/lib/sems/audio/mailbox
%dir /usr/lib/sems/ivr/imap_mailbox
/usr/lib/sems/audio/mailbox/and.wav
/usr/lib/sems/audio/mailbox/beep.wav
/usr/lib/sems/audio/mailbox/bye.wav
/usr/lib/sems/audio/mailbox/default_en.wav
/usr/lib/sems/audio/mailbox/first_msg.wav
/usr/lib/sems/audio/mailbox/msg_deleted.wav
/usr/lib/sems/audio/mailbox/msg_menu.wav
/usr/lib/sems/audio/mailbox/msg_saved.wav
/usr/lib/sems/audio/mailbox/new_msg.wav
/usr/lib/sems/audio/mailbox/next_msg.wav
/usr/lib/sems/audio/mailbox/no_msg.wav
/usr/lib/sems/audio/mailbox/saved_msg.wav
/usr/lib/sems/audio/mailbox/you_have.wav
/usr/lib/sems/ivr/mailbox.pyc
/usr/lib/sems/ivr/mailbox_query.pyc
/usr/lib/sems/ivr/imap_mailbox/*.pyc

%files pin_collect
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/sems/etc/pin_collect.conf
%doc doc/Readme.pin_collect.txt
%dir /usr/lib/sems/audio/pincollect
/usr/lib/sems/audio/pincollect/enter_pin.wav
/usr/lib/sems/audio/pincollect/welcome.wav
/usr/lib/sems/ivr/pin_collect.pyc

%files python
%defattr(-,root,root)
##%config(noreplace) %{_sysconfdir}/sems/etc/py_sems.conf
%doc doc/Readme.py_sems.txt
##/usr/lib/sems/plug-in/py_sems.so
##/usr/lib/sems/plug-in/py_sems_log.pyc

%files speex
%defattr(-,root,root)
/usr/lib/sems/plug-in/speex.so

%changelog
* Fri Mar 1 2013 Pavel Kasparek <pavel@iptel.org> 1.6.0-1
- Updating for v1.6.0, build attempt on CentOS 6

* Tue Dec 09 2008 Alfred E. Heggestad <aeh@db.org> 1.1.0-1
- Update for v1.1.0

* Fri Oct 17 2008 Peter Lemenkov <lemenkov@gmail.com> 1.0.0-0.7.svn1095
- Fixed installation of audio files

* Sun Sep 28 2008 Peter Lemenkov <lemenkov@gmail.com> 1.0.0-0.6.svn1095
- New svn rev. 1095
- Some rpmlint-related fixes

* Thu Aug 21 2008 Peter Lemenkov <lemenkov@gmail.com> 1.0.0-0.5.svn
- Dropped upstreamed sems--initscript_fix.diff
- Installation of some audiofiles was fixed upstream

* Tue Aug 19 2008 Peter Lemenkov <lemenkov@gmail.com> 1.0.0-0.4.svn
- Splitted ivr module
- Fixed some rpmlint errors

* Thu Aug 14 2008 Peter Lemenkov <lemenkov@gmail.com> 1.0.0-0.3.svn
- Conditional switch "with_ilbc"

* Thu Aug 14 2008 Peter Lemenkov <lemenkov@gmail.com> 1.0.0-0.2.svn
- Splitted some modules

* Wed Aug 13 2008 Peter Lemenkov <lemenkov@gmail.com> 1.0.0-0.1.svn
- Preliminary ver. 1.0.0 (from svn)

* Sun Jun 29 2008 Peter Lemenkov <lemenkov@gmail.com> 1.0.0-rc1
- GCC4.3 patches upstreamed
- Ver. 1.0.0-rc1

* Wed Mar 26 2008 Peter Lemenkov <lemenkov@gmail.com> 0.10.0
- Initial package for Fedora

* Wed Dec 13 2006 Peter Nixon <peter+rpmspam@suntel.com.tr>
- First version of the spec file for SUSE.

