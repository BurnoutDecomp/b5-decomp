#pragma once

#include "types.hpp"

// ============================================================================
// CgsPcNetIdentity.h -- the one PC network identity the platform leaves and the PC DirtySDK
// backend share.
//
// [PC platform leaf] On the console the signed-in profile supplies the gamertag and XUID, and the
// online lobby hands out the player ident. The PC has no profile service, so one host-side
// identity stands in for all three, and every consumer (the X* user leaves in
// CgsXboxLivePC.cpp, the DirtySDK PC backend in vendor\dirtysdk\src\pc\) must read it from here
// so the name, XUID and ident agree byte for byte.
//
// Environment, read once on first use and cached for the life of the process:
//   BP_LAN=0        explicitly disable the PC LAN service (default: enabled).
//   BP_LAN_NAME     persona, at most 15 characters (longer values are truncated).
//                   Unset: "Slot<n>" from BRN_HARNESS_SLOT, else USERNAME / "Player".
//   BP_LAN_XUID     optional XUID override (hex, optional 0x prefix).
//                   Unset or unparsable: 0x0009FFFF00000000 | FNV1a32(persona),
//                   also salted by COMPUTERNAME for the default persona.
// ============================================================================

// Persona string, NUL-terminated, at most 15 characters. Never null, never empty.
const char* CgsPcNetIdentityName();

// Stand-in XUID for the local user.
u64 CgsPcNetIdentityXuid();

// Lobby ident / player id: (FNV1a32 over the 8 little-endian XUID bytes & 0x7FFFFFFF) | 1.
// Never 0 (the login flow treats ident 0 as "no user").
u32 CgsPcNetIdentityLobbyIdent();

// True unless BP_LAN=0 explicitly disables the PC LAN service.
bool CgsPcNetLanEnabled();
