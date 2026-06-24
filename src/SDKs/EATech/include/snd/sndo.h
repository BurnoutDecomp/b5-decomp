#ifndef SDKS_EATECH_SND_SNDO_H
#define SDKS_EATECH_SND_SNDO_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/snd/sndo.h  (DWARF home)
//
// EATech "Snd9" audio-middleware interfaces. MINIMAL reconstruction: the
// polymorphic IAemsSamplePlayer interface (sndo.h:1421) that the game's
// CgsSound::Playback::AemsRWSamplePlayer derives from is modelled, plus its
// InputSelector enum (sndo.h:1429) and the AemsPlayerInputAccessor input table
// helper (sndo.h, used by the AemsRWSampleFactory voice-creation path).
// IAemsSamplePlayer is the abstract base whose vtable shape the derived player's
// destructor and overrides depend on; its out-of-line destructor is defined in
// the sibling sndo.cpp (the X360 base scalar-deleting dtor @ 0x82681950).
//
// FLAG (MINIMAL vendor surface): the full sndo.h SDK declares many more types
// (IAemsSamplePlayerFactory, SNDREQUESTSTATUS, Util, ...). Only the slice the
// CgsSound AEMS player TUs need is homed here. The remainder is reconstructed
// when a TU that touches it is decompiled.
// ============================================================================

namespace Snd9
{
    // sndo.h:1421. Abstract sample-player interface a platform/middleware backend
    // implements. All hooks are pure virtual; the derived player overrides them.
    struct IAemsSamplePlayer
    {
        // sndo.h:1429. Per-input selector ids passed to SetInput.
        enum InputSelector
        {
            PLAYER_INPUT_PITCHMULT = 0,
            PLAYER_INPUT_TIMEMULT  = 1,
            PLAYER_INPUT_VOL       = 2,
            PLAYER_INPUT_AZIMUTH   = 3,
            PLAYER_INPUT_ELEVATION = 4,
            PLAYER_INPUT_FXWET0    = 5,
            PLAYER_INPUT_LOWPASS   = 6,
            PLAYER_INPUT_HIGHPASS  = 7,
            PLAYER_INPUT_DRYLEVEL  = 8,
            PLAYER_INPUT_USER_FIRST = 9,
            PLAYER_INPUT_USER_LAST  = 136,
        };

        // sndo.h:1491/1505/1518/1533/1549/1562 -- pure-virtual control surface.
        virtual void Release()                                  = 0;
        virtual void Pause()                                    = 0;
        virtual void Unpause()                                  = 0;
        virtual void SetInput(InputSelector aeSelector, int aiValue) = 0;
        virtual void SetAzimuth(int aiAzimuth, int* apLegacyAzimuths) = 0;
        virtual void GetOutputs(int aiNumOutputs, int* apValues) = 0;

    protected:
        // sndo.h:1566. Protected default ctor (subclass-only construction).
        IAemsSamplePlayer() {}
        // sndo.h:1567. Polymorphic base destructor. Defined OUT-OF-LINE in sndo.cpp
        // so it owns a real .cpp definition home -- the X360 base scalar-deleting
        // destructor @ 0x82681950 (stores the IAemsSamplePlayer vtable, then
        // conditionally operator-delete's). The body is empty (no members to tear
        // down); the vtable store + delete-tail is what the compiler emits.
        virtual ~IAemsSamplePlayer();
    };

    // sndo.h. AemsPlayerInputAccessor: a small read-only view over a player's
    // per-input table. Each input is a 12-byte record { u8 type @+0; <pad/flags> @+4;
    // s32 value @+8 }. The table is { u32 muCount @+0; Record* mpRecords @+4 }.
    //
    // Reconstructed BY NAME from the access pattern @ 0x82B6FE00 (GetValueByType):
    // the X360 walks mpRecords linearly, comparing each record's leading type byte to
    // the requested selector and returning its +8 value, or 0 when the table is empty
    // / the selector is not found. The intervening +4 word is not read by this TU, so
    // it is modelled as a named opaque field rather than fabricated.
    struct AemsPlayerInputAccessor
    {
        // One per-input record. 12 bytes; only meType (+0) and miValue (+8) are read.
        struct Input
        {
            u8  meType;   // +0  selector id (matched against the requested type)
            u8  mau8Pad[3]; // +1  (alignment to the +4 word; not read here)
            u32 mu32Flags;  // +4  per-input flags/state (not read by GetValueByType)
            s32 miValue;    // +8  the input value returned for a matching selector
        };

        u32    muCount;    // +0  number of valid Input records in mpRecords
        Input* mpRecords;  // +4  base of the per-input record array

        // 0x82B6FE00. Linear-search mpRecords for the record whose meType == aeType
        // and return its miValue; returns 0 when the table is empty or no record
        // matches (mirrors the X360 store-for-store: empty-table / not-found -> 0).
        s32 GetValueByType(s32 aeType) const;
    };

    // The X360 stride between Input records is 12 bytes (the search advances r10 by
    // 0xC and reads value at +8); lock the reconstructed layout to that.
    static_assert(sizeof(AemsPlayerInputAccessor::Input) == 12,
                  "AemsPlayerInputAccessor::Input must match the 12-byte X360 stride");

    // sndo.h. Abstract sample-player FACTORY interface. A platform/middleware backend
    // (e.g. the game's Snd9::AemsStandardSamplePlayerFactory / AemsRWSampleFactory)
    // derives from this and creates IAemsSamplePlayer instances. This is a separate
    // polymorphic interface from IAemsSamplePlayer above (its own vtable, off_820AB168).
    //
    // MINIMAL home: the ONLY recovered function for this interface is its vector-
    // deleting destructor thunk @ 0x82681998 (lis/addi off_820AB168 -> stw 0(this);
    // if (a2 & 1) operator delete(this); return this). That thunk resets the object to
    // THIS interface's OWN vtable as the last step of a derived player-factory's
    // teardown, so the destructor must be virtual and out-of-line (defined in sndo.cpp)
    // to give MSVC a home to emit the destructor + the 1-entry vtable.
    //
    // FLAG (DEFERRED virtual surface): the factory's create/release pure-virtual hooks
    // (DoCreateVoice / DoCreateContent / Create / ... -- exact signatures and vtable
    // slot order) are implemented by Snd9::AemsStandardSamplePlayerFactory and are NOT
    // recovered here. Only the destructor slot is attested by 0x82681998, so only it is
    // modelled (the project rule forbids fabricating the unverified virtual signatures).
    // The full factory surface is reconstructed when a TU that touches it is decompiled.
    struct IAemsSamplePlayerFactory
    {
    protected:
        // Subclass-only construction (abstract interface base).
        IAemsSamplePlayerFactory() {}
        // 0x82681998. Polymorphic base destructor; defined OUT-OF-LINE in sndo.cpp so
        // it owns a real .cpp definition home. Empty body (the interface holds no
        // members); the compiler emits the off_820AB168 vtable store + (for the
        // deleting variant) the operator-delete tail.
        virtual ~IAemsSamplePlayerFactory();
    };
} // namespace Snd9

#endif // SDKS_EATECH_SND_SNDO_H
