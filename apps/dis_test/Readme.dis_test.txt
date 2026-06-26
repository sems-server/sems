# DIS Integration Test Module (dis_test)

## Overview

The `dis_test` module is a minimal SEMS application for testing integration with DIS (Distributed Interactive Simulation) infrastructure.

**Features:**
- Generates 400Hz sinus wave audio (military radio test tone)
- Sends OpenDIS-encoded EntityStatePDU packets at regular intervals
- Supports both multicast and unicast modes
- Uses OpenDIS6 for protocol marshaling
- Configurable DIS gateway address and port

## Purpose

Use this module to:
1. **Test DIS integration** without ED-137B encoding
2. **Verify packet structure** using Wireshark
3. **Validate network connectivity** to DIS infrastructure
4. **Benchmark latency** and packet loss
5. **Develop DIS processing** on your wargame system

## Building

The module is built only if OpenDIS6 is available.

### 1. Install OpenDIS on target host

```bash
git clone https://github.com/open-dis/open-dis-cpp.git
cd open-dis-cpp
mkdir build && cd build
cmake .. -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF
make -j$(nproc)
sudo make install
```

If OpenDIS is installed in a non-default prefix, point CMake to it when building SEMS:

```bash
cmake .. -DOpenDIS_DIR=/path/to/opendis/lib/cmake/OpenDIS
```

### 2. Build SEMS with dis_test

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

Expected CMake output includes:

```text
Using OpenDIS: YES
```

If OpenDIS is not found, `dis_test` is skipped and the rest of SEMS still builds.

## Configuration

Edit `etc/dis_test.conf`:

```ini
# Multicast mode (true) or unicast to gateway (false)
dis_multicast_mode=true

# Multicast address (standard DIS: 224.0.0.1)
dis_multicast_addr=224.0.0.1

# Direct gateway address (if using unicast)
dis_gateway_addr=127.0.0.1

# DIS gateway port (standard: 3000)
dis_gateway_port=3000
```

### Configuration Options

| Parameter | Default | Description |
|-----------|---------|-------------|
| `dis_multicast_mode` | `true` | Use multicast (true) or unicast (false) |
| `dis_multicast_addr` | `224.0.0.1` | Multicast address for DIS |
| `dis_gateway_addr` | `127.0.0.1` | Unicast gateway IP |
| `dis_gateway_port` | `3000` | DIS port |
| `sinus_frequency` | `400` | Audio tone frequency (Hz) |
| `sinus_amplitude` | `0.5` | Audio amplitude (0.0-1.0) |
| `dis_send_interval_ms` | `100` | DIS packet send interval (ms) |
| `dis_exercise_id` | `1` | DIS exercise id |
| `dis_site_id` | `1` | Entity site id |
| `dis_application_id` | `200` | Entity application id |
| `dis_entity_id` | `1` | Entity id |
| `dis_force_id` | `1` | DIS force id |
| `dis_entity_country` | `840` | DIS country code |
| `dis_entity_category` | `50` | DIS entity category |

## Usage

### Step 1: Start SEMS with dis_test module

```bash
sudo sems -D -L4 -u dis_test
```

Or configure SEMS to load dis_test at startup.

### Step 2: Make SIP call to trigger test

Using any SIP client (e.g., SIPp, Linphone, Twinkle):

```bash
# Call example (adjust domain to your SEMS instance)
sip:dis_test@127.0.0.1
```

### Step 3: Verify audio and DIS packets

**Audio Test (optional):**
- Listen to the SIP call - you should hear a 400Hz sinus tone

**DIS Packets (primary test):**
- Start packet capture on port 3000:
  ```bash
  sudo tcpdump -i eth0 'udp port 3000' -XX
  ```
- Initiate the test call
- Verify UDP packets appear with DIS EntityStatePDU structure

### Step 4: End call to stop DIS transmission

```bash
# Hang up the call from SIP client
# SEMS logs will show packet count
```

## Testing Modes

### Mode 1: Local Loopback (No Network)

```ini
dis_multicast_mode=false
dis_gateway_addr=127.0.0.1
dis_gateway_port=3000
```

Start a local UDP listener:
```bash
nc -u -l 127.0.0.1 3000
```

Then make SIP call to dis_test. You'll see binary data arriving at the listener.

### Mode 2: Multicast (Network Testing)

```ini
dis_multicast_mode=true
dis_multicast_addr=224.0.0.1
dis_gateway_port=3000
```

Capture multicast traffic:
```bash
sudo tcpdump -i eth0 'net 224.0.0.0/4'
```

### Mode 3: Direct Gateway (Wargame Integration)

```ini
dis_multicast_mode=false
dis_gateway_addr=192.168.1.50
dis_gateway_port=3000
```

Point to your actual DIS gateway/wargame system.

## DIS Packet Structure

Each packet is an OpenDIS6-generated IEEE 1278 EntityStatePDU. Field defaults are configurable in `dis_test.conf`.

```
PDU Header (28 bytes)
├── Protocol Version (1 byte) = 6
├── Exercise ID (1 byte) = 1
├── PDU Type (1 byte) = 1 (EntityStatePDU)
├── Protocol Family (1 byte) = 1 (Simulation Management)
├── Timestamp (4 bytes) = simulation time (ms)
├── PDU Length (2 bytes)
└── Padding (2 bytes)

EntityID (12 bytes)
├── Site ID (2 bytes) = 1
├── Application ID (2 bytes) = 200
└── Entity ID (2 bytes) = 1

Entity Type (8 bytes)
├── Kind (1 byte) = 1 (Platform)
├── Domain (1 byte) = 1 (Land)
├── Country (2 bytes) = 840 (USA)
├── Category (1 byte) = 50 (Other)
└── [Subcategory, Specific, Extra]

Location (24 bytes)
├── X (8 bytes, double) = 0.0
├── Y (8 bytes, double) = 0.0
└── Z (8 bytes, double) = 0.0

Orientation (12 bytes)
├── Psi (4 bytes, float) = 0.0
├── Theta (4 bytes, float) = 0.0
└── Phi (4 bytes, float) = 0.0

Velocity (12 bytes)
└── [Linear velocity components]
```

**Key Details:**
- **Packets sent every 100ms** (configurable)
- **EntityID (1,200,1)** represents the "recording test forwarder"
- **Timestamp** increments from 0 at session start
- **All coordinates/velocities** are zero (stationary entity)

## Debugging

### Enable Verbose Logging

```bash
sudo sems -D -L7 -u dis_test
```

Look for log messages:
```
DisTest: loaded (multicast=yes, addr=224.0.0.1, port=3000)
DisTestSession: created (id=1234567890_123456789)
DisTestSession: onSessionStart (id=...)
SinusAudioSource: freq=400.0 Hz, amplitude=0.50, phase_inc=0.314159
DISPacketSender: started (multicast=yes)
DISPacketSender: sent packet #1 (size=100, sim_time=0 ms)
DISPacketSender: sent packet #2 (size=100, sim_time=100 ms)
...
```

### Packet Capture Analysis

```bash
# Capture 10 packets
sudo tcpdump -i eth0 'udp port 3000' -c 10 -XX > dis_packets.txt

# Analyze with Wireshark GUI
wireshark &
# Open capture, filter: udp.dstport == 3000
```

### Check SEMS Module Loaded

```bash
ldd /usr/local/lib/sems/plug-in/dis_test.so | grep -E 'OpenDIS6|opendis6'
# Should show OpenDIS6 linked
```

## Troubleshooting

### Module fails to load
- Check `/var/log/sems/sems.log` for errors
- Verify `etc/dis_test.conf` exists in SEMS config directory
- Ensure `dis_test.so` is in `/usr/local/lib/sems/plugins/`

### No audio heard in SIP call
- Audio generation should work (no dependencies)
- Check SEMS audio routing configuration
- Verify RTP is flowing (use `netstat -an | grep rtp`)

### DIS packets not appearing
- Check firewall: `sudo ufw allow 3000/udp`
- Verify multicast routing (if using multicast):
  ```bash
  route -n | grep 224.0.0.0
  ```
- Test with local UDP listener first (Mode 1)

### Multicast not working
- Check interface supports multicast: `ip link show`
- Verify group membership:
  ```bash
  netstat -g
  ```
- Try unicast mode (Mode 3) to troubleshoot network separately from multicast

## Next Steps

Once this test module works:

1. **Verify DIS packet arrival** on your wargame system
2. **Parse EntityStatePDU** in your DIS consumer
3. **Extend with ED-137B encoding** (add audio payload metadata)
4. **Integrate with real SIPREC** (recording infrastructure)
5. **Test with multi-party calls** (multiple entities)

## Performance Characteristics

- **Packet rate**: ~10 packets/sec (100ms interval)
- **CPU overhead**: Minimal (< 0.1% per session)
- **Memory**: ~100KB per session
- **Latency**: < 10ms to DIS gateway (LAN)

## References

- IEEE 1278: DIS Protocol Specification
- RFC 2326: RTSP (not used in this module, future extension)
- SEMS Module API: `/usr/local/include/sems/AmSession.h`
- OpenDIS C++: https://github.com/open-dis/open-dis-cpp

---

**Module Created**: 2026-06-26  
**Status**: Test/Development  
**License**: GPL (same as SEMS)
