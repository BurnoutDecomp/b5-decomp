#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestExitZone.h"

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"

// ===========================================================================
// MassiveAdClient3::CRequestExitZone -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (no leak / DecFIGS).
//
// The wire tags are reproduced verbatim from the `li r4, <tag>` immediates in
// the disassembly, in asm order. The vftable install the X360 ctor / vector
// deleting destructor perform (off_82187850) is modelled by the class virtuals,
// so no vftable store is written by hand. The verbose-trace call sites the X360
// renders as Hex-Rays `STUB(5, mpcName, fmt, ...)` are the MassiveLog hook
// (r3 = level 5, r4 = the base name at +0x0C, r5 = format, then the args).
//
// Parse @ 0x82BDC700 is intentionally NOT defined here (BLOCKED): after
// CRequestObject::ReadRemoveSignature its body calls an un-homed, un-named
// function (Hex-Rays `STUB(this, mpSignature, 20)`) and ReadRemoveSignature is
// un-attested in the committed base header. See the header.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CRequestExitZone::CRequestExitZone @ 0x82BDC488
//
//   bl CRequestObject::CRequestObject(this, 0x43=67, "RequestExitZone")
//   stw off_82187850, 0(this)             -> install vftable (modelled by vtable)
// ---------------------------------------------------------------------------
CRequestExitZone::CRequestExitZone()
    : CRequestObject(67, "RequestExitZone")
{
}

// ---------------------------------------------------------------------------
// CRequestExitZone::~CRequestExitZone @ 0x82BDC4E0 (vector deleting destructor)
//
// No own teardown -- the X360 vector deleting destructor rewrites the vftable,
// chains ~CRequestObject, and conditionally frees through the base operator
// delete. Modelled by the empty virtual destructor.
// ---------------------------------------------------------------------------
CRequestExitZone::~CRequestExitZone()
{
}

// ---------------------------------------------------------------------------
// CRequestExitZone::WriteExitZoneRequest @ 0x82BDC538
//
// Block body (all appends, bPrepend = 0):
//   [71,string zoneName]
//   [42,u32 gnMassivePlayerID]
//   [43,u32 gnMassiveSessionID]
//   if (size && time && items):
//     [16,u32 size][17,u32 time][18,u16 items]
//   FinishBaseBlock(207, 1, 1);   return 1;
// ---------------------------------------------------------------------------
int CRequestExitZone::WriteExitZoneRequest(const char* pcZoneName, int nBandwidthTotalSize,
                                           int nBandwidthTotalTime,
                                           unsigned short sBandwidthTotalItems)
{
    MassiveLog(5, GetName(), "Writing Request...");

    WriteU8(71, 0);                  // li r4, 0x47
    WriteString(pcZoneName, 0);
    MassiveLog(5, GetName(), "Writing Zone Name: %s", pcZoneName);

    WriteU8(42, 0);                  // li r4, 0x2A
    WriteU32(gnMassivePlayerID, 0);  // dword_8327F2D0
    MassiveLog(5, GetName(), "Writing Massive Player ID: %d", gnMassivePlayerID);

    WriteU8(43, 0);                  // li r4, 0x2B
    WriteU32(gnMassiveSessionID, 0); // dword_8327F2CC
    MassiveLog(5, GetName(), "Writing Massive Session ID: %d", gnMassiveSessionID);

    if (nBandwidthTotalSize && nBandwidthTotalTime && sBandwidthTotalItems)
    {
        WriteU8(16, 0);              // li r4, 0x10
        WriteU32(nBandwidthTotalSize, 0);
        MassiveLog(5, GetName(), "Writing Bandwidth Total Size: %d", nBandwidthTotalSize);

        WriteU8(17, 0);              // li r4, 0x11
        WriteU32(nBandwidthTotalTime, 0);
        MassiveLog(5, GetName(), "Writing Bandwidth Total Time: %d", nBandwidthTotalTime);

        WriteU8(18, 0);              // li r4, 0x12
        WriteU16(sBandwidthTotalItems, 0);
        MassiveLog(5, GetName(), "Writing Bandwidth Total Items: %d", sBandwidthTotalItems);
    }

    FinishBaseBlock(207, 1, 1);      // li r4, 0xCF ; li r5, 1 ; li r6, 1
    return 1;
}

// ---------------------------------------------------------------------------
// CRequestExitZone::CreateRequest @ 0x82BDC8A0
//
// Rejects a null builder or a null zone name (-1100). Otherwise chains the base
// CreateRequest(pBuilder, 256, 256), writes the block, marks the request ready
// to submit (SetStatus 2), logs, and returns 0.
// ---------------------------------------------------------------------------
int CRequestExitZone::CreateRequest(CRequestBuilder* pBuilder, const char* pcZoneName,
                                    int nBandwidthTotalSize, int nBandwidthTotalTime,
                                    unsigned short sBandwidthTotalItems)
{
    if (!pBuilder || !pcZoneName)
        return -1100;                // li r3, -0x44C

    CRequestObject::CreateRequest(pBuilder, 256, 256); // li r5, 0x100 ; li r6, 0x100
    WriteExitZoneRequest(pcZoneName, nBandwidthTotalSize, nBandwidthTotalTime,
                         sBandwidthTotalItems);
    SetStatus(2);                    // li r4, 2
    MassiveLog(5, GetName(), "Created Request Zone=%s", pcZoneName);
    return 0;
}

} // namespace MassiveAdClient3
