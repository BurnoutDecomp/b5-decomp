#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestHeartbeat.h"

// ===========================================================================
// MassiveAdClient3::CRequestHeartbeat -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (no leak / DecFIGS).
//
// The wire tags are reproduced verbatim from the `li r4, <tag>` immediates in
// the disassembly, in asm order, and every scalar write here carries bPrepend=1
// (r5 = 1) -- the heartbeat block prepends its session/player fields onto the
// front of the request buffer. The vftable install the X360 ctor / vector
// deleting destructor perform (off_82186AD4) is modelled by the class virtuals,
// so no vftable store is written by hand.
//
// Parse @ 0x82BD86B8 was long parked on two collaborators; both are resolved
// in the committed tree, so its body now lives below:
//   - CRequestObject::ReadRemoveSignature @ 0x82BD02D0 is declared in the base
//     header and DEFINED in MassiveAdClient3Request.cpp (headless-dumped body).
//   - The `bl STUB` inside the signature branch targets 0x82AD5078, whose
//     entire body is a single `blr` shared by ~150 callers across unrelated
//     subsystems -- an ICF-folded debug/trace hook compiled out of retail. An
//     attested no-op, documented at the call site rather than modelled.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CRequestHeartbeat::CRequestHeartbeat @ 0x82BD8608
//
//   li r4, 0x85=133 ; addi r5, "RequestHeartbeat"
//   bl CRequestObject::CRequestObject(this, 133, "RequestHeartbeat")
//   stw off_82186AD4, 0(this)             -> install vftable (modelled by vtable)
// ---------------------------------------------------------------------------
CRequestHeartbeat::CRequestHeartbeat()
    : CRequestObject(133, "RequestHeartbeat")
{
}

// ---------------------------------------------------------------------------
// CRequestHeartbeat::~CRequestHeartbeat @ 0x82BD8660
//                                        (vector deleting destructor)
//
// No own teardown -- the X360 vector deleting destructor rewrites the vftable,
// chains ~CRequestObject, and conditionally frees through the base operator
// delete. Modelled by the empty virtual destructor.
// ---------------------------------------------------------------------------
CRequestHeartbeat::~CRequestHeartbeat()
{
}

// ---------------------------------------------------------------------------
// CRequestHeartbeat::GetRequestURL @ 0x82BD8650
//
//   lis r11, off_82F91D10@ha ; lwz r3, off_82F91D10@l(r11)  # "/impsrv/4/heartbeat"
//   blr
//
// off_82F91D10 is the .data pointer to the rodata endpoint string; returning the
// literal is byte-identical to the indirection the X360 performs.
// ---------------------------------------------------------------------------
const char* CRequestHeartbeat::GetRequestURL()
{
    return "/impsrv/4/heartbeat";
}

// ---------------------------------------------------------------------------
// CRequestHeartbeat::Parse @ 0x82BD86B8
//
// Response walker, the same shape as the committed sibling
// CRequestImpressionUpdate::Parse @ 0x82BD9DA8 (MassiveAdClient3Request_wL_01
// .cpp) with the heartbeat response-type byte 215: verify protocol version,
// expect response type 215 (0xD7), read the remaining-byte total, then walk the
// response fields. The one signature field (wire tag 30) is pulled back OUT of
// the buffer by ReadRemoveSignature so the closing HMAC check digests the
// payload without it; every other field must SkipField cleanly. Success
// requires exactly one signature block AND a matching HMAC.
//
// Register map (measured, 0x82BD86B8..0x82BD8790):
//   r31 = this, r28 = signature-block counter (u8: `clrlwi r11, r28, 24`),
//   r29 = post-header cursor snapshot (`lwz r29, 0x20(r31)` after ReadU32),
//   r30 = remaining-byte total from ReadU32,
//   r11 = consumed = mnPosition - r29, recomputed at the BOTTOM of every
//         iteration (`lwz r11, 0x20(r31) ; subf r11, r29, r11`, loc_82BD8760)
//         and compared UNSIGNED against the total (`cmplw cr6, r11, r30 ; blt`).
// All the integer literals below are WIRE protocol values (field tags / a
// response-type byte / a serialised field size), not console offsets or
// strides; both members are reached BY NAME through the protected base
// (mnPosition at X360 +0x20, mpSignature at X360 +0x30 -- offsets quoted only
// in comments).
// ---------------------------------------------------------------------------
int CRequestHeartbeat::Parse()
{
    unsigned char lnSignatureBlocks = 0;      // r28 = 0
    mnPosition = 0;                           // stw r28, 0x20(r31)

    if (!ReadRemoveVerifyProtocolVersion() ||             // cmpwi r3, 0 ; beq
        static_cast<unsigned char>(ReadU8()) != 215)      // clrlwi 24 ; cmplwi 0xD7
    {
        return 0;                             // li r3, 0
    }

    unsigned int lnRemaining = ReadU32();     // r30 = remaining-byte total
    int lnBase = mnPosition;                  // r29 = post-header cursor

    // subf r11, r29, r29 => the first consumed value is 0; the loop test lives
    // at loc_82BD8768 (`cmplw cr6, r11, r30 ; blt loc_82BD8708`).
    for (unsigned int lnConsumed = 0; lnConsumed < lnRemaining;
         lnConsumed = static_cast<unsigned int>(mnPosition - lnBase))
    {
        unsigned char lnTag = static_cast<unsigned char>(ReadU8()); // clrlwi r11, r4, 24
        if (lnTag == 30)                      // cmplwi cr6, r11, 0x1E
        {
            ReadRemoveSignature();
            // bl 0x82AD5078 with (r3 = this, r4 = mpSignature [lwz r4, 0x30(r31)],
            // r5 = 20 [li r5, 0x14, the SHA1-HMAC digest length]). MEASURED: the
            // callee's entire body is a single `blr` (4 bytes) shared by ~150
            // call sites across unrelated subsystems -- an ICF-folded empty
            // debug/trace hook compiled out of the retail build. Attested no-op:
            // there is no behaviour to reproduce and the fold destroyed the name,
            // so the call is DOCUMENTED here rather than modelled (inventing a
            // hook name would be fabrication).
            lnRemaining -= 22;                // addi r30, r30, -0x16 (2-byte
                                              // length + 20-byte digest that
                                              // ReadRemoveSignature removed
                                              // from the buffer; the tag byte
                                              // itself stays accounted for by
                                              // the cursor)
            ++lnSignatureBlocks;              // addi r11, r11, 1 ; clrlwi r28, r11, 24
        }
        else if (!SkipField(lnTag))           // cmpwi r3, 0 ; bne -> next iteration
        {
            return 0;
        }
    }

    if (lnSignatureBlocks != 1)               // clrlwi r11, r28, 24 ; cmplwi r11, 1 ; bne
        return 0;

    // cntlzw / extrwi 1,26 / xori 1 == a plain "!= 0" normalisation.
    return VerifyHMACSignature() != 0;
}

// ---------------------------------------------------------------------------
// CRequestHeartbeat::WriteHeartbeatRequest @ 0x82BD8798
//
// All writes bPrepend=1:
//   WriteU32(gnMassiveSessionID);  WriteU8(43);
//   WriteU32(gnMassivePlayerID);   WriteU8(42);
//   return FinishBaseBlock(214, 1, 1);
// ---------------------------------------------------------------------------
int CRequestHeartbeat::WriteHeartbeatRequest()
{
    WriteU32(gnMassiveSessionID, 1); // dword_8327F2CC ; li r5, 1
    WriteU8(43, 1);                  // li r4, 0x2B    ; li r5, 1
    WriteU32(gnMassivePlayerID, 1);  // dword_8327F2D0 ; li r5, 1
    WriteU8(42, 1);                  // li r4, 0x2A    ; li r5, 1
    return FinishBaseBlock(214, 1, 1); // li r4, 0xD6 ; li r5, 1 ; li r6, 1
}

// ---------------------------------------------------------------------------
// CRequestHeartbeat::CreateRequest @ 0x82BD8818
//
// Rejects a null builder (-1100). Otherwise chains the base CreateRequest(
// pBuilder, 256, 256), writes the block, marks the request ready to submit
// (SetStatus 2), and returns 0.
// ---------------------------------------------------------------------------
int CRequestHeartbeat::CreateRequest(CRequestBuilder* pBuilder)
{
    if (!pBuilder)
        return -1100;                // li r3, -0x44C

    CRequestObject::CreateRequest(pBuilder, 256, 256); // li r5, 0x100 ; li r6, 0x100
    WriteHeartbeatRequest();
    SetStatus(2);                    // li r4, 2
    return 0;
}

} // namespace MassiveAdClient3
