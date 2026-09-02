#ifndef GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYEFFECTSSERIALISER_H
#define GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYEFFECTSSERIALISER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"   // BrnReplays::BaseSerialiser (the real base; EMode)

// ============================================================================
// GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.h
//
// BrnReplays::EffectsSerialiser -- the replay serialiser channel for the effects
// system, and BrnReplays::EffectsSerialiserStaticLayout -- the structured view of
// its per-frame static buffer. The effects state machines (boost / jump) write the
// per-car effect locators into the static layout while RECORDING and read them back
// while PLAYING, so the replay reproduces the same particle anchors.
//
// SOURCE-OF-TRUTH: the GetBoostLocators / SetBoostLocators bodies are attested in
// the ARTIST asm (0x822789D0 / 0x82278B08); their signatures + the per-car bounds
// (race-car index < 8, boost-locator count < 16) come straight from those bodies.
// GetStaticLayout (0x82278698) returns the serialiser's static buffer reinterpreted
// as the layout. The full EffectsSerialiser body (Construct / Read / Write) lives in
// BrnReplayEffectsSerialiser.cpp against THIS declaration (the .cpp's former
// file-local shape is gone -- see the class note below). GROW additively; do NOT
// fork the type.
// ============================================================================

namespace BrnReplays
{
    // The structured static-buffer view holding each active race car's recorded
    // boost-locator set (count + 4 world transforms per car). Accessed BY NAME via
    // the Get/SetBoostLocators accessors (the raw buffer offsets are an internal
    // serialised layout, owned by these accessors -- not exposed as members here).
    class EffectsSerialiserStaticLayout
    {
    public:
        // ---- Buffer geometry (X360-attested) -------------------------------
        // The static layout is an opaque 4784-byte (0x12B0) serialised buffer addressed by
        // these X360-attested byte offsets; no field names are invented (no DWARF for this TU).
        static const s32 KI_STATIC_LAYOUT_SIZE = 4784;        // 0x12B0 (Construct static size)

        static const s32 KI_OFF_NUM_GLASS_EVENTS = 0x30;      // glass-event count
        static const s32 KI_OFF_NUM_CONTACTS     = 0x34;      // car-contact count

        static const s32 KI_OFF_GLASS_EVENTS     = 0x60;      // 8 * 0x40
        static const s32 KI_OFF_GLASS_EVENT_VECS = 0x260;     // 8 * 0x10

        static const s32 KI_OFF_BOOST_ACTIVE   = 0x4E0;       // u8[8]
        static const s32 KI_OFF_BOOST_COUNTS   = 0x528;       // u8[8] (locator counts)

        static const s32 KI_OFF_BOOST_LOCATORS   = 0x530;     // 8 cars * 0x100
        static const s32 KI_BOOST_LOCATOR_STRIDE = 0x100;     // 4 * Matrix44Affine

        static const s32 KI_OFF_CONTACT_FIELD1 = 0xD30;       // u32[32]
        static const s32 KI_OFF_CONTACT_FIELD2 = 0xDB0;       // u32[32]
        static const s32 KI_OFF_CONTACT_FIELD3 = 0xE30;       // u16[2][32] (stride 4)
        static const s32 KI_OFF_CONTACT_VEC_A  = 0xEB0;       // Vector4[32]
        static const s32 KI_OFF_CONTACT_VEC_B  = 0x10B0;      // Vector4[32]

        // ---- Accessors (all X360-attested) ---------------------------------

        // X360 0x82278738. Reset the whole static layout to cleared state.
        void Clear();

        // X360 0x82278918. Read a race car's boost data (index < 8): active byte,
        // value float, and boost type (asserted in [-1,3)).
        void GetBoostData(u32 luRaceCarIndex, u8& lruActive, f32& lrfValue, s32& lriBoostType);

        // X360 0x822789D0. Read the recorded boost locators for a race car: writes the
        // live locator count into lruCount (asserts < 16) and copies the 4 boost-locator
        // world transforms into lpLocators[0..3]. luRaceCarIndex asserted < 8.
        void GetBoostLocators(u32 luRaceCarIndex, u8& lruCount, Matrix44Affine* lpLocators);

        // X360 0x82278B08. Record the boost locators for a race car: stores luActiveMask
        // (the live-locator bit count, asserted < 16) and copies the 4 boost-locator world
        // transforms from lpLocators[0..3] back into the buffer. luRaceCarIndex asserted < 8.
        void SetBoostLocators(u32 luRaceCarIndex, u8 luActiveMask, const Matrix44Affine* lpLocators);

        // X360 0x82278868. Record a race car's boost data into the static buffer:
        // active flag (+0x4E0+index), boost type (+0x4E8+4*index), value (+0x508+4*index).
        // luRaceCarIndex asserted < 8, liBoostType asserted in [-1,3). Read back by GetBoostData.
        int WriteBoostData(u32 luRaceCarIndex, u8 lu8ActiveFlag, f32 lfBoostValue, s32 liBoostType);

        // X360 0x8227EEE0. Read a race-car contact record (index < contact count): two field
        // words, two u16s, two contact vectors (single quadwords, hence Vector4 not
        // Matrix44Affine), and the active flag (vecB.w == 1.0f).
        void GetCarContact(s32 liContactIndex, bool* lpbActive, u32* lpuField1, u32* lpuField2,
                           u16* lpaField3, Vector4* lpVecA, Vector4* lpVecB);

        // X360 0x822785F0. Mark the newest race-car contact active: write the caller's contact
        // Vector4 (X360 VMX arg) into its slot (+0x10A0+16*count) and force lane 0 to 1.0f.
        // Asserts the car-contact count (+0x34) > 0. The pointer is an opaque 16-byte input
        // standing in for the X360 VMX register argument.
        int UpdateCarContact(const void* lpContactVector);

        // X360 0x82287A48. Read a glass-smash event record (index < glass-event count).
        // UNCERTAIN: out-param field semantics + the vperm/vrlimi packed vector (lpVpermPacked,
        // mask rodata unk_82CDB450) are not attested; reconstructed as opaque quadword copies.
        void GetGlassEventData(u8 luEventIndex, u32* lpuField, void* lpBlock0, void* lpBlock1,
                               void* lpBlock2, void* lpBlock3, void* lpBlock4,
                               void* lpVpermPacked, void* lpVec260);

        // X360 0x8227ED88. Append a recorded glass-smash event into the static buffer
        // (event id + a 64-byte fragment matrix copied FROM lpMatrix + four glass-event
        // Vector4s assembled from the caller's vector args), bumping the glass-event count
        // (+0x30). No-ops once the buffer holds 8 events. NOTE: the four Vector4 writes are
        // hand-scheduled vrlimi128 lane-splices over multiple VMX register args in the original;
        // the pointer params here are opaque 16-byte inputs (data-movement model only).
        int SetGlassEventData(int liEventId, const void* lpMatrix, const void* lpVec1,
                              const void* lpVec2, const void* lpVec3, const void* lpVec4);
    };

    // The effects replay-serialiser channel (DWARF BrnReplayEffectsSerialiser.h; a
    // BaseSerialiser leaf). 2026-09-02 (tyre-mark wave): this used to be a 4-byte
    // `{ EMode meMode; }` SLICE while the .cpp kept a SECOND, private 3-member
    // BaseSerialiser + EffectsSerialiser pair for its own bodies -- an ODR fork
    // that put mpStaticBuffer at +0x04 in one definition and +0x18 in the other.
    // EffectsModule embeds the real object BY VALUE (X360 +0x2F550, 0x58 bytes,
    // EffectsModule::Construct @0x8228FE98 calls EffectsSerialiser::Construct on it
    // and Update @0x8229EC28 early-outs on a null GetStaticLayout()), so the type is
    // now the real derivation and the .cpp's bodies run on it. EMode / GetMode come
    // from BaseSerialiser (the same enum, the same leading word).
    class EffectsSerialiser : public BaseSerialiser
    {
    public:
        // X360 0x8264C9D8: BaseSerialiser::Construct(8, 0, 4816, 4784, "Effects", 0).
        s32 Construct();

        // X360 0x82278698. The serialiser's per-frame static buffer, viewed as the
        // structured boost-locator layout (asserts the buffer is large enough).
        EffectsSerialiserStaticLayout* GetStaticLayout();

        // X360 0x82650508 / 0x82650600 -- the per-update transfer of the static layout
        // (write it out while RECORDING, read it in while PLAYING).
        s32 Read();
        s32 Write();
    };
}

#endif // GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYEFFECTSSERIALISER_H
