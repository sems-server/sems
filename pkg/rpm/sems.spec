# defines
%global		build_timestamp %(date +"%Y%m%d%H%M")

Summary:	SIP Express Media Server, an extensible SIP media server
Name:		sems
Version:	1.7.0
Release:	1.%{build_timestamp}%{?dist}
URL:		https://github.com/sems-server/%{name}
Source0:	https://github.com/sems-server/%{name}/archive/%{version}/%{name}-%{version}.tar.gz

License:	GPLv2+
BuildRequires:	bcg729-devel
BuildRequires:	cmake3
BuildRequires:	codec2-devel
BuildRequires:	flite-devel
BuildRequires:	gcc-c++
BuildRequires:	gsm-devel
BuildRequires:	hiredis-devel
BuildRequires:	ilbc-devel
BuildRequires:	lame-devel
BuildRequires:	lame-devel
BuildRequires:	libevent-devel
BuildRequires:	libmpg123-devel
BuildRequires:	libsamplerate-devel
BuildRequires:	mISDN-devel
BuildRequires:	mysql++-devel
BuildRequires:	openssl-devel
BuildRequires:	opus-devel
BuildRequires:	python-devel
#BuildRequires:	python2-devel
BuildRequires:	sip-devel
BuildRequires:	spandsp-devel
BuildRequires:	speex-devel
BuildRequires:	systemd

Requires(pre):  /usr/sbin/useradd
Requires(post): systemd-units
Requires(preun): systemd-units
Requires(postun): systemd-units

%description
SEMS (SIP Express Media Server) is very extensible and programmable
SIP media server for SER or OpenSER. The plug-in based SDK enables
you to extend SEMS and write your own applications and integrate new
codec. Voice-mail, announcement and echo plug-ins are already included.
SEMS supports g711u, g711a, GSM06.10 and wav file.

#%package	conf_auth
#Summary:	Conference with authorization
#Requires:	%{name}%{?_isa} = %{version}-%{release}
#Requires:	%{name}-ivr%{?_isa} = %{version}-%{release}

#%description	conf_auth
#Conference with authorization by PIN-numbers.

%package	conference
Summary:	Conferencing application
Requires:	%{name}%{?_isa} = %{version}-%{release}
Obsoletes:	%{name} < 1.2.0

%description	conference
Conferencing application for SEMS.

%package	diameter_client
Summary:	A simple DIAMETER client implementation
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	diameter_client
This is a very simple DIAMETER client implementation. it does
implement only parts of the base protocol, and is not a complete
DIAMETER implementation.

It is used from other modules with the DI API - i.e. other modules
can execute DI functions to add a server connection, or send a
DIAMETER request.

%package	dsm
Summary:	The state machine interpreter for SEMS
Requires:	%{name}%{?_isa} = %{version}-%{release}
Obsoletes:	%{name} < 1.2.0

%description	dsm
DonkeySM is a state machine interpreter for SEMS. Application
or service logic can comfortably and accurately be defined
as state machine, in a simple textual state machine definition
language, and executed by the dsm module as application in SEMS.

%package	early_announce
Summary:	Early announce application
Requires:	%{name}%{?_isa} = %{version}-%{release}
Obsoletes:	%{name} < 1.2.0

%description	early_announce
Early annonce application for SEMS.

%package	g722
Summary:	G.722 support for SEMS
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	g722
This is a wrapper around the g722 codec from the spandsp library.


%package	g729
Summary:	G.729 support for SEMS
BuildRequires:	bcg729-devel
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	g729
This is a wrapper around the g729 codec from the bcg729 library.

#%package	gateway
#Summary:	ISDN gateway for SEMS
#Requires:	%{name}%{?_isa} = %{version}-%{release}

#%description	gateway
#ISDN gateway for SEMS.

%package	gsm
Summary:	GSM support for SEMS
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	gsm
GSM support for SEMS.

%package	ilbc
Summary:	iLBC support for SEMS
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	ilbc
iLBC support for SEMS.

#%package	ivr
#Summary:	IVR functionality for SEMS
#Requires:	python2 >= 2.3
#Requires:	%{name}%{?_isa} = %{version}-%{release}

#%description	ivr
#IVR functionality for SEMS.

#%package	mailbox
#Summary:	Mailbox application
#Requires:	%{name}%{?_isa} = %{version}-%{release}
#Requires:	%{name}-ivr%{?_isa} = %{version}-%{release}

#%description	mailbox
#The mailbox application is a mailbox where callers can leave messages
#for offline or unavailable users and the users can dial in to check their
#messages. It uses an IMAP server as back-end to store the voice messages.

%package	mp3
Summary:	mp3 support for SEMS
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	mp3
mp3 support for SEMS.

%package	opus
Summary:	Opus support for SEMS
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	opus
Opus support for SEMS.

#%package	pin_collect
#Summary:	Collects a PIN
#Requires:	%{name}%{?_isa} = %{version}-%{release}
#Requires:	%{name}-ivr%{?_isa} = %{version}-%{release}

#%description	pin_collect
#This application collects a PIN and then transfers using a
#(proprietary) REFER the call.

%package	python
Summary:	Python bindings for SEMS
#BuildRequires:	python2 >= 2.3
#BuildRequires:	python2-sip-devel
%{?_sip_api:Requires: sip-api(%{_sip_api_major}) >= %{_sip_api}}
#Requires:	python2 >= 2.3
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	python
Python bindings for SEMS.

%if 0%{?_with_rtmp}
%package	rtmp
Summary:	RTMP support for SEMS
BuildRequires:	librtmp-devel
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	rtmp
RTMP support for SEMS.
%endif

%package	speex
Summary:	Speex support for SEMS
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	speex
Speex support for SEMS.

%package	xmlrpc2di
Summary:	XMLRPC interface for SEMS
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description	xmlrpc2di
This module makes the Dynamic Invocation (DI) Interfaces exported
by component modules accessible from XMLRPC. Additionally the built-in
methods calls, get_loglevel and set_loglevel are implemented (like in the
stats UDP server). Additionally, it can be used as client to access
XMLRPC servers.

%prep
%autosetup -p1
mv ./apps/dsm/fsmc/readme.txt  ./apps/dsm/fsmc/Readme.fsmc.txt

%build
mkdir cmake_build && cd cmake_build
%{cmake3} .. -DCMAKE_C_FLAGS_RELEASE:STRING=-DNDEBUG \
	-DCMAKE_CXX_FLAGS_RELEASE:STRING=-DNDEBUG \
	-DCMAKE_Fortran_FLAGS_RELEASE:STRING=-DNDEBUG \
	-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
	-DCMAKE_INSTALL_PREFIX:PATH=/usr \
	-DINCLUDE_INSTALL_DIR:PATH=/usr/include \
	-DLIB_INSTALL_DIR:PATH=/usr/lib64 \
	-DSYSCONF_INSTALL_DIR:PATH=/etc \
	-DSHARE_INSTALL_PREFIX:PATH=/usr/share \
	-DLIB_SUFFIX=64 \
	-DBUILD_SHARED_LIBS:BOOL=ON \
	-DSEMS_USE_SPANDSP=yes \
	-DSEMS_USE_LIBSAMPLERATE=yes \
	-DSEMS_USE_ZRTP=NO \
	-DSEMS_USE_MP3=yes \
	-DSEMS_USE_ILBC=yes \
	-DSEMS_USE_G729=yes \
	-DSEMS_USE_OPUS=yes \
	-DSEMS_USE_TTS=yes \
	-DSEMS_USE_OPENSSL=yes \
	-DSEMS_USE_MONITORING=yes \
	-DSEMS_USE_IPV6=yes \
	-DSEMS_CFG_PREFIX= \
	-DSEMS_AUDIO_PREFIX=/usr/share \
	-DSEMS_EXEC_PREFIX=/usr \
	-DSEMS_LIBDIR=lib64 \
	-DSEMS_DOC_PREFIX=/usr/share/doc

make %{?_smp_mflags}
cd ..

%install
cd cmake_build
make install DESTDIR=%{buildroot}
cd ..

install -D -m 0644 -p pkg/rpm/sems.sysconfig %{buildroot}%{_sysconfdir}/sysconfig/%{name}

# install systemd files
install -D -m 0644 -p pkg/rpm/sems.systemd.service %{buildroot}%{_unitdir}/%{name}.service
install -D -m 0644 -p pkg/rpm/sems.systemd.tmpfiles.d.conf %{buildroot}%{_tmpfilesdir}/%{name}.conf

mkdir -p %{buildroot}%{_localstatedir}/run/%{name}
mkdir -p %{buildroot}%{_localstatedir}/spool/%{name}/voicebox

# Remove installed docs
rm -rf %{buildroot}%{_docdir}/%{name}
rm -rf %{buildroot}%{_sysconfdir}/%{name}/default.template.sample
rm -rf %{buildroot}%{_sysconfdir}/%{name}/sems.conf.default

# remove currently empty conf-file
rm -f %{buildroot}%{_sysconfdir}/%{name}/etc/conf_auth.conf

# add empty directories for audiofiles
mkdir -p %{buildroot}%{_datadir}/%{name}/audio/ann_b2b
mkdir -p %{buildroot}%{_datadir}/%{name}/audio/announcement
mkdir -p %{buildroot}%{_datadir}/%{name}/audio/announce_transfer

ls -laR %{buildroot}

%pre
getent passwd %{name} >/dev/null || \
/usr/sbin/useradd -r -c "SIP Media Server"  -d %{_localstatedir}/spool/%{name} -s /sbin/nologin %{name} 2>/dev/null || :


%post
%systemd_post %{name}.service


%preun
%systemd_preun %{name}.service


%files
%dir %{_sysconfdir}/%{name}/
%dir %{_sysconfdir}/%{name}/etc/
%dir %{_libdir}/%{name}/
%dir %{_datadir}/%{name}/
%dir %{_datadir}/%{name}/audio/
%dir %{_datadir}/%{name}/audio/ann_b2b/
%dir %{_datadir}/%{name}/audio/announcement/
%dir %{_datadir}/%{name}/audio/announce_transfer/
%dir %{_datadir}/%{name}/audio/annrecorder/
%dir %{_datadir}/%{name}/audio/precoded_announce/
%dir %{_datadir}/%{name}/audio/voicebox/
%dir %{_datadir}/%{name}/audio/voicemail/
%dir %{_datadir}/%{name}/audio/webconference/
%dir %{_libdir}/%{name}/plug-in/
%dir %attr(0755, %{name}, %{name}) %{_localstatedir}/spool/%{name}/
%dir %attr(0750, %{name}, %{name}) %{_localstatedir}/spool/%{name}/voicebox/

%config(noreplace) %{_sysconfdir}/sysconfig/%{name}

%{_unitdir}/%{name}.service
%{_tmpfilesdir}/%{name}.conf

%ghost %dir %attr(0755, %{name}, %{name}) %{_localstatedir}/run/%{name}/

%config(noreplace) %{_sysconfdir}/%{name}/default.template
%config(noreplace) %{_sysconfdir}/%{name}/%{name}.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/ann_b2b.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/announce_transfer.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/announcement.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/annrecorder.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/app_mapping.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/callback.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/click2dial.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/db_reg_agent.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/echo.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/monitoring.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/msg_storage.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/mwi.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/precoded_announce.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/reg_agent.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/stats.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/voicebox.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/voicemail.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/webconference.conf

%config(noreplace) %{_sysconfdir}/%{name}/etc/auth_b2b.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/call_timer.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/cc_call_timer.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/cc_pcalls.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/cc_syslog_cdr.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/codecfilter.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/prepaid.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/refuse.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/replytranslate.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/sbc.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/src_ipmap.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/sst_b2b.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/symmetricrtp.sbcprofile.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/transparent.sbcprofile.conf

%doc README
%doc core/plug-in/adpcm/README_G711
%doc doc/figures
%doc doc/Howtostart_noproxy.txt
%doc doc/Howtostart_simpleproxy.txt
%doc doc/Howtostart_voicemail.txt
%doc doc/CHANGELOG
%doc doc/COPYING
%doc doc/README.stats
%doc doc/Readme.ann_b2b.txt
%doc doc/Readme.announce_transfer.txt
%doc doc/Readme.announcement.txt
%doc doc/Readme.annrecorder.txt
%doc doc/Readme.auth_b2b.txt
%doc doc/Readme.call_timer.txt
%doc doc/Readme.callback.txt
%doc doc/Readme.click2dial.txt
%doc doc/Readme.conf_auth.txt
%doc doc/Readme.echo.txt
%doc doc/Readme.monitoring.txt
%doc doc/Readme.msg_storage.txt
#%doc doc/Readme.py_sems.txt
%doc doc/Readme.reg_agent.txt
%doc doc/Readme.registrar_client.txt
%doc doc/Readme.sst_b2b.txt
%doc doc/Readme.sw_prepaid_sip.txt
#%doc doc/Readme.twit.txt
%doc doc/Readme.uac_auth.txt
%doc doc/Readme.voicebox.txt
%doc doc/Readme.voicemail.txt
%doc doc/Readme.webconference.txt
%doc doc/Tuning.txt
#%doc doc/ZRTP.txt

%{_sbindir}/%{name}
%{_sbindir}/%{name}-get-callproperties
%{_sbindir}/%{name}-list-active-calls
%{_sbindir}/%{name}-list-calls
%{_sbindir}/%{name}-list-finished-calls
%{_sbindir}/%{name}-logfile-callextract
%{_sbindir}/%{name}-rtp-mux-get-max-frame-age-ms
%{_sbindir}/%{name}-rtp-mux-get-mtu-threshold
%{_sbindir}/%{name}-rtp-mux-set-max-frame-age-ms
%{_sbindir}/%{name}-rtp-mux-set-mtu-threshold
%{_sbindir}/%{name}-sbc-get-activeprofile
%{_sbindir}/%{name}-sbc-get-regex-map-names
%{_sbindir}/%{name}-sbc-list-profiles
%{_sbindir}/%{name}-sbc-load-callcontrol-modules
%{_sbindir}/%{name}-sbc-load-profile
%{_sbindir}/%{name}-sbc-reload-profile
%{_sbindir}/%{name}-sbc-reload-profiles
%{_sbindir}/%{name}-sbc-set-activeprofile
%{_sbindir}/%{name}-sbc-set-regex-map
%{_sbindir}/%{name}-sbc-teardown-call
%{_sbindir}/%{name}-stats

%{_datadir}/%{name}/audio/beep.wav
%{_datadir}/%{name}/audio/default_en.wav
%{_datadir}/%{name}/audio/annrecorder/beep.wav
%{_datadir}/%{name}/audio/annrecorder/bye.wav
%{_datadir}/%{name}/audio/annrecorder/confirm.wav
%{_datadir}/%{name}/audio/annrecorder/greeting_set.wav
%{_datadir}/%{name}/audio/annrecorder/to_record.wav
%{_datadir}/%{name}/audio/annrecorder/welcome.wav
%{_datadir}/%{name}/audio/annrecorder/your_prompt.wav
%{_datadir}/%{name}/audio/precoded_announce/test.predef
%{_datadir}/%{name}/audio/voicebox/0.wav
%{_datadir}/%{name}/audio/voicebox/1.wav
%{_datadir}/%{name}/audio/voicebox/10.wav
%{_datadir}/%{name}/audio/voicebox/11.wav
%{_datadir}/%{name}/audio/voicebox/12.wav
%{_datadir}/%{name}/audio/voicebox/13.wav
%{_datadir}/%{name}/audio/voicebox/14.wav
%{_datadir}/%{name}/audio/voicebox/15.wav
%{_datadir}/%{name}/audio/voicebox/16.wav
%{_datadir}/%{name}/audio/voicebox/17.wav
%{_datadir}/%{name}/audio/voicebox/18.wav
%{_datadir}/%{name}/audio/voicebox/19.wav
%{_datadir}/%{name}/audio/voicebox/2.wav
%{_datadir}/%{name}/audio/voicebox/20.wav
%{_datadir}/%{name}/audio/voicebox/3.wav
%{_datadir}/%{name}/audio/voicebox/30.wav
%{_datadir}/%{name}/audio/voicebox/4.wav
%{_datadir}/%{name}/audio/voicebox/40.wav
%{_datadir}/%{name}/audio/voicebox/5.wav
%{_datadir}/%{name}/audio/voicebox/50.wav
%{_datadir}/%{name}/audio/voicebox/6.wav
%{_datadir}/%{name}/audio/voicebox/60.wav
%{_datadir}/%{name}/audio/voicebox/7.wav
%{_datadir}/%{name}/audio/voicebox/70.wav
%{_datadir}/%{name}/audio/voicebox/8.wav
%{_datadir}/%{name}/audio/voicebox/80.wav
%{_datadir}/%{name}/audio/voicebox/9.wav
%{_datadir}/%{name}/audio/voicebox/90.wav
%{_datadir}/%{name}/audio/voicebox/and.wav
%{_datadir}/%{name}/audio/voicebox/bye.wav
%{_datadir}/%{name}/audio/voicebox/first_new_msg.wav
%{_datadir}/%{name}/audio/voicebox/first_saved_msg.wav
%{_datadir}/%{name}/audio/voicebox/in_your_voicebox.wav
%{_datadir}/%{name}/audio/voicebox/msg_deleted.wav
%{_datadir}/%{name}/audio/voicebox/msg_end_menu.wav
%{_datadir}/%{name}/audio/voicebox/msg_menu.wav
%{_datadir}/%{name}/audio/voicebox/msg_saved.wav
%{_datadir}/%{name}/audio/voicebox/new_msg.wav
%{_datadir}/%{name}/audio/voicebox/new_msgs.wav
%{_datadir}/%{name}/audio/voicebox/next_new_msg.wav
%{_datadir}/%{name}/audio/voicebox/next_saved_msg.wav
%{_datadir}/%{name}/audio/voicebox/no_more_msg.wav
%{_datadir}/%{name}/audio/voicebox/no_msg.wav
%{_datadir}/%{name}/audio/voicebox/pin_prompt.wav
%{_datadir}/%{name}/audio/voicebox/saved_msg.wav
%{_datadir}/%{name}/audio/voicebox/saved_msgs.wav
%{_datadir}/%{name}/audio/voicebox/x1.wav
%{_datadir}/%{name}/audio/voicebox/x2.wav
%{_datadir}/%{name}/audio/voicebox/x3.wav
%{_datadir}/%{name}/audio/voicebox/x4.wav
%{_datadir}/%{name}/audio/voicebox/x5.wav
%{_datadir}/%{name}/audio/voicebox/x6.wav
%{_datadir}/%{name}/audio/voicebox/x7.wav
%{_datadir}/%{name}/audio/voicebox/x8.wav
%{_datadir}/%{name}/audio/voicebox/x9.wav
%{_datadir}/%{name}/audio/voicebox/you_have.wav
%{_datadir}/%{name}/audio/voicemail/default_en.wav
%{_datadir}/%{name}/audio/voicemail/beep.wav
%{_datadir}/%{name}/audio/webconference/0.wav
%{_datadir}/%{name}/audio/webconference/1.wav
%{_datadir}/%{name}/audio/webconference/2.wav
%{_datadir}/%{name}/audio/webconference/3.wav
%{_datadir}/%{name}/audio/webconference/4.wav
%{_datadir}/%{name}/audio/webconference/5.wav
%{_datadir}/%{name}/audio/webconference/6.wav
%{_datadir}/%{name}/audio/webconference/7.wav
%{_datadir}/%{name}/audio/webconference/8.wav
%{_datadir}/%{name}/audio/webconference/9.wav
%{_datadir}/%{name}/audio/webconference/beep.wav
%{_datadir}/%{name}/audio/webconference/entering_conference.wav
%{_datadir}/%{name}/audio/webconference/first_participant.wav
%{_datadir}/%{name}/audio/webconference/pin_prompt.wav
%{_datadir}/%{name}/audio/webconference/wrong_pin.wav

%{_libdir}/%{name}/plug-in/adpcm.so
%{_libdir}/%{name}/plug-in/ann_b2b.so
%{_libdir}/%{name}/plug-in/announce_transfer.so
%{_libdir}/%{name}/plug-in/announcement.so
%{_libdir}/%{name}/plug-in/annrecorder.so
%{_libdir}/%{name}/plug-in/callback.so
%{_libdir}/%{name}/plug-in/cc_bl_redis.so
%{_libdir}/%{name}/plug-in/cc_call_timer.so
%{_libdir}/%{name}/plug-in/cc_ctl.so
%{_libdir}/%{name}/plug-in/cc_dsm.so
%{_libdir}/%{name}/plug-in/cc_pcalls.so
%{_libdir}/%{name}/plug-in/cc_prepaid.so
%{_libdir}/%{name}/plug-in/cc_registrar.so
%{_libdir}/%{name}/plug-in/cc_syslog_cdr.so
%{_libdir}/%{name}/plug-in/click2dial.so
%{_libdir}/%{name}/plug-in/db_reg_agent.so
%{_libdir}/%{name}/plug-in/echo.so
%{_libdir}/%{name}/plug-in/isac.so
%{_libdir}/%{name}/plug-in/l16.so
%{_libdir}/%{name}/plug-in/monitoring.so
%{_libdir}/%{name}/plug-in/msg_storage.so
%{_libdir}/%{name}/plug-in/mwi.so
%{_libdir}/%{name}/plug-in/precoded_announce.so
%{_libdir}/%{name}/plug-in/reg_agent.so
%{_libdir}/%{name}/plug-in/registrar_client.so
%{_libdir}/%{name}/plug-in/sbc.so
%{_libdir}/%{name}/plug-in/session_timer.so
%{_libdir}/%{name}/plug-in/stats.so
%{_libdir}/%{name}/plug-in/uac_auth.so
%{_libdir}/%{name}/plug-in/voicebox.so
%{_libdir}/%{name}/plug-in/voicemail.so
%{_libdir}/%{name}/plug-in/wav.so
%{_libdir}/%{name}/plug-in/webconference.so

#%files conf_auth
# currently empty
#%config(noreplace) %{_sysconfdir}/%{name}/etc/conf_auth.conf
#%doc doc/Readme.conf_auth.txt
#%{_libdir}/%{name}/ivr/conf_auth.py*

%files conference
%config(noreplace) %{_sysconfdir}/%{name}/etc/conference.conf
%doc doc/Readme.conference.txt
%dir %{_datadir}/%{name}/audio/conference/
%{_libdir}/%{name}/plug-in/conference.so
%{_datadir}/%{name}/audio/conference/beep.wav
%{_datadir}/%{name}/audio/conference/first_participant.wav

%files diameter_client
%doc doc/Readme.diameter_client.txt
%{_libdir}/%{name}/plug-in/diameter_client.so

%files dsm
%config(noreplace) %{_sysconfdir}/%{name}/etc/dsm.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/dsm_in_prompts.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/dsm_out_prompts.conf
%config(noreplace) %{_sysconfdir}/%{name}/etc/mod_regex.conf
%doc doc/dsm
%dir %{_libdir}/%{name}/dsm/
%{_libdir}/%{name}/dsm/mod_conference.so
%{_libdir}/%{name}/dsm/mod_dlg.so
%{_libdir}/%{name}/dsm/mod_groups.so
%{_libdir}/%{name}/dsm/mod_monitoring.so
#%{_libdir}/%{name}/dsm/mod_mysql.so
#%{_libdir}/%{name}/dsm/mod_py.so
%{_libdir}/%{name}/dsm/mod_redis.so
%{_libdir}/%{name}/dsm/mod_regex.so
%{_libdir}/%{name}/dsm/mod_sbc.so
%{_libdir}/%{name}/dsm/mod_subscription.so
%{_libdir}/%{name}/dsm/mod_sys.so
%{_libdir}/%{name}/dsm/mod_uri.so
%{_libdir}/%{name}/dsm/mod_utils.so
%{_libdir}/%{name}/plug-in/dsm.so
## DSM scripts
%{_libdir}/%{name}/dsm/early_dbprompt.dsm
%{_libdir}/%{name}/dsm/inbound_call.dsm
%{_libdir}/%{name}/dsm/mobile_push.dsm
%{_libdir}/%{name}/dsm/outbound_call.dsm


%files early_announce
%config(noreplace) %{_sysconfdir}/%{name}/etc/early_announce.conf
%doc doc/Readme.early_announce.txt
%{_libdir}/%{name}/plug-in/early_announce.so

%files g722
%doc core/plug-in/g722/Readme.g722codec
%{_libdir}/%{name}/plug-in/g722.so


%files g729
%doc core/plug-in/g729/Readme.g729.md
%{_libdir}/%{name}/plug-in/g729.so

#%files gateway
#%config(noreplace) %{_sysconfdir}/%{name}/etc/gateway.conf
#%{_libdir}/%{name}/plug-in/gateway.so

%files gsm
%{_libdir}/%{name}/plug-in/gsm.so

%files ilbc
%doc doc/Readme.iLBC.txt
%{_libdir}/%{name}/plug-in/ilbc.so

#%files ivr
#%config(noreplace) %{_sysconfdir}/%{name}/etc/ivr.conf
#%doc doc/Readme.ivr.txt
#%dir %{_libdir}/%{name}/ivr/
#%{_libdir}/%{name}/plug-in/ivr.so
#%{_libdir}/%{name}/ivr/log.py*

#%files mailbox
#%config(noreplace) %{_sysconfdir}/%{name}/etc/mailbox.conf
#%config(noreplace) %{_sysconfdir}/%{name}/etc/mailbox_query.conf
#%doc doc/Readme.mailbox.txt
#%dir %{_datadir}/%{name}/audio/mailbox/
#%dir %{_libdir}/%{name}/ivr/imap_mailbox/
#%{_datadir}/%{name}/audio/mailbox/and.wav
#%{_datadir}/%{name}/audio/mailbox/beep.wav
#%{_datadir}/%{name}/audio/mailbox/bye.wav
#%{_datadir}/%{name}/audio/mailbox/default_en.wav
#%{_datadir}/%{name}/audio/mailbox/first_msg.wav
#%{_datadir}/%{name}/audio/mailbox/msg_deleted.wav
#%{_datadir}/%{name}/audio/mailbox/msg_menu.wav
#%{_datadir}/%{name}/audio/mailbox/msg_saved.wav
#%{_datadir}/%{name}/audio/mailbox/new_msg.wav
#%{_datadir}/%{name}/audio/mailbox/next_msg.wav
#%{_datadir}/%{name}/audio/mailbox/no_msg.wav
#%{_datadir}/%{name}/audio/mailbox/saved_msg.wav
#%{_datadir}/%{name}/audio/mailbox/you_have.wav
#%{_libdir}/%{name}/ivr/mailbox.py*
#%{_libdir}/%{name}/ivr/mailbox_query.py*
#%{_libdir}/%{name}/ivr/imap_mailbox/MailboxURL.py*
#%{_libdir}/%{name}/ivr/imap_mailbox/__init__.py*
#%{_libdir}/%{name}/ivr/imap_mailbox/imap4ext.py*

%files mp3
%doc doc/Readme.mp3plugin.txt
%{_libdir}/%{name}/plug-in/mp3.so

%files opus
%{_libdir}/%{name}/plug-in/opus.so

#%files pin_collect
#%config(noreplace) %{_sysconfdir}/%{name}/etc/pin_collect.conf
#%doc doc/Readme.pin_collect.txt
#%dir %{_datadir}/%{name}/audio/pin_collect/
#%{_datadir}/%{name}/audio/pin_collect/enter_pin.wav
#%{_datadir}/%{name}/audio/pin_collect/welcome.wav
#%{_libdir}/%{name}/ivr/pin_collect.py*

#%files python
#%config(noreplace) %{_sysconfdir}/%{name}/etc/py_sems.conf
#%doc doc/Readme.py_sems.txt
#%{_libdir}/%{name}/plug-in/py_sems.so
#%{_libdir}/%{name}/plug-in/py_sems_log.py*

%if 0%{?_with_rtmp}
%files rtmp
%{_libdir}/%{name}/plug-in/rtmp.so
%endif

%files speex
%{_libdir}/%{name}/plug-in/speex.so

%files xmlrpc2di
%config(noreplace) %{_sysconfdir}/%{name}/etc/xmlrpc2di.conf
%doc doc/Readme.xmlrpc2di.txt
%{_libdir}/%{name}/plug-in/xmlrpc2di.so
