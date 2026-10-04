/*
 * Plug-in entry point of the SIP to DIS radio gateway
 *
 * Kept apart from SipDisGw.cpp so that the unit tests can link the gateway
 * next to other applications exporting the same factory symbol.
 */

#include "SipDisGw.h"

EXPORT_SESSION_FACTORY(SipDisGwFactory, MOD_NAME);
