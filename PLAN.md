# Issue Selection and Implementation Plan

## Issue Selected: #164 — Relay Reason Header from CANCEL Through SBC B2BUA

**GitHub:** https://github.com/sems-server/sems/issues/164
**Related PR (incomplete/stale):** https://github.com/sems-server/sems/pull/165

---

## Critical Assessment: Why This Issue?

### Issues Reviewed and Rejected

| Issue | Title | Verdict |
|-------|-------|---------|
| #210 | rtp_receiver_threads race condition | Complex threading bug. Not simple — requires deep concurrency expertise and extensive testing under high load. |
| #205 | SBCCallRegistry Call-ID mismatch with suffix patterns | Already has a fix branch (`fix/issue205-callid-registry-update`). Work is done. |
| #92  | out of buffers for RTP packets, dropping | Already has a fix branch (`fix/issue92-rtp-buffer-exhaustion`). Work is done. |
| #120 | twit module should use twython instead of twyt | **Out of scope / irrelevant.** A Twitter integration module in a VoIP server was always niche. Twitter's API has been overhauled (now X), twython itself is now archived/unmaintained, and the entire `apps/twit` module has no realistic user base. This issue should be closed, not fixed. |
| #45  | DVSNI / Let's Encrypt support | Large feature, not simple. Requires ACME protocol client integration. |
| #180 | trouble with callback | A support question, not a bug report. Should be closed as not-a-bug. |
| #104 | Disable TCP transport | A configuration question. Not a code issue. |
| #162 | Change Contact header on LegA | A usage question. Not a code issue. |
| #144 | Handle REFER in SBC DSM | Feature request for DSM scripting. Niche and complex. |
| #63  | IPv4/IPv6 networking problem | Stale (2017), vague report, likely resolved in later versions. |
| #90  | Echo App decode() error | Stale (2018), insufficient reproduction information. |
| #22  | Segfault on startup | Stale (2016), likely resolved. |

### Why #164 Is the Right Choice

1. **Real protocol compliance issue.** RFC 3326 defines the Reason header. When an upstream entity sends CANCEL with `Reason: SIP;cause=location`, that diagnostic information is lost at the SBC boundary. This breaks end-to-end diagnostics for every SEMS SBC deployment.

2. **Affects the primary use case.** The SBC is SEMS's flagship application. Anyone routing calls through SEMS as a B2BUA loses Reason headers on CANCEL, which matters for CDR generation, debugging, and interworking with telco networks.

3. **The infrastructure already exists.** `AmSipDialog::cancel(const string& hdrs)` accepts arbitrary headers. `trans_layer::cancel()` includes them in the outgoing message. The plumbing is there — the problem is that `CallLeg::onCancel()` discards the headers instead of forwarding them.

4. **Well-scoped.** The previous PR author (#165) mapped out exactly what was wrong with his attempt: it passes all headers instead of just Reason, lacks configurability, and lives in the wrong layer. Those are clear, fixable shortcomings.

5. **Community demand.** The issue has 7 comments, a PR attempt, and recent activity (October 2025). People care about this.

---

## Root Cause Analysis

SEMS operates as a B2BUA (Back-to-Back User Agent). When the A-leg caller sends CANCEL:

```
Caller --CANCEL(Reason: SIP;cause=location)--> SEMS A-leg
```

SEMS receives the CANCEL in `CallLeg::onCancel(const AmSipRequest& req)` at `apps/sbc/CallLeg.cpp:1063`. The `req` object contains the full Reason header. However:

1. `onCancel()` calls `stopCall()` which calls `terminateOtherLeg()`
2. `terminateOtherLeg()` posts a bare `B2BTerminateLeg` event with **no headers**
3. The B-leg receives this event and calls `terminateLeg()`
4. `terminateLeg()` calls `dlg->bye("", SIP_FLAGS_VERBATIM)` with **empty headers**

Result: The outgoing CANCEL to the callee has no Reason header.

```
SEMS B-leg --CANCEL(no Reason header)--> Callee
```

---

## Implementation Plan

### Step 1: Extend the B2BTerminateLeg Event to Carry Headers

**File:** `core/AmB2BSession.h`

Add a new event type or extend B2BEvent to carry a `hdrs` string when terminating:

```cpp
struct B2BTerminateLegEvent : public B2BEvent {
  string hdrs;
  B2BTerminateLegEvent() : B2BEvent(B2BTerminateLeg) {}
  B2BTerminateLegEvent(const string& hdrs) : B2BEvent(B2BTerminateLeg), hdrs(hdrs) {}
};
```

### Step 2: Extract and Store Reason Header on CANCEL Receipt

**File:** `apps/sbc/CallLeg.cpp` — `onCancel()`

Extract the Reason header from `req.hdrs` using the existing `getHeader()` utility and pass it through the termination flow:

```cpp
void CallLeg::onCancel(const AmSipRequest& req)
{
  if ((call_status == Ringing) || (call_status == NoReply)) {
    if (a_leg) {
      // Extract Reason header from incoming CANCEL
      string cancel_hdrs;
      string reason = getHeader(req.hdrs, SIP_HDR_REASON);
      if (!reason.empty()) {
        cancel_hdrs = SIP_HDR_COLSP(SIP_HDR_REASON) + reason + CRLF;
      }
      onCallFailed(CallCanceled, NULL);
      updateCallStatus(Disconnected, StatusChangeCause::Canceled);
      stopCall(StatusChangeCause::Canceled, cancel_hdrs);
    }
  }
}
```

### Step 3: Thread Headers Through stopCall → terminateOtherLeg

**File:** `apps/sbc/CallLeg.cpp`

Modify `stopCall()` and `terminateOtherLeg()` to accept and forward the headers string. Use the new `B2BTerminateLegEvent` when posting to the other leg.

### Step 4: Handle Headers on the Receiving End

**File:** `core/AmB2BSession.cpp`

In the `B2BTerminateLeg` case handler, downcast the event to `B2BTerminateLegEvent`, extract headers, and pass them to `terminateLeg()`:

```cpp
case B2BTerminateLeg: {
    B2BTerminateLegEvent* te = dynamic_cast<B2BTerminateLegEvent*>(ev);
    string hdrs = te ? te->hdrs : "";
    terminateLeg(hdrs);
    break;
}
```

### Step 5: Send CANCEL with Reason Header

**File:** `core/AmB2BSession.cpp` — `terminateLeg()`

Modify `terminateLeg()` to pass headers to `dlg->cancel(hdrs)` when the dialog is in early state (pre-200 OK), or to `dlg->bye(hdrs)` when connected:

```cpp
void AmB2BSession::terminateLeg(const string& hdrs)
{
  setStopped();
  clearRtpReceiverRelay();
  if (dlg->getStatus() == AmSipDialog::Connected) {
    dlg->bye("", SIP_FLAGS_VERBATIM);
  } else {
    dlg->cancel(hdrs);
  }
}
```

### Step 6: Add SBC Profile Configuration Option

**File:** `apps/sbc/SBCCallProfile.h` and `apps/sbc/SBCCallProfile.cpp`

Add a `relay_cancel_reason` boolean (default: `true`) to `SBCCallProfile`:

```cpp
// SBCCallProfile.h
bool relay_cancel_reason;

// SBCCallProfile.cpp — readFromConfiguration()
relay_cancel_reason = cfg.getParameter("relay_cancel_reason", "yes") == "yes";
```

Then gate the header extraction in `CallLeg::onCancel()` on this profile option.

### Step 7: Update Documentation

Add a note to the SBC README/configuration documentation about the new `relay_cancel_reason` parameter.

---

## What This Plan Improves Over PR #165

| PR #165 Limitation | This Plan |
|--------------------|-----------|
| Passes ALL headers from CANCEL | Extracts only the Reason header specifically |
| No configuration option | Adds `relay_cancel_reason` profile parameter |
| Changes in core layer | Primary changes in SBC application layer (CallLeg) with minimal core changes |
| Not tested on current master | Will be developed against current master |

---

## Files Modified

| File | Change |
|------|--------|
| `core/AmB2BSession.h` | Add `B2BTerminateLegEvent` struct |
| `core/AmB2BSession.cpp` | Handle headers in `B2BTerminateLeg` case; modify `terminateLeg()` |
| `apps/sbc/CallLeg.h` | Update `stopCall()` / `terminateOtherLeg()` signatures |
| `apps/sbc/CallLeg.cpp` | Extract Reason in `onCancel()`, pass through termination chain |
| `apps/sbc/SBCCallProfile.h` | Add `relay_cancel_reason` field |
| `apps/sbc/SBCCallProfile.cpp` | Read `relay_cancel_reason` from config |

---

## Risks and Mitigations

1. **Backward compatibility.** Default `relay_cancel_reason=yes` means behavior changes for existing deployments. Mitigation: This is the correct RFC-compliant behavior. Operators who don't want it can set `relay_cancel_reason=no`.

2. **Event struct compatibility.** The `B2BTerminateLegEvent` extends `B2BEvent`. Using `dynamic_cast` in the handler means old `B2BEvent` objects (from other code paths) are handled gracefully with empty headers.

3. **terminateLeg() signature change.** Adding a `const string& hdrs = ""` default parameter preserves all existing callers without modification.
