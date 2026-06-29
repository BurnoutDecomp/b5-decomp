#ifndef GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYEFFECTSSERIALISER_H
#define GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYEFFECTSSERIALISER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine

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
// BrnReplayEffectsSerialiser.cpp; only the slice the effects state machines call is
// declared here. GROW additively; do NOT fork the type. NOTE: the .cpp currently
// keeps a file-local EffectsSerialiser shape for its own bodies -- migrate it onto
// this header when that TU is next revisited.
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
        // X360 0x822789D0. Read the recorded boost locators for a race car: writes the
        // live locator count into lruCount (asserts < 16) and copies the 4 boost-locator
        // world transforms into lpLocators[0..3]. luRaceCarIndex asserted < 8.
        void GetBoostLocators(u32 luRaceCarIndex, u8& lruCount, Matrix44Affine* lpLocators);

        // X360 0x82278B08. Record the boost locators for a race car: stores luActiveMask
        // (the live-locator bit count, asserted < 16) and copies the 4 boost-locator world
        // transforms from lpLocators[0..3] back into the buffer. luRaceCarIndex asserted < 8.
        void SetBoostLocators(u32 luRaceCarIndex, u8 luActiveMask, const Matrix44Affine* lpLocators);
    };

    // The effects replay-serialiser channel. Only the slice the effects state machines
    // reach (the current EMode + GetStaticLayout) is declared; the rest (BaseSerialiser
    // derivation, Construct / Read / Write) lives in the .cpp.
    class EffectsSerialiser
    {
    public:
        // BaseSerialiser EMode (DWARF BrnReplayBaseSerialiser.h:54). The leading word
        // of the serialiser object; the effects state machines branch on whether it is
        // RECORDING (1/2/3) or PLAYING (4/5/6).
        enum EMode
        {
            E_MODE_IDLE                = 0,
            E_MODE_RECORDING_PREPARING = 1,
            E_MODE_RECORDING           = 2,
            E_MODE_RECORDING_STALLED   = 3,
            E_MODE_PLAYING_PREPARING   = 4,
            E_MODE_PLAYING             = 5,
            E_MODE_PLAYING_STALLED     = 6,
        };

        // The current serialiser mode (the object's leading word).
        EMode GetMode() const { return meMode; }

        // X360 0x82278698. The serialiser's per-frame static buffer, viewed as the
        // structured boost-locator layout (asserts the buffer is large enough).
        EffectsSerialiserStaticLayout* GetStaticLayout();

    protected:
        EMode meMode;   // +0x00 (BaseSerialiser mode)
    };
}

#endif // GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYEFFECTSSERIALISER_H
