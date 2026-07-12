#pragma once

// ===========================================================================
// XCAM::CRemoteConsole -- one remote-peer receive pipeline of the X360 XCAM
// (Xbox Live Vision camera) streaming SDK in BURNOUT_X360_ARTIST.XEX. A
// CRemoteConsoleList owns a fixed pool of these (30 embedded by value); each one
// reassembles a peer's wire stream through an embedded CDepacketizer (FEC frame
// reassembly) and decodes it through an embedded CDecoder, and threads itself
// through the owner's active/free list via mLink.
//
// This header is the canonical OWNING home for the reconstructed console bodies
// in XCamRemoteConsole.cpp:
//
//     XCAM::CRemoteConsole::Initialize           @ 0x82982AC8
//     XCAM::CRemoteConsole::DoWork               @ 0x82982980
//     XCAM::CRemoteConsole::GetNextDataChunk     @ 0x82982A50
//     XCAM::CRemoteConsole::ReleaseLastFrameData @ 0x82981B68
//
// The three send/QOS methods (GetEncodedPacket @ 0x82982B90,
// SubmitEncodedPacket @ 0x82982E30, ReInitialize @ 0x829832C8) are declared here
// but NOT defined: their bodies reach through the still-un-homed owning
// XCAM::CStreamEngine (mpConfig, +0x468) to its embedded CQOSController (+0x200),
// CPacketizer (+0x1BC), CRemoteConsoleList (+0x268) and rate-control fields, and
// call un-homed collaborators (CStreamEngine::UpdatePrivilegeBits,
// CEncoder::GetSequenceHeader, several blocked CQOSController bodies). They land
// when CStreamEngine is homed. They are declared so the committed
// CRemoteConsoleList / CStreamEngine callers compile against them.
//
// There is no reference source and no DWARF for this TU; the SHAPE below is
// reconstructed store-for-store from the X360 asm. The X360 byte offsets are
// quoted from the disassembly; on the x64 PC target the embedded subobjects and
// pointers widen, so those displacements do not carry over -- access is by named
// member throughout, per the widening convention. `XCAM` is an X360 SDK
// boundary, so its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamDecoder.h"        // CDecoder (embedded subobject)
#include "SDKs/XCam/XCamDepacketizer.h"   // CDepacketizer (embedded subobject)

namespace XCAM
{

// --- Xbox-360 kernel critical section (platform boundary) ------------------
// The RTL_CRITICAL_SECTION each console guards its DoWork / (re)init against.
// 28 bytes, the same opaque block the sibling CEncoder models. Only the Rtl*
// entry points below touch it; the fields are opaque. Supplied by the platform
// layer; NOT reconstructed here.
struct RTL_CRITICAL_SECTION_RC
{
    u8 mReserved[28];
};
void RtlInitializeCriticalSection(RTL_CRITICAL_SECTION_RC* pCriticalSection);
s32  RtlEnterCriticalSection(RTL_CRITICAL_SECTION_RC* pCriticalSection);
s32  RtlLeaveCriticalSection(RTL_CRITICAL_SECTION_RC* pCriticalSection);

// A circular doubly-linked list link. One is embedded in every CRemoteConsole
// (mLink); the CRemoteConsoleList keeps two heads of the same type (its active
// and free lists). A console is threaded through exactly one list at a time.
struct ListLink
{
    ListLink* mpNext;   // +0x00
    ListLink* mpPrev;   // +0x04
};

// The stream-setup descriptor handed to CRemoteConsoleList::Initialize and passed
// on to each CRemoteConsole::Initialize. It is the leading (config-bearing)
// portion of the owning CStreamEngine: CRemoteConsole::Initialize reads the video
// parameters below to bring up the embedded CDecoder / CDepacketizer, and the
// list reads miConsoleCount. The deeper CStreamEngine members (its embedded
// packetizer / QOS controller / console list) are only reached by the blocked
// console methods, so they stay opaque here. Field offsets attested by the
// CRemoteConsole::Initialize asm (a2[4]/a2[5]/a2[8]/a2[10]/a2[16]).
struct RemoteConsoleConfig
{
    u8    mReserved0[0x10]; // +0x00 stream identity / handles
    u32   muDimensions;     // +0x10 (a2[4]) packed frame dimensions (w<<16 | h)
    u32   muBitrate;        // +0x14 (a2[5]) target bit rate / depacketizer tag
    u8    mReserved18[0x08];// +0x18
    s32   miFramerate;      // +0x20 (a2[8]) target frame rate
    s32   miConsoleCount;   // +0x24 number of remote consoles to bring up
    u32   muPayloadSize;    // +0x28 (a2[10]) per-chunk payload size
    u8    mReserved2C[0x14];// +0x2C
    D3DDevice* mpDevice;    // +0x40 (a2[16]) decoder D3D device
};

class CRemoteConsole
{
public:
    // @ 0x82981CE8 (list ctor inlines each console's) -- sets the vtable + builds
    // the embedded subobjects. Its own construction is folded into the list ctor;
    // declared so the by-value pool default-constructs.
    CRemoteConsole();                                      // sets the vtable + builds subobjects
    virtual ~CRemoteConsole();                             // vtable @ off_8210BCC8

    // @ 0x82982AC8 -- capture the stream config, zero the per-console state, arm
    // the critical section and bring up the embedded decoder and depacketizer from
    // the config's video parameters (the console itself is the decoder's data
    // source). Returns 0, or the decoder / depacketizer Initialize failure code.
    int Initialize(RemoteConsoleConfig* pConfig);

    // @ 0x829832C8 -- tear the session down / re-arm (privilege set + send/receive
    // enables). BLOCKED: reaches through the un-homed CStreamEngine.
    int ReInitialize(int a1, int a2, int a3, int a4);

    // @ 0x82982980 -- pump one receive step under the lock: when receiving and the
    // depacketizer has a frame ready, decode it and fold the decode result into
    // the outbound QOS feedback flags (resetting the depacketizer on a hard error).
    int DoWork();

    // @ 0x82982A50 -- fetch the next chunk of the frame being decoded: either the
    // single directly-submitted chunk (mbHasDirectChunk) or the next depacketizer
    // chunk. *ppData = payload, *piLength = length, *pbLast = last-chunk flag.
    // Returns 10 while chunks remain, else 0.
    int GetNextDataChunk(void** ppData, int* piLength, int* pbLast);

    // @ 0x82981B68 -- release the packets of the frame just decoded (forwards to
    // the depacketizer).
    int ReleaseLastFrameData();

    // @ 0x82982B90 -- assemble the next outbound encoded packet (or a bare QOS
    // feedback packet). BLOCKED: reaches through the un-homed CStreamEngine to its
    // packetizer / QOS controller and calls CEncoder::GetSequenceHeader.
    int GetEncodedPacket(u8* pPacket, int* piOutLen);

    // @ 0x82982E30 -- accept an inbound encoded packet: feed QOS feedback, then
    // route the packet to the depacketizer / decoder. BLOCKED: reaches through the
    // un-homed CStreamEngine to its QOS controller.
    int SubmitEncodedPacket(const u8* pPacket, int iLength, XOVERLAPPED* pOverlapped);

    // --- shape (member ORDER store-for-store with the X360 asm; X360 offsets
    //     quoted, but access is by named member) ---
    u8                     maAddress[36];        // +0x04 remote-peer address / session id
    u64                    maPrivileges[4];      // +0x28 per-console privilege set
    s32                    miPrivilegeCount;     // +0x48 entries in maPrivileges
    s32                    mbSending;            // +0x4C encode/send side active
    s32                    mbReceiving;          // +0x50 decode/receive side active
    bool                   mbDisabled;           // +0x54 skip decoded output (set by list)
    u8                     mPad55[3];            // +0x55 align next word
    s32                    mbSendSequenceHeader; // +0x58 emit a sequence header next
    u32                    muSequenceNumber;     // +0x5C outbound packet sequence
    CDecoder               mDecoder;             // +0x60 receive-side video decoder
    CDepacketizer          mDepacketizer;        // +0x1A0 receive-side FEC depacketizer
    RTL_CRITICAL_SECTION_RC mCriticalSection;    // +0x440 per-console work lock
    void*                  mpDirectChunkData;    // +0x45C direct (non-FEC) chunk payload
    u32                    muDirectChunkLength;  // +0x460 direct chunk byte length
    bool                   mbHasDirectChunk;     // +0x464 a direct chunk is queued
    bool                   mbSignalRecovery;     // +0x465 QOS: recovery/data-loss flag
    bool                   mbSignalNewSequence;  // +0x466 QOS: sequence-change flag
    bool                   mbSignalDecodeError;  // +0x467 QOS: key-frame-request flag
    RemoteConsoleConfig*   mpConfig;             // +0x468 owning stream config/engine
    ListLink               mLink;                // +0x46C intrusive active/free list node
};

} // namespace XCAM
