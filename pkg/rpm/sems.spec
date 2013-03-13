Summary:	SIP Express Media Server, an extensible SIP media server
Name:		sems
Version:	1.1.0
Release:	1
URL:		http://www.iptel.org/sems
# svn -r 1095 export http://svn.berlios.de/svnroot/repos/sems/branches/1.0.0 sems-1.0.0
# tar cjvf sems-1.0.0.tar.bz2 sems-1.0.0
Source:		%{name}-%{version}.tar.gz
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
make install TTS="y" exclude_modules="examples %{!?with_ilbc:ilbc} mp3" \
	DESTDIR=$RPM_BUILD_ROOT \
	basedir= \
	prefix=%{_prefix} \
	modules-prefix= \
        modules-dir=%{_libdir}/sems/plug-in \
        modules-target=%{_libdir}/sems/plug-in \
	ivr-modules-dir=%{_libdir}/sems/ivr \
	cfg-prefix= \
	cfg-target=%{_sysconfdir}/sems/ \
	doc-prefix= \
	doc-dir=%{_docdir}/sems/ \
	audio-prefix= \
	audio-dir=%{_libdir}/sems/audio/

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
%config(noreplace) %{_sysconfdir}/sems/default.template
%config(noreplace) %{_sysconfdir}/sems/sems.conf
%config(noreplace) %{_sysconfdir}/sems/etc/ann_b2b.conf
%config(noreplace) %{_sysconfdir}/sems/etc/announce_transfer.conf
%config(noreplace) %{_sysconfdir}/sems/etc/announcement.conf
%config(noreplace) %{_sysconfdir}/sems/etc/annrecorder.conf
%config(noreplace) %{_sysconfdir}/sems/etc/app_mapping.conf
%config(noreplace) %{_sysconfdir}/sems/etc/binrpcctrl.conf
%config(noreplace) %{_sysconfdir}/sems/etc/call_timer.conf
%config(noreplace) %{_sysconfdir}/sems/etc/callback.conf
%config(noreplace) %{_sysconfdir}/sems/etc/click2dial.conf
%config(noreplace) %{_sysconfdir}/sems/etc/conference.conf
%config(noreplace) %{_sysconfdir}/sems/etc/early_announce.conf
%config(noreplace) %{_sysconfdir}/sems/etc/dsm.conf
%config(noreplace) %{_sysconfdir}/sems/etc/dsm_in_prompts.conf
%config(noreplace) %{_sysconfdir}/sems/etc/dsm_out_prompts.conf
%config(noreplace) %{_sysconfdir}/sems/etc/gateway.conf
%config(noreplace) %{_sysconfdir}/sems/etc/ivr.conf
%config(noreplace) %{_sysconfdir}/sems/etc/msg_storage.conf
%config(noreplace) %{_sysconfdir}/sems/etc/precoded_announce.conf
%config(noreplace) %{_sysconfdir}/sems/etc/py_sems.conf
%config(noreplace) %{_sysconfdir}/sems/etc/reg_agent.conf
%config(noreplace) %{_sysconfdir}/sems/etc/sipctrl.conf
%config(noreplace) %{_sysconfdir}/sems/etc/stats.conf
%config(noreplace) %{_sysconfdir}/sems/etc/sw_prepaid_sip.conf
%config(noreplace) %{_sysconfdir}/sems/etc/unixsockctrl.conf
%config(noreplace) %{_sysconfdir}/sems/etc/voicebox.conf
%config(noreplace) %{_sysconfdir}/sems/etc/voicemail.conf
%config(noreplace) %{_sysconfdir}/sems/etc/webconference.conf
%config(noreplace) %{_sysconfdir}/sems/etc/xmlrpc2di.conf

%doc README
%doc apps/examples/tutorial/cc_acc/Readme.cc_acc
%doc doc/figures
%doc doc/CHANGELOG
%doc doc/COPYING
%doc doc/Configure-Sems-OpenSER-HOWTO
%doc doc/Configure-Sems-Ser-HOWTO
%doc doc/Readme.ann_b2b
%doc doc/Readme.announce_transfer
%doc doc/Readme.announcement
%doc doc/Readme.annrecorder
%doc doc/Readme.auth_b2b
%doc doc/Readme.call_timer
%doc doc/Readme.callback
%doc doc/Readme.click2dial
%doc doc/Readme.conf_auth
%doc doc/Readme.conference
%doc doc/Readme.diameter_client
%doc doc/Readme.early_announce
%doc doc/Readme.echo
%if 0%{?with_ilbc}
%doc doc/Readme.iLBC
%endif
#%doc doc/Readme.mp3plugin
%doc doc/Readme.msg_storage
%doc doc/Readme.reg_agent
%doc doc/Readme.registrar_client
%doc doc/Readme.sw_prepaid_sip
%doc doc/Readme.uac_auth
%doc doc/Readme.voicebox
%doc doc/Readme.voicemail
%doc doc/Readme.webconference
%doc doc/WHATSNEW

%{_sysconfdir}/init.d/sems

%{_sbindir}/sems
%{_sbindir}/sems-stats

%dir %{_libdir}/sems
%dir %{_libdir}/sems/audio
%dir %{_libdir}/sems/audio/ann_b2b
%dir %{_libdir}/sems/audio/announcement
%dir %{_libdir}/sems/audio/announce_transfer
%dir %{_libdir}/sems/audio/annrecorder
%dir %{_libdir}/sems/audio/conference
%dir %{_libdir}/sems/audio/voicebox
%dir %{_libdir}/sems/audio/voicemail
%dir %{_libdir}/sems/audio/webconference
%dir %{_libdir}/sems/plug-in


%{_libdir}/sems/audio/beep.wav
%{_libdir}/sems/audio/default_en.wav
%{_libdir}/sems/audio/annrecorder/beep.wav
%{_libdir}/sems/audio/annrecorder/bye.wav
%{_libdir}/sems/audio/annrecorder/confirm.wav
%{_libdir}/sems/audio/annrecorder/greeting_set.wav
%{_libdir}/sems/audio/annrecorder/to_record.wav
%{_libdir}/sems/audio/annrecorder/welcome.wav
%{_libdir}/sems/audio/annrecorder/your_prompt.wav
%{_libdir}/sems/audio/conference/beep.wav
%{_libdir}/sems/audio/conference/first_participant.wav
%{_libdir}/sems/audio/voicebox/*.wav
%{_libdir}/sems/audio/voicemail/default_en.wav
%{_libdir}/sems/audio/voicemail/beep.wav
%{_libdir}/sems/audio/webconference/*.wav

%{_libdir}/sems/plug-in/adpcm.so
%{_libdir}/sems/plug-in/ann_b2b.so
%{_libdir}/sems/plug-in/announce_transfer.so
%{_libdir}/sems/plug-in/announcement.so
%{_libdir}/sems/plug-in/annrecorder.so
%{_libdir}/sems/plug-in/auth_b2b.so
%{_libdir}/sems/plug-in/binrpcctrl.so
%{_libdir}/sems/plug-in/call_timer.so
%{_libdir}/sems/plug-in/callback.so
%{_libdir}/sems/plug-in/cc_acc.so
%{_libdir}/sems/plug-in/click2dial.so
%{_libdir}/sems/plug-in/conference.so
%{_libdir}/sems/plug-in/diameter_client.so
%{_libdir}/sems/plug-in/dsm.so
%{_libdir}/sems/plug-in/early_announce.so
%{_libdir}/sems/plug-in/echo.so
%if 0%{?with_ilbc}
%{_libdir}/sems/plug-in/ilbc.so
%endif
%{_libdir}/sems/plug-in/l16.so
%{_libdir}/sems/plug-in/msg_storage.so
%{_libdir}/sems/plug-in/precoded_announce.so
%{_libdir}/sems/plug-in/reg_agent.so
%{_libdir}/sems/plug-in/registrar_client.so
%{_libdir}/sems/plug-in/sipctrl.so
%{_libdir}/sems/plug-in/session_timer.so
%{_libdir}/sems/plug-in/stats.so
%{_libdir}/sems/plug-in/sw_prepaid_sip.so
%{_libdir}/sems/plug-in/uac_auth.so
%{_libdir}/sems/plug-in/unixsockctrl.so
%{_libdir}/sems/plug-in/voicebox.so
%{_libdir}/sems/plug-in/voicemail.so
%{_libdir}/sems/plug-in/wav.so
%{_libdir}/sems/plug-in/webconference.so

%{_libdir}/sems/dsm/mod_dlg.so
%{_libdir}/sems/dsm/mod_sys.so
%{_libdir}/sems/dsm/mod_uri.so
%{_libdir}/sems/dsm/inbound_call.dsm
%{_libdir}/sems/dsm/outbound_call.dsm

%files conf_auth
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/sems/etc/conf_auth.conf
%doc doc/Readme.conf_auth
%{_libdir}/sems/ivr/conf_auth.pyc

%files gsm
%defattr(-,root,root)
%{_libdir}/sems/plug-in/gsm.so

%files ivr
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/sems/etc/ivr.conf
%doc doc/Readme.ivr
%dir %{_libdir}/sems/ivr
%{_libdir}/sems/plug-in/ivr.so
%{_libdir}/sems/plug-in/log.pyc

%files mailbox
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/sems/etc/mailbox.conf
%config(noreplace) %{_sysconfdir}/sems/etc/mailbox_query.conf
%doc doc/Readme.mailbox
%dir %{_libdir}/sems/audio/mailbox
%dir %{_libdir}/sems/ivr/imap_mailbox
%{_libdir}/sems/audio/mailbox/and.wav
%{_libdir}/sems/audio/mailbox/beep.wav
%{_libdir}/sems/audio/mailbox/bye.wav
%{_libdir}/sems/audio/mailbox/default_en.wav
%{_libdir}/sems/audio/mailbox/first_msg.wav
%{_libdir}/sems/audio/mailbox/msg_deleted.wav
%{_libdir}/sems/audio/mailbox/msg_menu.wav
%{_libdir}/sems/audio/mailbox/msg_saved.wav
%{_libdir}/sems/audio/mailbox/new_msg.wav
%{_libdir}/sems/audio/mailbox/next_msg.wav
%{_libdir}/sems/audio/mailbox/no_msg.wav
%{_libdir}/sems/audio/mailbox/saved_msg.wav
%{_libdir}/sems/audio/mailbox/you_have.wav
%{_libdir}/sems/ivr/mailbox.pyc
%{_libdir}/sems/ivr/mailbox_query.pyc
%{_libdir}/sems/ivr/imap_mailbox/*.pyc

%files pin_collect
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/sems/etc/pin_collect.conf
%doc doc/Readme.pin_collect
%dir %{_libdir}/sems/audio/pincollect
%{_libdir}/sems/audio/pincollect/enter_pin.wav
%{_libdir}/sems/audio/pincollect/welcome.wav
%{_libdir}/sems/ivr/pin_collect.pyc

%files python
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/sems/etc/py_sems.conf
%doc doc/Readme.py_sems
%{_libdir}/sems/plug-in/py_sems.so
%{_libdir}/sems/plug-in/py_sems_log.pyc

%files speex
%defattr(-,root,root)
%{_libdir}/sems/plug-in/speex.so

%changelog
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

