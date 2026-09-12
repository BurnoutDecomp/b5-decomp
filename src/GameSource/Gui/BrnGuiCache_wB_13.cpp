#include <cstring>   // std::memcpy (the event payload is adopted by value)

#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameSource/GameState/BrnGameStateTypes.h"           // BrnGameState::LandmarkIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h"        // SpecificGameModeEventInterface (COMPLETE: sizeof + layout)

// GuiCache map-event partfile. Reconstructed from the shipped console image.
//
// One function: the map-event EXIT producer, the arm of GuiCache::RecEvent that adopts a
// whole per-mode preset-event interface and rebuilds the online finish-point bitmask the
// sat-nav / crash-nav map counts its finish icons from. CGS_ASSERT is a no-op in this
// build (CgsAssert.h), matching the project convention for the console assert machinery.
//
// Plus the two PresetEvent landmark reads this body needs. PresetEvent is the cache's
// minimal slice of the same record the game-state interface calls
// SpecificGameModeEventInterface::Event; the two offsets below (+0x24 count, +0x00 + 2*i
// half-word) are the ones this function reads and they agree field-for-field with that
// record's own maLandmarkIndices / miNumLandmarks.

namespace BrnGui
{
    // The record's live landmark count (the console streams it as "GetNumLandmarks()" into
    // the indexed read's bound assert).
    s32 PresetEvent::GetNumLandmarks() const
    {
        return miNumLandmarks;
    }

    // One of the record's landmark indices by value, bounds-asserted into [0, count).
    // Asserts: BrnGameStateSharedIO.h:1929 / :1930 (both non-gating in this build; the
    // console streams the index and the live count into the second message).
    BrnGameState::LandmarkIndex PresetEvent::GetLandmark(s32 liIndex) const
    {
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        CGS_ASSERT(liIndex < miNumLandmarks, "liIndex < GetNumLandmarks()");

        return BrnGameState::LandmarkIndex(static_cast<s32>(
            static_cast<s16>(mau16LandmarkIndices[liIndex])));
    }

    // The map-event exit handler: RecEvent arm 194, BrnGui::GuiEventSpecificPresetRaces.
    // The queued payload is a verbatim 7704-byte copy of the game-state output buffer's own
    // SpecificGameModeEventInterface (the bridge copies and posts it whenever the buffer's
    // interface-is-valid flag is set), which is why it is spelled as that interface here.
    // Store-for-store:
    //
    //   1. assert the payload pointer (BrnGuiCache.cpp:4095 -- the console's own message
    //      still names the producer, OnlinePlay::HandleAllPreSetRacesEvent);
    //   2. adopt the WHOLE interface by value over maEventsStorage + mEventsCtorSentinel
    //      (one 7704-byte copy: the 175 x 44 element buffer AND the trailing count word,
    //      so the array's constructed-ness travels with the payload);
    //   3. zero the four doublewords of maOnlineFinishPointsMask;
    //   4. walk the adopted events and set bit i for each event whose LAST landmark --
    //      its finish point -- has not already been seen as the last landmark of an
    //      EARLIER event. That de-duplication is what makes the mask a set of distinct
    //      finish points rather than a set of events; GetNumOnlineFinishPoints popcounts
    //      it and GetOnlineFinishPoint turns a slot back into a landmark index.
    //
    // Faithful details that look like inefficiencies and are NOT: the live count is
    // re-read (with its array-constructed assert) on every iteration; the record for an
    // index is fetched TWICE, once for the count and once for the indexed landmark read;
    // and the inner scan re-fetches the earlier record twice per step as well. An event
    // with zero landmarks is skipped entirely and claims no bit.
    void GuiCache::HandleSpecificPreSetRacesEvent(
        const BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface* lpEvent)
    {
        typedef BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface
            PresetEventInterface;

        // The adopted block is exactly the cache's mEvents pair -- element buffer plus the
        // CgsArray count word that follows it.
        static_assert(sizeof(PresetEventInterface)
                          == sizeof(maEventsStorage) + sizeof(mEventsCtorSentinel),
                      "the preset-event payload is the cache's mEvents storage + count");

        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in OnlinePlay::HandleAllPreSetRacesEvent");   // cpp:4095

        // [FLAG PC bring-up guard] the console derefs immediately after that assert, which
        // is non-gating here, so a null payload would turn a reported miss into a crash on
        // the map-event exit path. Guard only; no invented behaviour.
        // DELETE-WHEN asserts gate.
        if (lpEvent == 0)
        {
            return;
        }

        std::memcpy(maEventsStorage, lpEvent, sizeof(maEventsStorage));
        std::memcpy(&mEventsCtorSentinel,
                    reinterpret_cast<const u8*>(lpEvent) + sizeof(maEventsStorage),
                    sizeof(mEventsCtorSentinel));

        maOnlineFinishPointsMask[0] = 0;
        maOnlineFinishPointsMask[1] = 0;
        maOnlineFinishPointsMask[2] = 0;
        maOnlineFinishPointsMask[3] = 0;

        for (s32 liIndex = 0; liIndex < GetNumPresetEvents(); ++liIndex)
        {
            const s32 liNumLandmarks = GetPresetEvent(liIndex)->GetNumLandmarks();
            if (liNumLandmarks == 0)
            {
                continue;
            }

            const s32 liFinishLandmark = static_cast<s32>(
                GetPresetEvent(liIndex)->GetLandmark(liNumLandmarks - 1));

            bool lbFinishAlreadyClaimed = false;
            for (s32 liEarlier = 0; liEarlier < liIndex; ++liEarlier)
            {
                // NOTE, deliberately preserved: the inner scan has NO zero-landmark skip
                // of its own, so an earlier event with an empty landmark set is read at
                // index -1. That is the shipped behaviour; the read stays inside the
                // cache object either way (the preceding record, or the word in front of
                // the storage), and the comparison simply cannot match a real finish
                // point. Do not "fix" it -- it changes which bits the mask carries.
                const s32 liEarlierNumLandmarks =
                    GetPresetEvent(liEarlier)->GetNumLandmarks();
                const s32 liEarlierFinishLandmark = static_cast<s32>(
                    GetPresetEvent(liEarlier)->GetLandmark(liEarlierNumLandmarks - 1));

                if (liFinishLandmark == liEarlierFinishLandmark)
                {
                    lbFinishAlreadyClaimed = true;
                    break;
                }
            }

            if (lbFinishAlreadyClaimed)
            {
                continue;
            }

            CGS_ASSERT(static_cast<u32>(liIndex) < 256u,
                       "Index < Number of bits");                    // CgsBitArray.h:222

            maOnlineFinishPointsMask[liIndex >> 6] |=
                static_cast<u64>(1) << (liIndex & 63);
        }
    }
}
