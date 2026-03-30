# ED-137B Ground Radio Station Simulator (`ed137b_grs_sim`)

A SEMS application module that simulates an ED-137B compatible Ground Radio
Station (GRS) at the SIP signalling level.

## Naming Convention

The naming used throughout this module follows a consistent scheme:

| Prefix/Term  | Meaning                                                  |
|--------------|----------------------------------------------------------|
| **ED-137B**  | EUROCAE standard revision B for VoIP in Air Traffic Control |
| **GRS**      | Ground Radio Station -- the radio equipment being simulated |
| **R2S**      | Radio-to-SIP interface profile defined in ED-137B        |
| `ed137b_grs_sim` | SEMS module name (used in config, app_mapping, build) |
| `Ed137bGrs...`   | C++ class prefix (e.g., `Ed137bGrsSimFactory`)       |
| `ED137B_...`     | C preprocessor macro prefix (e.g., `ED137B_HDR_WG67_VERSION`) |
| `ED137B-GRS`     | Log message prefix                                   |

## Overview

The simulator acts as a SIP User Agent that speaks the ED-137B R2S protocol.
It handles incoming and outgoing SIP sessions with the correct ED-137B SIP
headers and SDP attributes, and logs all signalling parameter changes
(frequency, channel spacing, TX/RX mode, etc.) to a dedicated log file.

### What it simulates

- **SIP headers**: `WG67-version` and `Priority` headers per ED-137B
- **SDP attributes**: `a=type`, `a=freq`, `a=txrxmode`, `a=cld` (channel
  spacing), `a=bss`, `a=sqc`, `a=climax`
- **R2S codec**: G.711 A-law (PCMA) with `a=fmtp:8 R2S` parameter
- **Re-INVITE handling**: Detects GRS parameter changes (frequency, mode,
  spacing) and logs transitions
- **UPDATE handling**: Same change detection for SIP UPDATE method
- **Always-on sessions**: GRS sessions stay alive (no RTP timeout)

### What it does NOT simulate

**RTP-level signalling (ED-137B RTP header extensions):**
- PTT (Push-To-Talk) indication -- the GRS does not read or write PTT bits
  in RTP header extensions; incoming PTT from clients is not detected or logged
- Squelch indication -- no SQU bit handling in RTP packets
- PTT-ID -- no transmitter identification via RTP extension fields
- Signal quality metrics (RSSI, BSS-Q) -- not reported in RTP extensions
- Key-in information distribution -- when a radio user keys the transmitter,
  a real GRS distributes PTT state to all connected CWPs via RTP header
  extensions on every outgoing packet; this simulator does not do so

**BSS (Best Signal Selection):**
- BSS arbitration -- no signal quality comparison across multiple receivers
- Climax switching -- SDP `a=climax` attribute is accepted and echoed but
  the GRS does not actively select or switch the best signal
- BSS RSSI/quality reporting to CWPs via RTP extensions

**Radio coupling and distribution:**
- Radio coupling/decoupling signalling between GRS instances
- Cross-coupling of audio between frequencies
- Simultaneous distribution of key-in state to multiple CWPs

**Security (ED-137C):**
- SRTP encryption of media streams
- TLS for SIP signalling
- DTLS-SRTP key exchange

**Transport:**
- Multicast RTP (some ED-137B deployments use multicast for efficiency)
- RTCP extended reports for ED-137B monitoring

**RF simulation:**
- Actual radio RF behaviour, signal propagation, or interference modelling

## Quick Start

### 1. Build

The module is included in the standard SEMS build:

```bash
mkdir build && cd build
cmake ..
make ed137b_grs_sim
```

Or exclude it with: `cmake -DEXCLUDE_APP_MODULES="ed137b_grs_sim" ..`

### 2. Configure

Copy the sample configuration:

```bash
cp apps/ed137b_grs_sim/etc/ed137b_grs_sim.conf /etc/sems/etc/ed137b_grs_sim.conf
```

Edit as needed (see [Configuration Reference](#configuration-reference) below).

### 3. Route SIP traffic

Add a mapping in `/etc/sems/etc/app_mapping.conf`:

```
# Route calls to sip:grs@... to the ED-137B GRS simulator
^sip:grs@.*=>ed137b_grs_sim

# Or route a specific extension
^sip:137@.*=>ed137b_grs_sim
```

### 4. Create the log directory

```bash
mkdir -p /var/log/sems
touch /var/log/sems/ed137b_grs_sim.log
chown sems:sems /var/log/sems/ed137b_grs_sim.log
```

### 5. Start SEMS

```bash
sems -f /etc/sems/sems.conf
```

## Configuration Reference

Configuration file: `/etc/sems/etc/ed137b_grs_sim.conf`

| Parameter          | Default          | Description                                       |
|--------------------|------------------|---------------------------------------------------|
| `frequency`        | `118.000`        | GRS frequency in MHz (SDP `a=freq`)               |
| `channel_spacing`  | `25kHz`          | Channel spacing: `25kHz` or `8.33kHz` (SDP `a=cld`) |
| `txrx_mode`        | `RxTx`           | TX/RX mode: `Rx`, `Tx`, or `RxTx` (SDP `a=txrxmode`) |
| `radio_type`       | `Radio`          | GRS type: `Radio`, `RadioRx`, `RadioTx` (SDP `a=type`) |
| `wg67_version`     | `radio.01.00`    | WG67-version SIP header value                     |
| `priority`         | `normal`         | SIP Priority header: `normal`, `urgent`, `emergency` |
| `bss`              | *(empty)*        | BSS mode: `bss` to enable, empty to disable       |
| `squelch_control`  | *(empty)*        | Squelch control: `sqcNormal`, `sqcCouple`         |
| `climax`           | `no`             | BSS climax state: `yes`/`no`                      |
| `audio_mode`       | `tone`           | Audio output: `tone` (400Hz side-tone) or `silence` |
| `tone_freq`        | `400`            | Tone frequency in Hz (when `audio_mode=tone`)     |
| `log_file`         | `/var/log/sems/ed137b_grs_sim.log` | Path to signalling change log   |

## Log File Format

All GRS signalling changes are logged with timestamps and Call-ID:

```
2026-03-30 14:22:01.123 [abc123@host] SESSION_START: freq=118.000 mode=RxTx spacing=25kHz type=Radio wg67=radio.01.00
2026-03-30 14:22:05.456 [abc123@host] FREQ_CHANGE: 118.000 -> 121.500
2026-03-30 14:22:05.456 [abc123@host] SPACING_CHANGE: 25kHz -> 8.33kHz
2026-03-30 14:22:10.789 [abc123@host] TXRXMODE_CHANGE: RxTx -> Rx
2026-03-30 14:25:00.000 [abc123@host] SESSION_END
```

### Logged Events

| Event              | Trigger                                          |
|--------------------|--------------------------------------------------|
| `SESSION_START`    | New GRS call established (initial INVITE 200 OK) |
| `SESSION_END`      | GRS call terminated (BYE received)               |
| `FREQ_CHANGE`      | Frequency changed via re-INVITE or UPDATE        |
| `SPACING_CHANGE`   | Channel spacing changed                          |
| `TXRXMODE_CHANGE`  | TX/RX mode changed                               |
| `TYPE_CHANGE`      | GRS type changed                                 |
| `BSS_CHANGE`       | BSS mode changed                                 |
| `SQC_CHANGE`       | Squelch control changed                          |
| `CLIMAX_CHANGE`    | BSS climax state changed                         |
| `PRIORITY_CHANGE`  | SIP Priority header changed                      |
| `WG67_CHANGE`      | WG67-version header changed                      |

## Example SIP Messages

### Incoming INVITE (from CWP to GRS simulator)

```
INVITE sip:grs@10.0.0.1 SIP/2.0
Via: SIP/2.0/UDP 10.0.0.2:5060
From: <sip:cwp1@10.0.0.2>;tag=abc
To: <sip:grs@10.0.0.1>
Call-ID: test123@10.0.0.2
CSeq: 1 INVITE
WG67-version: radio.01.00
Priority: normal
Content-Type: application/sdp

v=0
o=- 1 1 IN IP4 10.0.0.2
s=-
c=IN IP4 10.0.0.2
t=0 0
m=audio 5004 RTP/AVP 8
a=rtpmap:8 PCMA/8000
a=fmtp:8 R2S
a=type:Radio
a=freq:118.000
a=txrxmode:RxTx
a=cld:25kHz
```

### 200 OK Response (from GRS simulator)

```
SIP/2.0 200 OK
Via: SIP/2.0/UDP 10.0.0.2:5060
From: <sip:cwp1@10.0.0.2>;tag=abc
To: <sip:grs@10.0.0.1>;tag=xyz
Call-ID: test123@10.0.0.2
CSeq: 1 INVITE
WG67-version: radio.01.00
Priority: normal
Content-Type: application/sdp

v=0
o=sems 1 1 IN IP4 10.0.0.1
s=sems
c=IN IP4 10.0.0.1
t=0 0
m=audio 5006 RTP/AVP 8
a=rtpmap:8 PCMA/8000
a=fmtp:8 R2S
a=type:Radio
a=freq:118.000
a=txrxmode:RxTx
a=cld:25kHz
```

### Re-INVITE (frequency change)

```
INVITE sip:grs@10.0.0.1 SIP/2.0
...
CSeq: 2 INVITE
WG67-version: radio.01.00

v=0
...
m=audio 5004 RTP/AVP 8
a=rtpmap:8 PCMA/8000
a=fmtp:8 R2S
a=type:Radio
a=freq:121.500
a=txrxmode:Rx
a=cld:8.33kHz
```

This triggers the following log entries:

```
2026-03-30 14:22:05.456 [test123@10.0.0.2] FREQ_CHANGE: 118.000 -> 121.500
2026-03-30 14:22:05.456 [test123@10.0.0.2] SPACING_CHANGE: 25kHz -> 8.33kHz
2026-03-30 14:22:05.456 [test123@10.0.0.2] TXRXMODE_CHANGE: RxTx -> Rx
```

## Testing with SIPp

Basic test scenario -- send an INVITE, wait, send re-INVITE with new frequency:

```xml
<?xml version="1.0" encoding="UTF-8" ?>
<scenario name="ED-137B GRS Test">
  <send>
    <![CDATA[
      INVITE sip:grs@[remote_ip]:[remote_port] SIP/2.0
      Via: SIP/2.0/[transport] [local_ip]:[local_port]
      From: <sip:cwp@[local_ip]>;tag=[call_number]
      To: <sip:grs@[remote_ip]>
      Call-ID: [call_id]
      CSeq: 1 INVITE
      Contact: <sip:cwp@[local_ip]:[local_port]>
      Max-Forwards: 70
      WG67-version: radio.01.00
      Priority: normal
      Content-Type: application/sdp
      Content-Length: [len]

      v=0
      o=sipp 1 1 IN IP4 [local_ip]
      s=-
      c=IN IP4 [local_ip]
      t=0 0
      m=audio 6000 RTP/AVP 8
      a=rtpmap:8 PCMA/8000
      a=fmtp:8 R2S
      a=type:Radio
      a=freq:118.000
      a=txrxmode:RxTx
      a=cld:25kHz
    ]]>
  </send>

  <recv response="200" rtd="true" />
  <send> <![CDATA[ ACK sip:grs@[remote_ip]:[remote_port] SIP/2.0 [headers] ]]> </send>

  <pause milliseconds="3000" />

  <!-- Re-INVITE: change frequency and mode -->
  <send>
    <![CDATA[
      INVITE sip:grs@[remote_ip]:[remote_port] SIP/2.0
      Via: SIP/2.0/[transport] [local_ip]:[local_port]
      From: <sip:cwp@[local_ip]>;tag=[call_number]
      To: <sip:grs@[remote_ip]>[peer_tag_param]
      Call-ID: [call_id]
      CSeq: 2 INVITE
      Contact: <sip:cwp@[local_ip]:[local_port]>
      Max-Forwards: 70
      WG67-version: radio.01.00
      Content-Type: application/sdp
      Content-Length: [len]

      v=0
      o=sipp 1 2 IN IP4 [local_ip]
      s=-
      c=IN IP4 [local_ip]
      t=0 0
      m=audio 6000 RTP/AVP 8
      a=rtpmap:8 PCMA/8000
      a=fmtp:8 R2S
      a=type:Radio
      a=freq:121.500
      a=txrxmode:Rx
      a=cld:8.33kHz
    ]]>
  </send>

  <recv response="200" />
  <send> <![CDATA[ ACK sip:grs@[remote_ip]:[remote_port] SIP/2.0 [headers] ]]> </send>

  <pause milliseconds="2000" />

  <send>
    <![CDATA[
      BYE sip:grs@[remote_ip]:[remote_port] SIP/2.0
      Via: SIP/2.0/[transport] [local_ip]:[local_port]
      From: <sip:cwp@[local_ip]>;tag=[call_number]
      To: <sip:grs@[remote_ip]>[peer_tag_param]
      Call-ID: [call_id]
      CSeq: 3 BYE
      Max-Forwards: 70
      Content-Length: 0
    ]]>
  </send>

  <recv response="200" />
</scenario>
```

Run with:

```bash
sipp -sf ed137b_grs_test.xml -m 1 10.0.0.1:5060
```

Then check the log file:

```bash
tail -f /var/log/sems/ed137b_grs_sim.log
```

## Architecture

```
apps/ed137b_grs_sim/
├── Ed137bGrsSim.h/.cpp          # Factory + Session (main entry point)
├── Ed137bGrsSipHelper.h/.cpp    # SIP header and SDP attribute utilities
├── Ed137bGrsState.h/.cpp        # GRS state tracking and change logging
├── CMakeLists.txt               # Build configuration
├── etc/
│   └── ed137b_grs_sim.conf      # Default configuration
└── README.md                    # This file
```

The module registers as `ed137b_grs_sim` via `EXPORT_SESSION_FACTORY`. Incoming
calls matching the app_mapping pattern create an `Ed137bGrsSimSession` which:

1. Parses incoming ED-137B SIP headers and SDP attributes
2. Logs any GRS parameter changes to the dedicated log file
3. Replies with ED-137B compliant SIP headers and SDP
4. Generates PCMA audio (400Hz tone) to keep the RTP session alive
5. Handles re-INVITE and UPDATE for mid-session parameter changes

## ED-137B Reference

- EUROCAE ED-137B: "Interoperability Standard for VoIP ATM Components - Part 1: Radio"
- WG67 (Working Group 67) SIP/SDP extensions for radio-over-IP
- G.711 A-law (PCMA) is the mandatory codec for ED-137B R2S
- GRS = Ground Radio Station as defined in ED-137B
