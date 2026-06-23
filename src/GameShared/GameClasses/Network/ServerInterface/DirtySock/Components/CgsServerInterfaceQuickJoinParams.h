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
//
// On X360 the concrete type is ServerInterfaceQuickJoinParamsX360 (the platform
// leaf `ServerInterfaceQuickJoinParams` resolves to). Its Prepare
// (X360 @ 0x8287A4A8) clears the base list (count 0, all 16 slots -1, both flag
// bytes 0) and additionally zeroes an 80-byte X360-only payload at +0x4C plus a
// trailing word at +0x9C -- modelled below as maX360Payload[80] + muX360Field_9C
// so every byte the asm writes is addressable by name. The vector-deleting
// destructor for the base (X360 @ 0x82541550) is the compiler-emitted thunk for
// the virtual dtor declared here.
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

        // its own TU
        virtual bool Prepare();

        // its own TU
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
        bool mbRanked;        // +0x48
        // CgsServerInterfaceQuickJoinParams.h:87
        bool mbJoinUserset;   // +0x49
    };

    // X360 platform leaf. `ServerInterfaceQuickJoinParams` aliases to this on the
    // X360 build (matching the BrnNetwork::QuickJoinParams call site that constructs
    // the alias). Adds an 80-byte payload at +0x4C and a trailing word at +0x9C that
    // the X360 Prepare zeroes; the base occupies +0x00..+0x49 (two trailing bools,
    // then 2 bytes of alignment padding before the +0x4C word-aligned payload).
    struct ServerInterfaceQuickJoinParamsX360 : public ServerInterfaceQuickJoinParamsBase
    {
    public:
        ServerInterfaceQuickJoinParamsX360();

        // X360 @ 0x8287A4A8 -- reset the base list + zero the X360 payload, return true.
        virtual bool Prepare();

        // Alignment padding: the base ends at +0x4A (two trailing bools); the X360
        // payload below is word-aligned at +0x4C (the asm's addi r3, r31, 0x4C).
        u8  maAlignPad_4A[2];    // +0x4A
        // The X360-only payload the leaf Prepare zeroes (this+0x4C .. this+0x9B).
        u8  maX360Payload[80];   // +0x4C  (XMemSet(this+0x4C, 0, 80))
        u32 muX360Field_9C;      // +0x9C  (single stw 0)
    };

    typedef ServerInterfaceQuickJoinParamsX360 ServerInterfaceQuickJoinParams;
}

#endif // CGS_SERVER_INTERFACE_QUICK_JOIN_PARAMS_H
