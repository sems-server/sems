# SIP Express Media Server (SEMS)

[![Build distribution packages](https://github.com/sems-server/sems/actions/workflows/build_test.yml/badge.svg)](https://github.com/sems-server/sems/actions/workflows/build_test.yml)

[![Copr build status](https://copr.fedorainfracloud.org/coprs/marcel-hecko/sems/package/sems/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/marcel-hecko/sems/package/sems/)

## Introduction

SEMS is a free, high performance, extensible media server for SIP (RFC3261) based VoIP services.

It is intended to complement proxy/registrar servers
in VoIP networks for all applications where server-side processing of audio is required, for example away or pre-call announcements, voicemail, or network side 
conferencing. Another use case is for interconnecting
SIP networks, where a back-to-back user agent (B2BUA) is required.

SEMS can be used to implement simple high performance 
components like announcement servers as building 
blocks of more complex applications, or, using its powerful 
framework for application development including back-to-back 
user agent (B2BUA) and state machine scripting functionality, 
complex VoIP services can be realized completely in SEMS. 

SEMS supports all important patent free codecs out of the
box (g711u, g711a, GSM06.10, speex, G.726, L16, OPUS, iLBC etc).
There is a wrapper for the IPP G.729 codec implementation
available. Integrating other codecs in SEMS is very simple
(patented or not).

SEMS shows very good performance on current standard
PC architecture based server systems. It has sucessfully
been run with 1200 G.711 conference channels on a quad-core
Intel(R) Xeon at 2GHz (700 GSM, 280 iLBC channels), and up to 
5000 channels on a dual quad Xeon at 2.9GHz. Its back-to-back
user agent has been run with up to 19000 TPS on the latter
machine. On the other hand it also runs on very small devices -
for example small embedded systems like routers running OpenWRT,
for which of course the achievable channel count is not that
high.

## License

SEMS is free (speech+beer) software. It is licensed under dual 
license terms, the GPL (v2+) and proprietary license. This
program is released under the GPL with the additional exemption
that compiling, linking, and/or using OpenSSL is allowed.

For a license to use SEMS under non-GPL terms, please contact
FRAFOS GmbH at info@frafos.com .

See doc/COPYING for details.

## Applications

The following applications are shipped with SEMS :

### Back-to-back User Agent

* **sbc** - flexible SBC (Session Border Controller) application, supports
  - header manipulation and filtering
  - (multihomed) RTP relay, NAT handling
  - audio transcoding with codec mapping
  - SIP authentication (UAC, A/B leg)
  - session timer (SST) for both legs
  - contact hiding and identity change
  - SIP reply code/reason translation
  - DTMF filtering and detection
  - RTP bandwidth limiting
  - registration caching and retargeting
  - call_control modules:
    - **cc_bl_redis** - Redis-based call blacklisting
    - **cc_call_timer** - call duration limit enforcement
    - **cc_ctl** - profile control via SIP headers
    - **cc_dsm** - roles for DSM state machine scripting
    - **cc_parallel_calls** - concurrent call limiting per user
    - **cc_prepaid** - local credit-based call control
    - **cc_prepaid_xmlrpc** - prepaid via external XMLRPC server
    - **cc_registrar** - REGISTER caching and routing
    - **cc_rest** - call control via REST API
    - **cc_siprec** - SIPREC (RFC 7865/7866) call recording
    - **cc_syslog_cdr** - call detail records via syslog

### Announcements (Prompts, Ringbacktones, Pre-call-prompts):

* **announcement** - plays an announcement

* **ann_b2b** - pre-call-announcement, plays announcement 
  before connecting the callee in B2BUA mode

* **announce_transfer** - pre-call-announcement, plays announcement 
  and then transfers the caller to the callee 
  using REFER

* **early_announce** - (pre-call) announcement using early media (183),
  optionally continues the call in B2BUA mode

* **precoded_announce** - plays preencoded announcements

### Voicemail/Mailbox

* **voicebox** - users can dial in to the voicebox to check
  their messages

* **annrecorder** - users can record their personal greeting 
  message

* **mailbox** - auto-attendant that saves voicemails into
  an IMAP server. Users can dial in to check
  their messages (simpler version)

* **voicemail** - records voice messages and sends them 
  as email, saves them to a voicebox, or
  both

### Conferencing

* **conference** - enables many people to talk together 
  at the same time

* **webconference** - conference application that can be 
  controlled from an external program, 
  e.g. a website 

* **conf_auth** - collect a PIN number, verify it against an 
  XMLRPC authentication server and connects in 
  B2BUA mode

* **pin_collect** - collect a PIN, optionally verify it, and transfer
  the call into a conference

### App development

* **dsm** - DSM state machine scripting (use this)

* **ivr** - embedded Python interpreter for simple apps

* **py_sems** - another embedded Python interpreter

### Call Control and Routing

* **click2dial** - XMLRPC-enabled way to initiate authenticated calls

* **callback** - reject the call, call back caller later and have 
  caller enter a number to call in b2bua with media relay
  mode

* **mobile_push** - mobile push notification support for call routing

### Registration and Authentication

* **reg_agent** - SIP REGISTER to register SEMS' contact to an aor

* **registrar_client** - SIP registrar client functionality

* **db_reg_agent** - database-based registration agent using MySQL++

### Protocol Support and Integration

* **diameter_client** - simple DIAMETER client implementation for AAA integration

* **jsonrpc** - JSON-RPC protocol version 2.0 over TCP/Netstrings

* **xmlrpc2di** - makes DI interfaces accessible via XMLRPC, also XMLRPC client

### Monitoring and Management

* **monitoring** - in-memory AVP DB for call monitoring and generic data storage

* **msg_storage** - message storage functionality

* **mwi** - Message Waiting Indication (MWI) support

### Recording

* **siprec_srs** - minimal SIPREC Session Recording Server (RFC 7865/7866).
  Receives recording INVITEs from SIPREC clients (such as cc_siprec),
  records both call legs to separate WAV files, and saves the SIPREC
  metadata XML. Can run on the same SEMS instance as the SBC for
  self-contained call recording. See also **cc_siprec** under the SBC
  call_control modules.

### Media Processing

* **mp3** - MP3 codec support and media processing

* **rtmp** - RTMP (Real-Time Messaging Protocol) support

### Example and Test Applications

* **echo** - test module to echo the caller's voice

* **early_dbprompt** - early media database prompts example

* **gateway** - ISDN gateway functionality (requires mISDN)

* **twit** - example Twitter integration application

## Developing and customizing Applications and services

SEMS comes with a set of example applications intended to help
development of custom services, including a calling card
application, a traffic generator, a component to control the
media server via XMLRPC, and announcements played from DB.

DSM state machine scripting is a powerful yet simple method
to rapidly implement custom applications. With this method,
the service logic is written as an easy to understand
textual definition of a state machine, which is interpreted
and executed for every call. The (domain specific) language
for defining state machines can be extended by implementing
modules. A set of useful modules are shipped with SEMS,
including MySQL database access module, Python module,
conference support, Amazon AWS and more.

SEMS' core implements basic call and audio processing,
and loads plug-ins which extend the system. Audio
plug-ins enable new codecs and file formats,
application plug-ins implement the services' logic.
Other modules called component modules provide
functionality for other modules to use.

You can easily extend SEMS by creating your own plug-ins.
Applications can be written using the SEMS framework API
in C++, or in Python using an embedded python interpreter
of the ivr or py_sems modules, or the DSM.

## Requirements

All requirements are optional.

* Python 3 for the ivr (embedded python interpreter) and py_sems
* flite speech synthesizer for TTS in the ivr
* lame >= 3.95 for mp3 file output, mpg123 for mp3 playback
* spandsp library for DTMF detection and PLC (SEMS has its own implementations for both)
* libZRTP SDK (http://zfoneproject.com) for ZRTP
* libev for jsonrpc

## Supported environments

SEMS server hes been build-tested with the following:
* RHEL 7 with Python 3
* RHEL 8 with Python 3
* RHEL 9 with Python 3
* RHEL 10 with Python 3
* Debian 11 with Python 3
* Debian 12 with Python 3
* Debian 13 with Python 3

Please see appropriate Dockerfiles for reference.

## Building from source

```bash
git clone git@github.com:sems-server/sems.git
cd sems
mkdir -p build && cd build
cmake ..
make
ctest -V
./core/sems -v
sudo make install
```

## Build Options

### Feature Flags (`SEMS_USE_*`)

These CMake options control optional features and codec support. Pass them with `-D` on the cmake command line, e.g. `cmake -DSEMS_USE_OPUS=ON ..`

| Option | Default | Description |
|--------|---------|-------------|
| `SEMS_USE_OPUS` | `OFF` | Build with Opus codec support |
| `SEMS_USE_SPANDSP` | `OFF` | Build with spandsp (DTMF detection, G.722 codec) |
| `SEMS_USE_LIBSAMPLERATE` | `OFF` | Build with libsamplerate (external resampler); when OFF uses internal resampler |
| `SEMS_USE_ZRTP` | `OFF` | Build with ZRTP encryption support |
| `SEMS_USE_MP3` | `OFF` | Build with MP3 support (requires LAME; optionally mpg123 for decoding) |
| `SEMS_USE_ILBC` | `ON` | Build with iLBC codec (external library or bundled fallback) |
| `SEMS_USE_G729` | `OFF` | Build with G.729 codec (requires bcg729) |
| `SEMS_USE_CODEC2` | `OFF` | Build with Codec2 support |
| `SEMS_USE_TTS` | `OFF` | Build with text-to-speech support (requires Flite) |
| `SEMS_USE_OPENSSL` | `OFF` | Build with OpenSSL support |
| `SEMS_USE_MONITORING` | `ON` | Build with monitoring support (monitoring app + mod_monitoring DSM module) |
| `SEMS_USE_IPV6` | `OFF` | Build with IPv6 support |
| `SEMS_USE_PYTHON` | `ON` | Build Python-dependent modules (ivr, conf_auth, mailbox, pin_collect, mod_py) |
| `SEMS_USE_ASAN` | `OFF` | Build with AddressSanitizer (memory error detector) |

Some modules are also auto-detected based on whether their library is installed (e.g. `conference` requires libevent2, `db_reg_agent` requires MySQL++, `jsonrpc` requires libev).

### Excluding Modules

You can exclude specific modules from the build using these CMake variables. Module names are space- or semicolon-separated:

| Variable | Scope |
|----------|-------|
| `EXCLUDE_APP_MODULES` | Application modules in `apps/` |
| `EXCLUDE_DSM_MODULES` | DSM scripting modules in `apps/dsm/mods/` |
| `EXCLUDE_CORE_MODULES` | Core plug-in modules (codecs etc.) in `core/plug-in/` |

Examples:

```bash
# Exclude specific application modules (space-separated)
cmake -DEXCLUDE_APP_MODULES="voicemail webconference click2dial" ..

# Exclude DSM modules
cmake -DEXCLUDE_DSM_MODULES="mod_conference mod_py" ..

# Exclude core plug-in modules
cmake -DEXCLUDE_CORE_MODULES="echo adpcm" ..

# Combine feature flags and module exclusions
cmake -DSEMS_USE_PYTHON=OFF -DEXCLUDE_APP_MODULES="voicemail webconference" ..
```

Excluded modules will produce a status message during configuration, e.g.:
```
-- Excluding app module: voicemail
-- Excluding app module: webconference
```

### Path Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `SEMS_CFG_PREFIX` | (empty) | Configuration files prefix |
| `SEMS_AUDIO_PREFIX` | `/usr/local/lib` | Audio files prefix |
| `SEMS_EXEC_PREFIX` | `/usr/local` | Executable files prefix |
| `SEMS_RUNDIR_PREFIX` | `/var/local/run` | Runtime directory prefix |
| `SEMS_LIBDIR` | `lib` | Library directory name |
| `SEMS_DOC_PREFIX` | `/usr/share/doc` | Documentation prefix |

## Creating packages on debian (ubuntu, ...)

Install debian package build tools:
```bash
sudo apt-get install debhelper devscripts
```

Install dependencies (those below or let dpkg-buildpackage below tell you which ones):
```bash
sudo apt-get install debhelper g++ make libspandsp-dev flite1-dev \
                     libspeex-dev libgsm1-dev libopus-dev libssl-dev python-dev \
                     python-sip-dev openssl libev-dev libmysqlcppconn-dev libevent-dev \
                     libxml2-dev libcurl4-openssl-dev libhiredis-dev
```

Get the source: 
```bash
git clone https://github.com/sems-server/sems.git
```

```bash
cd sems; ln -s pkg/deb/buster ./debian
```

Set version in changelog if not correct:
```bash
dch -v x.y.z "SEMS x.y.z release"
```
or:
```bash
dch -b -v `git describe --always` "sems git master"
```

Build package:
```bash
dpkg-buildpackage -rfakeroot -us -uc
```

Install sems and sems-python-modules packages in .. using dpkg.

## Build in container and extract RPMs from the built container

This example is for RHEL7, for other OSes please see the appropriate Dockerfile.

```bash
mkdir -p /tmp/sems-rpms
# build the image which also builds the packages
docker build -t sems-rhel7 -f Dockerfile-rhel7 .
# create temporary container instance
docker create --name temp-sems-rhel7 sems-rhel7
# extract RPMs into host system
docker cp temp-sems-rhel7:/root/rpmbuild/RPMS /tmp/sems-rpms
# remove temporary container
docker rm temp-sems-rhel7
# list available SEMS RPMs
ls -al /tmp/sems-rpms/RPMS/x86_64/
```

## Documentation

In the doc/ directory there is a set of files describing the
applications shipped with SEMS, alongside some more documentation.
Generate the doxygen documentation with `make doc` in the build directory:
```bash
mkdir -p build && cd build
cmake ..
make doc
```
The generated documentation will be in doc/doxygen_doc.

## Support, mailing lists, bugs and contact

Best-effort support is given for SEMS.
Use https://github.com/sems-server/sems/issues which is the
first address to ask for help, report bugs and improvements.

Bugs can be filed as issues on this site.  Please submit all bugs,
crashes and feature requests you encounter.

## Authors

Raphael Coeffic (rco@iptel.org), the father of SEMS

Stefan Sayer (stefan.sayer@gmail.com), the lead developer

Contributors:

* Alex Gradinar
* Alfred E Heggestad
* Andreas Granig
* Andrey Samusenko
* Andriy I Pylypenko
* Anton Zagorskiy
* B. Oldenburg
* Balint Kovacs
* Bogdan Pintea
* Carsten Bock
* Greger Viken Teigre
* Grzegorz Stanislawski
* Helmut Kuper
* Jeremy A
* Jiri Kuthan
* Joe Stusick
* Jose-Luis Millan
* Juha Heinanen
* Marcel Hecko
* Matthew Williams
* Michael Furmur
* Ovidiu Sas
* Pavel Kasparek
* Peter Lemenkov
* Peter Loeppky
* Richard Newman
* Robert Szokovacs
* Rui Jin Zheng
* Tom van der Geer
* Ulrich Abend
* Vaclav Kubart
* Victor Seva

(if you feel you should be on this list, please submit PR)

Special thanks to FRAFOS GmbH, sipwise GmbH, IPTEGO GmbH, iptelorg GmbH and TelTech Systems Inc. for sponsoring development of SEMS.

## Contributions

All kinds of contributions and bug fixes are very welcome, for example new application or codec modules, documentation pages, howtos etc.

Please also have a look at the contributions license policy (see doc/COPYING).

SEMS - the media-S in the SLAMP.
