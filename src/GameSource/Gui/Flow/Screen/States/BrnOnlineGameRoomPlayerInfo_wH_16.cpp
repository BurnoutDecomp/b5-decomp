// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 16: the three remaining page
// teardowns (challenge list, pause menu, settings page).
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   UnloadChallengeResources @ 0x8249A170 (assert fires at cpp:4161)
//   UnloadPauseResources     @ 0x8249A348 (assert fires at cpp:4203)
//   UnloadSettingsResources  @ 0x8249A410 (assert fires at cpp:4224)
//
// All three share one shape: unmount the page's apt movie, assert the cache pointer,
// then hand that page's resource tuples back to the cache. UnloadChallengeResources
// additionally posts the ticker-clear record FIRST, before the movie comes down.
//
// The class shape, the member map and the rodata tuple tables live in the wave-H
// keystone scaffold BrnOnlineGameRoomPlayerInfo.{h,cpp}; the sibling bodies land in the
// other BrnOnlineGameRoomPlayerInfo_wH_XX.cpp partfiles.
//
// X360 offsets/sizes appear only in comments -- the host layout is name-based. In
// particular no console record size is written as a literal here:
//   * the {8,18,12,"",3} play-apt-movie record (X360 size 20, channel 41) is not built
//     by hand at all -- the faithful de-inlined form is the real method
//     CgsGui::StateInterface::PlayAptMovie(name, level), which owns that sizeof (the
//     record carries a pointer, so its host size differs). Same de-inlining the
//     committed siblings wH_15 / BrnCarSelectMain_wG_01 use.
//   * the ticker-clear record's X360 size 16 is emitted as sizeof(the wire view).
//
// HEADER-CLASH NOTE (spec's idiom section; BrnInGame.cpp / BrnCarSelectMain_wG_01
// precedent): the id-536 payload's typed home is BrnGuiDemangledEventTypes.h
// (GuiEventTickerClearMessages, 2 raw bytes), which HARD-COLLIDES with
// BrnGuiEventTypeDefs.h -- and the latter is pulled in transitively by the class
// header via BrnGuiCache.h. The record therefore gets a file-local wire view.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie / GetOutputEventQueue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache::UnloadResources

namespace BrnGui
{
    namespace
    {
        // The empty movie name PlayAptMovie is handed when a screen's apt movie is
        // unmounted (X360 unk_820046A7, the zero-length rodata string).
        const char KAC_EMPTY_MOVIE[] = "";

        // The apt movie level the screens mount/unmount on (X360 `li r11, 3`).
        const s32 KI_APT_MOVIE_LEVEL = 3;

        // The out-queue channel the ticker-clear record goes out on (X360 `li r5, 0x28`;
        // 40 == GuiEventOut).
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // ---- out-queue wire record: the id-536 ticker-clear command -------------------
        // X360 @0x8249A184..0x8249A1C8 stack-builds { 2, 536, 12 } then stores a zeroed
        // halfword at +0x0C and calls AddEvent(&record, 40, 16). The two payload bytes are
        // written individually into a 2-byte temp and copied out as one halfword -- i.e.
        // the GuiEventWrapper<T,40> copy of a 2-byte payload -- so they are modelled as the
        // two bytes they are, not as a 16-bit word. The trailing two bytes of the record
        // are never written by the X360 (modelled zeroed). Typed home:
        // BrnGuiDemangledEventTypes.h's GuiEventTickerClearMessages (id 536, size 2); see
        // the header-clash note above for why it is re-declared file-locally.
        // The identical record (with payload bytes { 1, 0 }) appears in the committed
        // BrnCarSelectMain_wG_01.cpp as GuiEventCarSelectAccept16.
        struct GuiEventTickerClearMessages16 : public CgsGui::GuiEvent<536>
        {
            u8 mu8Byte0;   // +0x0C (X360 stores 0)
            u8 mu8Byte1;   // +0x0D (X360 stores 0)
            u8 maPad[2];   // +0x0E (untouched by the X360; modelled zeroed)

            GuiEventTickerClearMessages16(u8 lu8Byte0, u8 lu8Byte1)
                : CgsGui::GuiEvent<536>(2, 12), mu8Byte0(lu8Byte0), mu8Byte1(lu8Byte1)
            {
                maPad[0] = maPad[1] = 0;
            }
        };
    }

    // ---------------------------------------------- UnloadChallengeResources @ 0x8249A170
    // Tear the freeburn-challenge page down. Unlike its two siblings this one first pushes
    // the ticker-clear command out on the GUI-out channel (the challenge page owns the
    // ticker messages it posted), then unmounts the apt movie via the inlined
    // CgsGui::GuiEventPlayAptMovie record { 8, 18, 12, "", 3 } on the view-state channel,
    // then hands the challenge resource tuples back to the cache.
    void OnlineGameRoomPlayerInfo::UnloadChallengeResources()
    {
        GuiEventTickerClearMessages16 lClearTicker(0, 0);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lClearTicker), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lClearTicker)));   // X360 record size 16

        mpStateInterface->PlayAptMovie(KAC_EMPTY_MOVIE, KI_APT_MOVIE_LEVEL);

        // X360 BrnOnlineGameRoomPlayerInfo.cpp:4161.
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        // Re-read (not the asserted copy): the X360 reloads the member and re-tests it.
        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(maChallengeResourceTuplesToLoad,
                                        static_cast<u32>(miNumChallengeResourcesToLoad));
        }
    }

    // ------------------------------------------------- UnloadPauseResources @ 0x8249A348
    // Tear the pause menu down: unmount its apt movie, then hand the pause resource tuples
    // back to the cache.
    void OnlineGameRoomPlayerInfo::UnloadPauseResources()
    {
        mpStateInterface->PlayAptMovie(KAC_EMPTY_MOVIE, KI_APT_MOVIE_LEVEL);

        // X360 BrnOnlineGameRoomPlayerInfo.cpp:4203.
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(maPauseResourceTuplesToLoad,
                                        static_cast<u32>(miNumPauseResourcesToLoad));
        }
    }

    // ---------------------------------------------- UnloadSettingsResources @ 0x8249A410
    // Tear the settings page down: unmount its apt movie, then hand the settings resource
    // tuples back to the cache.
    void OnlineGameRoomPlayerInfo::UnloadSettingsResources()
    {
        mpStateInterface->PlayAptMovie(KAC_EMPTY_MOVIE, KI_APT_MOVIE_LEVEL);

        // X360 BrnOnlineGameRoomPlayerInfo.cpp:4224.
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (mpGuiCache != 0)
        {
            mpGuiCache->UnloadResources(maSettingsResourceTuplesToLoad,
                                        static_cast<u32>(miNumSettingsResourcesToLoad));
        }
    }
}
