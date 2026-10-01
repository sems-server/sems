# SIP to DIS radio gateway (sip_dis_gw)

## Overview

`sip_dis_gw` connects SIP phones to the radio nets of a DIS (IEEE 1278.1-2012,
DIS 7) simulation. A call to `sip:<radio>@<sems>` becomes a DIS radio tuned to
that radio's frequency:

- the caller hears every DIS transmitter on the frequency, mixed;
- while the caller is keyed (push-to-talk), the caller's voice is sent as
  Signal PDUs, and the radio's Transmitter PDU says "transmitting";
- callers on the same frequency hear each other.

Every call is one radio of the gateway's DIS entity
(`dis_site_id:dis_application_id:dis_entity_id`), with its own radio number
(1, 2, ...). Only voice is handled; no ED-137 signalling.

The module has no dependencies beyond SEMS itself: the DIS 7 radio PDUs are
encoded and decoded in `Dis7Pdu.cpp` (OpenDIS does not implement the DIS 7
Transmitter and Signal PDUs).

## Building

The module is off by default:

```bash
mkdir build && cd build
cmake .. -DSEMS_USE_SIP_DIS_GW=ON
make
sudo make install
```

`Dockerfile-rhel10-sip-dis-gw` builds it, runs the unit tests and checks that
the module loads.

## Setup

Load the module and route calls to it, e.g. in `sems.conf`:

```ini
load_plugins=wav;sip_dis_gw
application=sip_dis_gw
```

Then configure `sip_dis_gw.conf`; every parameter is described there. The
essentials:

```ini
# broadcast, multicast or unicast, and the broadcast address, group or peer
dis_mode=broadcast
dis_address=192.168.1.255
dis_port=3000
dis_exercise_id=1

# sip:tower@<sems> and sip:guard@<sems>
radio_tower=118.1MHz
radio_guard=121.5MHz

# also sip:118100@<sems> (kHz) and sip:118.1@<sems> (MHz)
allow_dialed_frequency=yes

ptt_mode=vox
```

Calls to unknown radios are rejected with `404 Unknown Radio`.

## Push-to-talk

SIP has no push-to-talk, so `ptt_mode` chooses how a caller keys the radio:

| Mode     | Keyed while ...                                                  |
|----------|------------------------------------------------------------------|
| `vox`    | the caller speaks: above `vox_threshold_dbfs`, held for `vox_hang_ms` |
| `dtmf`   | between `*` and `#` (unkeyed after `ptt_max_ms` at the latest)  |
| `always` | the whole call                                                   |
| `listen` | never                                                            |

In `vox` mode the `vox_preroll_ms` of audio before the radio keyed is sent as
well, so that the first syllable is not lost. In every mode the radio unkeys
when the caller's audio stops for `tx_idle_ms` and keys again when it resumes.

## DIS behaviour

Sent, all DIS 7, PDU status 0:

- **Transmitter PDU**: on every state change (on, transmitting, off) and every
  `transmitter_heartbeat_ms`. Frequency, bandwidth, power, modulation, radio
  entity type and antenna position come from the configuration.
- **Signal PDU**: one per `signal_frame_ms` of audio while transmitting,
  8 kHz, encoded as `encoding` (mu-law by default).
- **Receiver PDU** (`publish_receiver`): "on, receiving" with the transmitter
  heard (the first one, if several) or "on, not receiving"; on change and
  every `receiver_heartbeat_ms`.
- **Entity State PDU** (`publish_entity_state`, off by default): a static
  entity at the antenna position, for simulators that ignore radios without
  an entity.

Received: Transmitter and Signal PDUs of the configured exercise, protocol
versions 4 to 7, also several PDUs bundled in one datagram. PDUs with the
gateway's own site and application are ignored (they come back on broadcast
and multicast loop). A Signal PDU is played on every radio whose frequency is
within `frequency_tolerance` of the frequency in its transmitter's latest
Transmitter PDU; signals of unknown transmitters are dropped. Decoded
encodings: 8-bit mu-law, 16-bit PCM big and little endian, 8-bit unsigned PCM,
at any sample rate (resampled to 8 kHz). Other encodings (CVSD, GSM, ...) are
logged once per transmitter and ignored.

Remote transmitters that send nothing for `remote_transmitter_timeout_ms` are
forgotten. Each transmitter heard gets its own playout buffer
(`jitter_prebuffer_ms`, capped at `jitter_max_ms`), and concurrent
transmitters are summed.

## Testing without a simulator

Wireshark decodes DIS: capture on the DIS port (`udp.port == 3000`) and call
`sip:118100@<sems>` with `ptt_mode=always`; Transmitter PDUs (state
"on, transmitting") and Signal PDUs appear. Two phones calling the same radio
hear each other through the local loop.

The unit tests (`sems_tests`, suite `test_sip_dis_gw`) cover the PDU layouts,
the audio helpers, the configuration, and the network side against a fake
simulator on the loopback interface.

## Limitations

- DIS 7 over IPv4 only; one exercise.
- No CVSD, GSM or other compressed Signal PDU encodings.
- Radio propagation is not modelled: everything on the frequency is heard.
- Push-to-talk priority and emergency (ED-137) have no counterpart here.
