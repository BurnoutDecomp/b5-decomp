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
// Parse @ 0x82BDC700 was parked on the `STUB(this, mpSignature, 20)` callee;
// that blocker is now GROUNDED (identically to the sibling
// CRequestImpressionUpdate::Parse in MassiveAdClient3Request_wL_01.cpp): every
// `bl STUB` in this function -- the format-string log calls AND the
// (this, mpSignature, 0x14) digest call -- targets the ONE address 0x82AD5078,
// whose entire body is a single `blr` shared by ~150 call sites across
// unrelated subsystems: an ICF-folded empty debug/trace hook compiled out of
// the retail build. The format-string sites are modelled by the declared
// MassiveLog vendor hook; the digest-dump site (no format string, no name
// survives the fold) is documented at its call site rather than modelled.
// ReadRemoveSignature is meanwhile declared in the committed base header and
// defined in MassiveAdClient3Request.cpp.
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
// CRequestExitZone::Parse @ 0x82BDC700
//
// Response walker: verify protocol version, expect response block ID 208, read
// the remaining-byte total, then walk the response fields. The one signature
// field (wire tag 30) is pulled back OUT of the buffer by ReadRemoveSignature
// so the closing HMAC check digests the payload without it; every other field
// must SkipField cleanly. Success requires exactly one signature block AND a
// matching HMAC. Unlike the sibling CRequestImpressionUpdate::Parse (same
// walker shape, block ID 212, no logging), this one traces every step through
// the MassiveLog hook (verbose level 5, error level 2).
//
// Register map (measured, 0x82BDC700..0x82BDC898):
//   r31 = this, r27 = signature-block counter (u8: `clrlwi r11, r27, 24`),
//   r30 = the block ID (`clrlwi r30, r11, 24` after ReadU8), then the
//         remaining-byte total from ReadU32,
//   r29 = post-header cursor snapshot (`lwz r29, 0x20(r31)` after ReadU32),
//   r28 = the hoisted "Reading HMAC Signature:" literal,
//   r11 = consumed = mnPosition - r29: `subf r11, r29, r29` (= 0) ahead of the
//         top-entry test, recomputed at the BOTTOM of every iteration
//         (`lwz r11, 0x20(r31) ; subf r11, r29, r11`) and compared UNSIGNED
//         against the total (`cmplw cr6, r11, r30 ; blt loc_82BDC7C4`).
// All the integer literals below are WIRE protocol values (field tags / a
// response-block ID / a serialised field size), not console offsets or strides
// -- nothing here is layout-dependent, and both members are reached BY NAME
// through the protected base (mnPosition at X360 +0x20, mpSignature at X360
// +0x30 -- offsets quoted only in comments).
// ---------------------------------------------------------------------------
int CRequestExitZone::Parse()
{
    unsigned char lnSignatureBlocks = 0;      // r27 = 0
    mnPosition = 0;                           // stw r27, 0x20(r31)
    MassiveLog(5, GetName(), "Reading ExitZone Response...");

    if (!ReadRemoveVerifyProtocolVersion())   // cmpwi r3, 0 ; beq
        return 0;

    unsigned char lnBlockID = static_cast<unsigned char>(ReadU8()); // clrlwi r30, r11, 24
    MassiveLog(5, GetName(), "Block ID: %d", lnBlockID);
    if (lnBlockID != 208)                     // cmplwi cr6, r30, 0xD0 ; beq
    {
        MassiveLog(2, GetName(), "Block ID of %d is not correct. Assuming its an error block.",
                   lnBlockID);
        return 0;
    }

    unsigned int lnRemaining = ReadU32();     // r30 = remaining-byte total
    MassiveLog(5, GetName(), "Block Length: %d", lnRemaining);
    int lnBase = mnPosition;                  // r29 = post-header cursor

    // Entry test: `subf r11, r29, r29` (consumed = 0) ; `cmplw cr6, r11, r30 ;
    // bge loc_82BDC834`; the bottom test at loc_82BDC824 re-derives consumed
    // from the live cursor.
    for (unsigned int lnConsumed = 0; lnConsumed < lnRemaining;
         lnConsumed = static_cast<unsigned int>(mnPosition - lnBase))
    {
        unsigned char lnTag = static_cast<unsigned char>(ReadU8()); // clrlwi r11, r4, 24
        if (lnTag == 30)                      // cmplwi cr6, r11, 0x1E
        {
            ReadRemoveSignature();
            MassiveLog(5, GetName(), "Reading HMAC Signature:");
            // bl 0x82AD5078 with (r3 = this, r4 = mpSignature [lwz r4,
            // 0x30(r31)], r5 = 20 [li r5, 0x14, the SHA1-HMAC digest length]).
            // MEASURED (see MassiveAdClient3Request_wL_01.cpp): the callee's
            // entire body is a single `blr` shared by ~150 call sites -- an
            // ICF-folded empty digest-dump debug hook compiled out of the
            // retail build. Attested no-op: no behaviour to reproduce, and the
            // fold destroyed the name, so the call is DOCUMENTED here rather
            // than modelled (inventing a hook name would be fabrication).
            lnRemaining -= 22;                // addi r30, r30, -0x16 (2-byte
                                              // length + 20-byte digest that
                                              // ReadRemoveSignature removed
                                              // from the buffer; the tag byte
                                              // stays accounted for by the
                                              // cursor)
            ++lnSignatureBlocks;              // addi r11, r11, 1 ; clrlwi r27, r11, 24
        }
        else if (!SkipField(lnTag))           // cmpwi r3, 0 ; beq -> fail
        {
            return 0;
        }
    }

    if (lnSignatureBlocks != 1)               // clrlwi r11, r27, 24 ; cmplwi cr6, r11, 1 ; bne
    {
        MassiveLog(2, GetName(), "Response does not contain all of the required fields.");
        return 0;
    }
    MassiveLog(5, GetName(), "Response contains all of the required fields.");

    if (!VerifyHMACSignature())               // cmpwi r3, 0 ; beq
        return 0;

    MassiveLog(5, GetName(), "Response successfully read and parsed.");
    return 1;                                 // li r3, 1
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
