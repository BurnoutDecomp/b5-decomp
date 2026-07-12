#pragma once

// ===========================================================================
// XCAM::CRemoteConsole -- one remote-peer receive pipeline of the X360 XCAM
// (Xbox Live Vision camera) streaming SDK in BURNOUT_X360_ARTIST.XEX. A
// CRemoteConsoleList owns a fixed pool of these (30 embedded by value); each one
// reassembles a peer's wire stream through an embedded CDepacketizer (FEC frame
// reassembly) and decodes it through an embedded CDecoder, and threads itself
// through the owner's active/free list via mLink.
//
// This header is the canonical (currently MINIMAL) home for CRemoteConsole. The
// full class -- its vtable, ctor/dtor, Initialize/ReInitialize/DoWork bodies and
// the per-console state between the named members below -- is its own TU and is
// NOT reconstructed here; only the SHAPE the owning CRemoteConsoleList touches is
// modelled (the 36-byte peer address, the disabled flag, the embedded
// CDecoder/CDepacketizer subobjects and the intrusive list node). The methods are
// declared so the list bodies compile against them; their definitions land when
// the CRemoteConsole TU is worked, extending this header.
//
// The X360 byte offsets are quoted from the disassembly (of the sibling
// CRemoteConsoleList methods that reach into a console). On the x64 PC target the
// embedded subobjects and pointers widen, so those displacements do not carry
// over; access is by named member and the mReservedNN buffers stand in for the
// still-unattested per-console fields (the padding-recovery idiom). `XCAM` is an
// X360 SDK boundary, so its identifiers are preserved verbatim per the naming
// convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamDecoder.h"        // CDecoder (embedded subobject)
#include "SDKs/XCam/XCamDepacketizer.h"   // CDepacketizer (embedded subobject)

namespace XCAM
{

// A circular doubly-linked list link. One is embedded in every CRemoteConsole
// (mLink); the CRemoteConsoleList keeps two heads of the same type (its active
// and free lists). A console is threaded through exactly one list at a time.
struct ListLink
{
    ListLink* mpNext;   // +0x00
    ListLink* mpPrev;   // +0x04
};

// The stream-setup descriptor handed to CRemoteConsoleList::Initialize and passed
// on to each CRemoteConsole::Initialize. Only the console-count field is attested
// here (the CRemoteConsoleList Initialize/Shutdown read it at +0x24); the leading
// stream parameters are consumed by CRemoteConsole's own TU and left opaque.
struct RemoteConsoleConfig
{
    u8  mReserved0[0x24];  // +0x00 stream parameters (dimensions / rate / source)
    s32 miConsoleCount;    // +0x24 number of remote consoles to bring up
};

class CRemoteConsole
{
public:
    // Its own TU (bodies not reconstructed here); declared so the list compiles.
    CRemoteConsole();                                      // sets the vtable + builds subobjects
    virtual ~CRemoteConsole();                             // vtable @ off_8210BCC8
    int Initialize(RemoteConsoleConfig* pConfig);          // bring the pipeline up
    int ReInitialize(int a1, int a2, int a3, int a4);      // tear the session down / re-arm
    int DoWork();                                          // pump one work step

    // --- shape reached by CRemoteConsoleList (X360 offsets quoted) ---
    u8            maAddress[36];      // +0x04 remote-peer address / session identity
    u8            mReserved28[0x2C];  // +0x28 unattested per-console state (own TU)
    bool          mbDisabled;         // +0x54 set by SetEnabledStateForAllActiveConsoles
    u8            mReserved55[0x0B];  // +0x55 pad to the embedded decoder
    CDecoder      mDecoder;           // +0x60 receive-side video decoder
    CDepacketizer mDepacketizer;      // +0x1A0 receive-side FEC depacketizer
    u8            mReservedTail[0x2C];// +0x440 unattested per-console state (own TU)
    ListLink      mLink;              // +0x46C intrusive active/free list node
};

} // namespace XCAM
