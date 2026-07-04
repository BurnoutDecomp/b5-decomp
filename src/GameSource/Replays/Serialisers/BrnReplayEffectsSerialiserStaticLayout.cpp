// BrnReplays::EffectsSerialiserStaticLayout -- the structured view of the effects replay
// serialiser's per-frame static buffer. Reconstructed from BURNOUT_X360_ARTIST.XEX. No DWARF
// for this TU: the layout is an opaque 4784-byte (0x12B0) buffer addressed by X360-attested
// byte offsets (the KI_OFF_* constants on the class); no field names are invented.
//
//   Clear            @ 0x82278738
//   GetBoostData     @ 0x82278918
//   GetBoostLocators @ 0x822789D0
//   WriteBoostData   @ 0x82278868
//   SetBoostLocators @ 0x82278B08
//   UpdateCarContact @ 0x822785F0
//   SetGlassEventData@ 0x8227ED88
//   GetCarContact    @ 0x8227EEE0
//   GetGlassEventData@ 0x82287A48

#include "GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "BrnCommonTypes.h"

#include <cstring>

namespace BrnReplays
{
    // @ 0x82278738 -- reset the whole static layout to cleared state.
    void EffectsSerialiserStaticLayout::Clear()
    {
        u8* const lpBase = reinterpret_cast<u8*>(this);

        // Header validity word + status bytes.
        *reinterpret_cast<s32*>(lpBase + 0x00) = -1;
        lpBase[0x04] = 0;
        lpBase[0x05] = 0;
        lpBase[0x06] = 0;
        lpBase[0x07] = 0;

        // Two 16-byte header vectors (stvx @ +0x10, +0x20).
        std::memset(lpBase + 0x10, 0, 0x20);

        // Counts.
        *reinterpret_cast<s32*>(lpBase + KI_OFF_NUM_GLASS_EVENTS) = 0;   // +0x30
        *reinterpret_cast<s32*>(lpBase + KI_OFF_NUM_CONTACTS)     = 0;   // +0x34

        // Header quadwords @ +0x38..+0x50.
        std::memset(lpBase + 0x38, 0, 0x20);

        // Glass-event records + side tables.
        std::memset(lpBase + KI_OFF_GLASS_EVENTS,     0, 0x200);   // +0x60,  8 * 0x40
        std::memset(lpBase + KI_OFF_GLASS_EVENT_VECS, 0, 0x80);    // +0x260, 8 * 0x10
        std::memset(lpBase + 0x2E0,                   0, 0x200);   // +0x2E0 glass-event tail block

        // Boost data: active bytes zeroed, boost handle/link quadwords set to -1.
        *reinterpret_cast<s64*>(lpBase + KI_OFF_BOOST_ACTIVE) = 0;   // +0x4E0 (u8[8])
        *reinterpret_cast<s64*>(lpBase + 0x4E8) = -1;
        *reinterpret_cast<s64*>(lpBase + 0x4F0) = -1;
        *reinterpret_cast<s64*>(lpBase + 0x4F8) = -1;
        *reinterpret_cast<s64*>(lpBase + 0x500) = -1;
        *reinterpret_cast<s64*>(lpBase + 0x508) = 0;
        *reinterpret_cast<s64*>(lpBase + 0x510) = 0;
        *reinterpret_cast<s64*>(lpBase + 0x518) = 0;
        *reinterpret_cast<s64*>(lpBase + 0x520) = 0;
        *reinterpret_cast<s64*>(lpBase + KI_OFF_BOOST_COUNTS) = 0;   // +0x528 (u8[8])

        // Boost-locator matrices (8 cars x 0x100).
        std::memset(lpBase + KI_OFF_BOOST_LOCATORS, 0, 0x800);       // +0x530

        // Race-car-contact side tables.
        std::memset(lpBase + KI_OFF_CONTACT_FIELD1, 0, 0x80);        // +0xD30
        std::memset(lpBase + KI_OFF_CONTACT_FIELD2, 0, 0x80);        // +0xDB0
        std::memset(lpBase + KI_OFF_CONTACT_FIELD3, 0, 0x80);        // +0xE30
        std::memset(lpBase + KI_OFF_CONTACT_VEC_A,  0, 0x200);       // +0xEB0
        std::memset(lpBase + KI_OFF_CONTACT_VEC_B,  0, 0x200);       // +0x10B0
    }

    // @ 0x82278918 -- read a race car's boost data (index < 8).
    void EffectsSerialiserStaticLayout::GetBoostData(u32 luRaceCarIndex, u8& lruActive,
                                                     f32& lrfValue, s32& lriBoostType)
    {
        CGS_ASSERT(luRaceCarIndex < 8,
                   "Invalid Race Car Index in EffectsSerialiser (GetBoostData)");

        const u8* const lpBase = reinterpret_cast<const u8*>(this);

        lruActive    = lpBase[KI_OFF_BOOST_ACTIVE + luRaceCarIndex];
        lriBoostType = *reinterpret_cast<const s32*>(lpBase + 0x4E8 + luRaceCarIndex * 4);
        lrfValue     = *reinterpret_cast<const f32*>(lpBase + 0x508 + luRaceCarIndex * 4);

        CGS_ASSERT(lriBoostType >= -1 && lriBoostType < 3,
                   "Invalid Boost Type in EffectsSerialiser (GetBoostData)");
    }

    // @ 0x822789D0 -- read the recorded boost locators for a race car (index < 8).
    void EffectsSerialiserStaticLayout::GetBoostLocators(u32 luRaceCarIndex, u8& lruCount,
                                                         Matrix44Affine* lpLocators)
    {
        CGS_ASSERT(luRaceCarIndex < 8,
                   "Invalid Race Car Index in EffectsSerialiser (GetBoostLocators)");

        const u8* const lpBase = reinterpret_cast<const u8*>(this);

        const u8 luCount = lpBase[KI_OFF_BOOST_COUNTS + luRaceCarIndex];
        lruCount = luCount;
        CGS_ASSERT(luCount < 16u,
                   "Too many Boost Locators in EffectsSerialiser (GetBoostLocators)");

        const u8* const lpSrc = lpBase + KI_OFF_BOOST_LOCATORS
                              + luRaceCarIndex * KI_BOOST_LOCATOR_STRIDE;
        std::memcpy(lpLocators, lpSrc, 4 * sizeof(Matrix44Affine));
    }

    // @ 0x82278868 -- record a race car's boost data into the static buffer (index < 8).
    int EffectsSerialiserStaticLayout::WriteBoostData(u32 luRaceCarIndex, u8 lu8ActiveFlag,
                                                      f32 lfBoostValue, s32 liBoostType)
    {
        u8* lpBase = reinterpret_cast<u8*>(this);

        CGS_ASSERT(luRaceCarIndex < 8,
                   "Invalid Race Car Index in EffectsSerialiser (SetBoostData)");
        CGS_ASSERT(liBoostType >= -1 && liBoostType < 3,
                   "Invalid Boost Type in EffectsSerialiser (SetBoostData)");

        lpBase[0x4E0 + luRaceCarIndex]                                = lu8ActiveFlag;
        *reinterpret_cast<s32*>(lpBase + 0x4E8 + 4 * luRaceCarIndex)  = liBoostType;
        *reinterpret_cast<f32*>(lpBase + 0x508 + 4 * luRaceCarIndex)  = lfBoostValue;

        return reinterpret_cast<intptr_t>(this);
    }

    // @ 0x82278B08 -- record the boost locators for a race car (index < 8).
    void EffectsSerialiserStaticLayout::SetBoostLocators(u32 luRaceCarIndex, u8 luActiveMask,
                                                         const Matrix44Affine* lpLocators)
    {
        CGS_ASSERT(luRaceCarIndex < 8,
                   "Invalid Race Car Index in EffectsSerialiser (SetBoostLocators)");
        CGS_ASSERT(luActiveMask < 16u,
                   "Too many Boost Locators in EffectsSerialiser (SetBoostLocators)");

        u8* const lpBase = reinterpret_cast<u8*>(this);

        lpBase[KI_OFF_BOOST_COUNTS + luRaceCarIndex] = luActiveMask;

        u8* const lpDst = lpBase + KI_OFF_BOOST_LOCATORS
                        + luRaceCarIndex * KI_BOOST_LOCATOR_STRIDE;
        std::memcpy(lpDst, lpLocators, 4 * sizeof(Matrix44Affine));
    }

    // @ 0x822785F0 -- mark the most-recently-added race-car contact active.
    //   Writes the caller's contact Vector4 (X360: VMX register v1) into its slot
    //   @ this+0x10A0+16*count and forces lane 0 to 1.0f. Asserts the count (+0x34) > 0.
    //   Faithful to the two 16-byte vrlimi128 splices in the asm; the contact vector arrives
    //   in a VMX register in the original, modelled here as an opaque 16-byte input.
    int EffectsSerialiserStaticLayout::UpdateCarContact(const void* lpContactVector)
    {
        u8* lpBase = reinterpret_cast<u8*>(this);
        const s32 liCount = *reinterpret_cast<const s32*>(lpBase + 0x34);

        CGS_ASSERT(liCount > 0,
                   "Trying to Update a non-existant RaceCar Contact in the EffectsSerialiser");

        // The contact Vector4 for the current (newest) contact slot.
        u8* lpSlot = lpBase + 0x10A0 + 16 * liCount;

        // Splice the caller's contact vector into the slot (asm keeps the slot's existing
        // lane 0 across this first store), then force lane 0 to 1.0f.
        std::memcpy(lpSlot, lpContactVector, 16);
        *reinterpret_cast<f32*>(lpSlot) = 1.0f;

        return reinterpret_cast<intptr_t>(this);
    }

    // @ 0x8227ED88 -- append one recorded glass-smash event into the static buffer.
    //   Silently no-ops once the buffer is full (glass-event count @ +0x30 == 8). Stores:
    //     - event id (a2)           -> this + 0x38 + 4*count
    //     - 64-byte fragment matrix -> this + 0x60 + 64*count  (copied FROM lpMatrix)
    //     - four glass-event Vector4 slots @ this+0x2E0+64*count, this+0x2F0+64*count,
    //       this + ((count+12)<<6), this+0x310+64*count  (assembled from the caller's
    //       vector args), then ++count.
    //   UNCERTAIN (confidence low): the four Vector4 writes are hand-scheduled vrlimi128
    //   lane-splices reading multiple caller vector register arguments; the per-lane field
    //   semantics are not attested. The stores below reproduce the ATTESTED byte regions,
    //   store order and copy DIRECTIONS, treating the caller vectors as opaque 16-byte inputs.
    int EffectsSerialiserStaticLayout::SetGlassEventData(int liEventId,
                                                         const void* lpMatrix,
                                                         const void* lpVec1,
                                                         const void* lpVec2,
                                                         const void* lpVec3,
                                                         const void* lpVec4)
    {
        u8* lpBase = reinterpret_cast<u8*>(this);
        s32& lriCount = *reinterpret_cast<s32*>(lpBase + 0x30);

        // Buffer holds at most 8 glass events per frame; drop once full.
        if (static_cast<u32>(lriCount) == 8u)
        {
            return reinterpret_cast<intptr_t>(this);
        }

        const u32 luCount = static_cast<u32>(lriCount);

        // Event id table: this + 0x38 + 4*count = a2.
        *reinterpret_cast<s32*>(lpBase + 0x38 + 4 * luCount) = liEventId;

        // 64-byte fragment matrix copied FROM the caller into the record body
        // (asm: lvx128 from a3 -> stvx128 into this + 0x60 + 64*count).
        u8* lpRecord = lpBase + 0x60 + (luCount << 6);
        std::memcpy(lpRecord, lpMatrix, 64);

        // Four glass-event Vector4 slots (record base is this + 64*count). Opaque 16-byte
        // writes at the attested offsets from the caller's vector args.
        u8* lpSlotBase   = lpBase + (luCount << 6);
        u8* lpSlotMirror = lpBase + ((luCount + 12) << 6);
        std::memcpy(lpSlotBase   + 0x2E0, lpVec1, 16);
        std::memcpy(lpSlotBase   + 0x2F0, lpVec2, 16);
        std::memcpy(lpSlotMirror + 0x00,  lpVec3, 16);
        std::memcpy(lpSlotBase   + 0x310, lpVec4, 16);

        ++lriCount;
        return reinterpret_cast<intptr_t>(this);
    }

    // @ 0x8227EEE0 -- read a race-car contact record (index < contact count).
    void EffectsSerialiserStaticLayout::GetCarContact(s32 liContactIndex, bool* lpbActive,
                                                      u32* lpuField1, u32* lpuField2, u16* lpaField3,
                                                      Vector4* lpVecA, Vector4* lpVecB)
    {
        const u8* const lpBase = reinterpret_cast<const u8*>(this);

        CGS_ASSERT(liContactIndex < *reinterpret_cast<const s32*>(lpBase + KI_OFF_NUM_CONTACTS),
                   "Requested Contact Number is out of range");

        *lpuField1 = *reinterpret_cast<const u32*>(lpBase + KI_OFF_CONTACT_FIELD1 + liContactIndex * 4);
        *lpuField2 = *reinterpret_cast<const u32*>(lpBase + KI_OFF_CONTACT_FIELD2 + liContactIndex * 4);

        const u16* const lpField3 =
            reinterpret_cast<const u16*>(lpBase + KI_OFF_CONTACT_FIELD3 + liContactIndex * 4);
        lpaField3[0] = lpField3[0];
        lpaField3[1] = lpField3[1];

        // Two contact quadwords copied verbatim (single lvx/stvx each).
        const u8* const lpVecASrc = lpBase + KI_OFF_CONTACT_VEC_A + liContactIndex * 0x10;
        const u8* const lpVecBSrc = lpBase + KI_OFF_CONTACT_VEC_B + liContactIndex * 0x10;
        std::memcpy(lpVecA, lpVecASrc, sizeof(Vector4));
        std::memcpy(lpVecB, lpVecBSrc, sizeof(Vector4));

        // Active flag = the w lane of the second contact vector == 1.0f
        // (vspltw lane3 + vcmpeqfp against vcfsx(1); CR6 bit extracted).
        const f32 lfActiveLane = *reinterpret_cast<const f32*>(lpVecBSrc + 0x0C);
        *lpbActive = (lfActiveLane == 1.0f);
    }

    // @ 0x82287A48 -- read a glass-smash event record (index < glass-event count).
    void EffectsSerialiserStaticLayout::GetGlassEventData(u8 luEventIndex, u32* lpuField,
                                                          void* lpBlock0, void* lpBlock1,
                                                          void* lpBlock2, void* lpBlock3,
                                                          void* lpBlock4, void* lpVpermPacked,
                                                          void* lpVec260)
    {
        const u8* const lpBase = reinterpret_cast<const u8*>(this);

        CGS_ASSERT(luEventIndex < *reinterpret_cast<const u32*>(lpBase + KI_OFF_NUM_GLASS_EVENTS),
                   "Invalid Event Number in EffectsSerialiser");

        const u32 luEvent = luEventIndex;
        const u8* const lpEvent  = lpBase + (luEvent << 6);
        const u8* const lpEvent2 = lpBase + ((luEvent + 12) << 6);

        // 64-byte primary glass-event record @ (event<<6)+0x60.
        std::memcpy(lpBlock0, lpEvent + 0x60, 0x40);

        // Per-event word @ +0x38 (stride 4).
        *lpuField = *reinterpret_cast<const u32*>(lpBase + 0x38 + luEvent * 4);

        // Four 16-byte sub-blocks copied verbatim (lvx/stvx).
        std::memcpy(lpBlock1, lpEvent  + 0x2E0, 0x10);   // r7 block
        std::memcpy(lpBlock2, lpEvent  + 0x2F0, 0x10);   // r8 block
        std::memcpy(lpBlock3, lpEvent2 + 0x00,  0x10);   // ((event+12)<<6) block
        std::memcpy(lpBlock4, lpEvent  + 0x310, 0x10);   // r30 block

        // Per-event side vector @ +0x260 (stride 0x10) -> arg_5C (last pointer).
        std::memcpy(lpVec260, lpBase + KI_OFF_GLASS_EVENT_VECS + luEvent * 0x10, 0x10);

        // NOTE (UNATTESTED SEMANTICS): the X360 also builds a packed vector via
        //   vperm(block1=+0x2E0, block2=+0x2F0, rodata mask) then vrlimi128(with block3)
        // and stores it to the arg_54 out-pointer (lpVpermPacked). The exact lane algebra +
        // permute mask (rodata unk_82CDB450) are not reconstructable in scope; reproduced
        // here as an opaque byte handoff of the assembled 16-byte result (flagged low-confidence).
        std::memcpy(lpVpermPacked, lpEvent + 0x2E0, 0x10);
    }
}
