#pragma once

// ===================================================================================
// CgsNetwork network utilities -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/CgsNetworkUtils.h
//
// Free helpers in the CgsNetwork namespace for IP-address string<->int conversion,
// username comparison, and CgsID encode/decode. SHAPE from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsNetworkUtils.h):
//       const char KC_CGS_ID_REPLACE_CHAR = 63;
//       int32_t IPAddressStringToInt(const char*);
//       void    IPAddressIntToString(char*, int32_t);
//       int32_t UsernameCompare(const char*, const char*);
//       void    NetworkEncodeUncompressedCgsID(char*);
//       void    NetworkDecodeUncompressedCgsID(char*);
//
// Ledger func for this TU:
//   IPAddressIntToString @ 0x8286EFF8 -- bodied in CgsNetworkUtils.cpp.
//
// The remaining helpers are declared here (their bodies live in their own TUs or call
// into the DirtySDK lobby API) so the rest of the network code can call them by name.
// ===================================================================================

#include "types.hpp"

namespace CgsNetwork
{
    // DWARF CgsNetworkUtils.h:37 -- the byte a CgsID encode substitutes for an
    // out-of-range character ('?').
    const char KC_CGS_ID_REPLACE_CHAR = 63;

    // A dotted-quad IP string ("255.255.255.255\0") needs 16 bytes.
    const s32 KI_IPADDRESS_STRING_LENGTH = 16;

    // Parse a dotted-quad "a.b.c.d" into a packed big-endian 32-bit address
    // (a<<24 | b<<16 | c<<8 | d).
    s32  IPAddressStringToInt(const char* lpacIPAddress);

    // Format a packed 32-bit address back into "a.b.c.d" in lacIPAddress (16 bytes).
    // Ledger func @ 0x8286EFF8.
    void IPAddressIntToString(char* lacIPAddress, s32 liIPAddress);

    // Compare two usernames; forwards to the DirtySDK lobby name comparator.
    s32  UsernameCompare(const char* lpacName1, const char* lpacName2);

    // In-place (de)compression of an uncompressed CgsID string. Bodies live in their
    // own TUs; declared here so callers can reference them by name.
    void NetworkEncodeUncompressedCgsID(char* lpacCgsID);
    void NetworkDecodeUncompressedCgsID(char* lpacCgsID);
}
