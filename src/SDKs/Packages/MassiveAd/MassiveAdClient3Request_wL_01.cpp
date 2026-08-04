
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestImpressionUpdate.h"

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::Parse @ 0x82BD9DA8
//
// Response walker: verify protocol version, expect response type 212, read the
// remaining-byte total, then walk the response fields. The one signature field
// (wire tag 30) is pulled back OUT of the buffer by ReadRemoveSignature so the
// closing HMAC check digests the payload without it; every other field must
// SkipField cleanly. Success requires exactly one signature block AND a
// matching HMAC.
//
// Register map (measured, 0x82BD9DA8..0x82BD9E80):
//   r31 = this, r28 = signature-block counter (u8: `clrlwi r11, r28, 24`),
//   r29 = post-header cursor snapshot (`lwz r29, 0x20(r31)` after ReadU32),
//   r30 = remaining-byte total from ReadU32,
//   r11 = consumed = mnPosition - r29, recomputed at the BOTTOM of every
//         iteration (`lwz r11, 0x20(r31) ; subf r11, r29, r11`) and compared
//         UNSIGNED against the total (`cmplw cr6, r11, r30 ; blt`).
// All the integer literals below are WIRE protocol values (field tags / a
// response-type byte / a serialised field size), not console offsets or
// strides -- nothing here is layout-dependent, and both members are reached BY
// NAME through the protected base (mnPosition at X360 +0x20, mpSignature at
// X360 +0x30 -- offsets quoted only in comments).
// ---------------------------------------------------------------------------
int CRequestImpressionUpdate::Parse()
{
    unsigned char lnSignatureBlocks = 0;      // r28 = 0
    mnPosition = 0;                           // stw r28, 0x20(r31)

    if (!ReadRemoveVerifyProtocolVersion() ||             // cmpwi r3, 0 ; beq
        static_cast<unsigned char>(ReadU8()) != 212)      // clrlwi 24 ; cmplwi 0xD4
    {
        return 0;                             // li r3, 0
    }

    unsigned int lnRemaining = ReadU32();     // r30 = remaining-byte total
    int lnBase = mnPosition;                  // r29 = post-header cursor

    // subf r11, r29, r29 => the first consumed value is 0; the loop test lives
    // at loc_82BD9E58 (`cmplw cr6, r11, r30 ; blt loc_82BD9DF8`).
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
                                              // ReadRemoveSignature removed from
                                              // the buffer; the tag byte itself
                                              // stays accounted for by the cursor)
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

} // namespace MassiveAdClient3
