#ifndef BRN_NETWORK_DEFINES_H
#define BRN_NETWORK_DEFINES_H

#include "types.hpp"

// ===========================================================================
// network_defines.h
//   Home: GameSource/Network/network_defines.h
//
// Burnout network-wide shared definitions header, pulled in by the network
// subsystem's umbrella headers (e.g. BrnServerInterfaceBase.h). In the original
// tree this carried the network-wide compile switches / shared constants /
// typedefs for the Burnout online layer.
//
// FLAGGED: the concrete contents are not present in the available exports -- no
// committed TU in scope references a symbol from this header (BrnServerInterfaceBase
// only includes it transitively). It is provided as an honest, minimal placeholder
// so the umbrella header resolves; pull in the shared primitive vocabulary so any
// future definitions added here have it available. Real switches/constants/typedefs
// GROW this home additively as they are recovered from their own TUs.
// ===========================================================================

#endif // BRN_NETWORK_DEFINES_H
