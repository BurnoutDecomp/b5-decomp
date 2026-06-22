#ifndef CGS_SERVER_INTERFACE_QUICK_JOIN_PARAMS_H
#define CGS_SERVER_INTERFACE_QUICK_JOIN_PARAMS_H

#include "types.hpp"

// ===========================================================================
// CgsNetwork::ServerInterfaceQuickJoinParamsBase
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceQuickJoinParams.{h,cpp}
//
// Parameter block describing a "quick join" request: a list of up to
// KI_MAX_QUICK_JOIN_PARAMS candidate game ids (each emitted as a GS<n> field),
// plus a ranked / userset / force-leave triple.
//
// LAYOUT (from dwarfdump + X360 asm @ 0x8287A398 Prepare / 0x8287A3E0
// SerialiseToString): a vtable-only polymorphic base, then the data members.
//   +0x00  vptr
//   +0x04  miNumParameters         (s32)        Prepare zeroes it
//   +0x08  maiQuickJoinParams[16]  (s32 x16)    Prepare XMemSets to -1 (0x40 bytes)
//   +0x48  mbRanked                (bool)       Prepare zeroes it (offset 0x48)
//   +0x49  mbJoinUserset           (bool)       Prepare zeroes it (offset 0x49)
// The first virtual after the dtor is Prepare (asm just below).
// ===========================================================================

namespace CgsNetwork
{
    // CgsServerInterfaceQuickJoinParams.h:84
    const s32 KI_MAX_QUICK_JOIN_PARAMS = 16;

    struct ServerInterfaceQuickJoinParamsBase
    {
    public:
        ServerInterfaceQuickJoinParamsBase();

        // CgsServerInterfaceQuickJoinParams.h:59
        virtual ~ServerInterfaceQuickJoinParamsBase();

        // CgsServerInterfaceQuickJoinParams.cpp:48
        virtual bool Prepare();

        // CgsServerInterfaceQuickJoinParams.cpp:69
        void SerialiseToString(char* lpcRecord, s32 liRecLen) const;

        // CgsServerInterfaceQuickJoinParams.h:118
        void SetJoinUserset(bool lbJoinUserset) { mbJoinUserset = lbJoinUserset; }

        // CgsServerInterfaceQuickJoinParams.h:125
        void SetRanked(bool lbRanked) { mbRanked = lbRanked; }

        // CgsServerInterfaceQuickJoinParams.h:131
        bool IsRanked() const { return mbRanked; }

    protected:
        // CgsServerInterfaceQuickJoinParams.h:83
        s32  miNumParameters;
        // CgsServerInterfaceQuickJoinParams.h:84
        s32  maiQuickJoinParams[KI_MAX_QUICK_JOIN_PARAMS];
        // CgsServerInterfaceQuickJoinParams.h:86
        bool mbRanked;
        // CgsServerInterfaceQuickJoinParams.h:87
        bool mbJoinUserset;
    };
}

#endif // CGS_SERVER_INTERFACE_QUICK_JOIN_PARAMS_H
