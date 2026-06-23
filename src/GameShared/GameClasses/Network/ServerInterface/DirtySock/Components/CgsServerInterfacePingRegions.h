#ifndef CGS_SERVER_INTERFACE_PING_REGIONS_H
#define CGS_SERVER_INTERFACE_PING_REGIONS_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// ===========================================================================
// CgsNetwork::ServerInterfacePingRegions
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfacePingRegions.{h,cpp}
//
// The DirtySock server-interface component that pings the candidate game-server regions and
// records each region's round-trip ping. Derives from CgsNetwork::ServerInterfaceComponent
// (its destructors -- this leaf's @0x827DE2C8 and the sibling ServerInterfaceHttp's
// @0x827DE1F0 -- both restore the shared component vtable off_820CDBF8 at this+0, the
// hallmark of a ServerInterfaceComponent leaf).
//
// LAYOUT (X360 asm @ 0x8287C8A8 Construct + @ 0x82877968 GetPingValue + @ 0x8287CC18
// HandlePingResults), after the 4-word Component base:
//     +0x00  (Component base: vptr / mpcCurrentAction / meStatus / miLastError)
//     +0x10  maRegionPings[KI_MAX_PING_REGIONS]   -- per-region ping values; ctor fills -1
//     +0xD8  miField_D8        (s32)   ctor stores 0
//     +0xDC  miField_DC        (s32)   ctor stores 0
//     +0xE0  mpPingManager     (void*) ctor stores 0; a refcounted callback object
//                                       (HandlePingResults calls its vtable slot +0xC, frees it)
//     +0xE4  miCurrentRegion   (s32)   ctor stores 0; the count of regions filled so far
//     +0xE8  miField_E8        (s32)   ctor stores -1
//     +0xEC  miField_EC        (s32)   ctor stores 2
//     +0xF0  miField_F0        (s32)   ctor stores 1
//
// KI_MAX_PING_REGIONS == 0x32 (50) is grounded in HandlePingResults
// (@ 0x8287CC18: `cmpwi miCurrentRegion, 0x32` -- the "miCurrentRegion < KI_MAX_PING_REGIONS"
// guard) and in Construct's 50-iteration -1 fill loop (mtctr 0x32). The non-ping trailing
// words keep raw-offset names (only the ctor's zero/sentinel stores reach them; their
// descriptive meaning is not present in the available exports) so the array + miCurrentRegion
// that THIS TU's functions touch land at the asm-observed offsets.
//
// This TU owns GetPingValue (@ 0x82877968) and the X360 scalar-deleting destructor
// (@ 0x827DE2C8). The component's behavioural methods (Construct / Prepare / Update /
// StartPingRegions / ...) are declared for layout fidelity but bodied in their own TUs.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterfacePingRegions : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfacePingRegions.h -- max number of ping regions (asm literal 0x32).
        static const s32 KI_MAX_PING_REGIONS = 50;

        ServerInterfacePingRegions();

        // CgsServerInterfacePingRegions.h -- scalar/vector deleting destructor @ 0x827DE2C8.
        virtual ~ServerInterfacePingRegions();

        // @ 0x82877968 -- return the recorded ping for liRegion. Asserts
        // 0 <= liRegion < miCurrentRegion (both vacuous), then returns maRegionPings[liRegion].
        s32 GetPingValue(s32 liRegion) const;

    private:
        s32   maRegionPings[KI_MAX_PING_REGIONS];   // +0x10 (50 words; ctor fills -1)
        s32   miField_D8;                           // +0xD8
        s32   miField_DC;                           // +0xDC
        void* mpPingManager;                        // +0xE0
        s32   miCurrentRegion;                      // +0xE4
        s32   miField_E8;                           // +0xE8
        s32   miField_EC;                           // +0xEC
        s32   miField_F0;                           // +0xF0
    };
}

#endif // CGS_SERVER_INTERFACE_PING_REGIONS_H
