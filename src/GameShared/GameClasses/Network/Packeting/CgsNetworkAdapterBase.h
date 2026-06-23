#pragma once

// ===================================================================================
// CgsNetwork::NetworkAdapterPrepareParams -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h
//
// The prepare-params block passed (by pointer) into the network-adapter Prepare chain
// (CgsNetworkAdapterBase.h in the X360 build); populated by Construct @ 0x82581330 and
// consumed by BrnNetwork::BrnNetworkManager::Prepare.
//
// LAYOUT (X360 asm @ 0x82581330 Construct -- five word stores, used size 0x14 / 20B):
//   stw r25,0x00  ; +0x00  the 6th ctor arg (a6)            -- mpField_00
//   stw r29,0x04  ; +0x04  leServerType (E_SERVER_TYPE_*)   -- meServerType
//   stw r28,0x08  ; +0x08  lpHeapMalloc                     -- mpHeapMalloc
//   stw r27,0x0C  ; +0x0C  lpNetworkManager                 -- mpNetworkManager
//   stw r26,0x10  ; +0x10  the 5th ctor arg (a5)            -- mpField_10
//
// Construct bounds-checks leServerType in [E_SERVER_TYPE_LOCAL, E_SERVER_TYPE_COUNT) and
// non-null lpHeapMalloc / lpNetworkManager, then writes the five words in this order. The
// two end words (a5, a6) are stored straight from the call but never individually read in
// the available exports, so they keep raw-offset-derived names with their observed pointer
// width; recovering their meaning would GROW this home additively. The struct is
// non-polymorphic (Construct takes `this` as a plain pointer; no vtable store).
//
// E_SERVER_TYPE_LOCAL == 0, E_SERVER_TYPE_COUNT == 7 (from the asm bounds blt 0 / blt 7).
// ===================================================================================

#include "types.hpp"

namespace CgsNetwork
{
    struct NetworkAdapterPrepareParams
    {
        enum EServerType
        {
            E_SERVER_TYPE_LOCAL = 0,
            E_SERVER_TYPE_COUNT = 7,
        };

        // @ 0x82581330 -- populate the param block (returns `this`).
        // Param order matches the X360 register order: leServerType, lpHeapMalloc,
        // lpNetworkManager, then the two trailing opaque words (a5 -> mpField_10,
        // a6 -> mpField_00).
        NetworkAdapterPrepareParams* Construct(u32 leServerType, void* lpHeapMalloc,
                                               void* lpNetworkManager,
                                               void* lpField_10, void* lpField_00);

        void* mpField_00;        // +0x00  (6th ctor arg, a6)
        u32   meServerType;      // +0x04  EServerType
        void* mpHeapMalloc;      // +0x08
        void* mpNetworkManager;  // +0x0C
        void* mpField_10;        // +0x10  (5th ctor arg, a5)
    };
}
