#include "GameSource/Gui/SatNav/BrnEventIconManager.h"

#include <cstring>   // std::memcpy / std::memset (the icon-table adopt + clear)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // OutputViewState / OutputInternalState

// BrnGui::EventIconManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (5 ledger functions, DWARF primary file
// GameSource/Gui/SatNav/BrnEventIconManager.cpp):
//   EventIconManager::Prepare                @0x82517390  (the icon-pass arm-up)
//   EventIconManager::ReleaseResources       @0x825174C0  (the icon-pass stand-down)
//   EventIconManager::Update2DIcons          @0x824F5938  (CrashNavIconRenderer::RenderIcons)
//   EventIconManager::GetEventIconPositions  @0x824F59C8  (the MapIconManager icon walks)
//   EventIconManager::GetEventIDForIconIndex @0x824F5A78  (MapIconManager::GetEventIDAtIndex)
//
// The last three are straight table operations over the 175-slot {x, y, id} bank; every
// assert is a non-gating tripwire (the X360 falls through after firing).
//
// ⭐ PREPARE / RELEASERESOURCES BOTH POST THE SAME EVENT TWICE, ON TWO CHANNELS. Each ends
// with an identical pair of CgsModule::VariableEventQueue<65536,16>::AddEvent calls -- the
// SAME 68-byte record, first with channel 41 then with channel 42. That is exactly
// StateInterface::OutputViewState<T> followed by OutputInternalState<T>
// (CgsGuiStateInterface.h documents 41 == view state / 42 == internal state, and the
// GuiEventWrapper header the asm builds -- {56, 554, 12} -- is that template's own record).
// The queue is reached as `lpStateInterface + 12`, which is mOutEventQueue on the console's
// 4-byte-pointer ABI (mpObserver +0, mpAccessPointers +4, mpAllocator +8); on the host it is
// reached BY NAME through the two template methods, which is the same thing.

namespace BrnGui
{
    // -------------------------------------------------------------------------
    // @ 0x82517390 -- arm the icon pass.
    //
    // Latches the cache, empties the 175-slot bank, and posts a "draw event icons" event on
    // both output channels telling the icon renderer which event set to show, over what fade,
    // and which ids to skip.
    //
    // STORE ORDER IS THE CONSOLE'S: `*(this + 2104) = lpGuiCache` happens BEFORE the
    // `memset(this, 0, 2100)`, which is safe because 2100 == KI_MAX_2DEVENTICONS * 12 covers
    // the table only; then `*(this + 2100) = 0` clears the count. Reached by name here --
    // 2100 / 2104 are the console's 32-bit offsets and are documentary.
    //
    // ⭐ THE PPC FLOAT-ARG GPR SKIP IS PRESENT IN *BOTH* SIGNATURES ON THIS PATH -- this
    // method's own and the GuiEventDrawEventIcons::Construct it calls. IDA prints Prepare as
    // eight integer parameters (a1..a8) because the f32 rides f1 and DEAD-ENDS r6: the real
    // shape is (this=r3, lpStateInterface=r4, lpGuiCache=r5, lfOptionalFadeDuration=f1
    // [r6 skipped], leNewEventIconType=r7, lpuIconsToIgnore=r8, liNumIconsToIgnore=r9), which
    // is the six-parameter declaration in BrnEventIconManager.h, in order. The nested
    // `GuiEventDrawEventIcons::Construct(v27, 1, a6, v15, a7, a8, a4)` reads the same way --
    // `v15` is that call's own dead r6 -- so it is Construct(true, leNewEventIconType,
    // lfOptionalFadeDuration, lpuIconsToIgnore, liNumIconsToIgnore). Do not add a phantom
    // integer parameter to either.
    // -------------------------------------------------------------------------
    void EventIconManager::Prepare(CgsGui::StateInterface* lpStateInterface, GuiCache* lpGuiCache,
                                   f32 lfOptionalFadeDuration, s32 leNewEventIconType,
                                   u32* lpuIconsToIgnore, s32 liNumIconsToIgnore)
    {
        CGS_ASSERT(lpStateInterface != NULL, "lpStateInterface");   // cpp:72
        CGS_ASSERT(lpGuiCache != NULL, "lpGuiCache");               // cpp:73

        mpGuiCache = lpGuiCache;

        std::memset(ma2DEventIcons, 0, sizeof(ma2DEventIcons));
        miNumEventIcons = 0;

        GuiEventDrawEventIcons lDrawIcons;
        lDrawIcons.Construct(true,
                             static_cast<GuiEventDrawEventIcons::EIconDisplayType>(leNewEventIconType),
                             lfOptionalFadeDuration,
                             lpuIconsToIgnore,
                             liNumIconsToIgnore);

        lpStateInterface->OutputViewState(lDrawIcons);       // AddEvent(..., channel 41, 68)
        lpStateInterface->OutputInternalState(lDrawIcons);   // AddEvent(..., channel 42, 68)
    }

    // -------------------------------------------------------------------------
    // @ 0x825174C0 -- stand the icon pass down.
    //
    // Empties the bank's count and posts the same event with the DRAW flag clear and the
    // display type set to the COUNT sentinel, so the renderer fades whatever it is showing
    // out over lfOptionalFadeDuration and then shows nothing.
    //
    // The console does NOT call GuiEventDrawEventIcons::Construct here -- it inlines it, as
    // four direct field writes into the 56-byte stack record:
    //     v8[10]         = a3   -> +0x28 mfFadeTime         = lfOptionalFadeDuration
    //     LODWORD(v8[11]) = 5   -> +0x2C meIconDisplayType  = E_ICON_DISPLAY_TYPE_COUNT
    //     v8[12]         = 0.0  -> +0x30 miNumIconsToIgnore = 0
    //     HIBYTE(v8[13]) = 0    -> +0x34 mbDrawIcons        = false
    // (HIBYTE is IDA's byte 3 of the word at +0x34, which on this big-endian target is the
    // byte AT +0x34.) Those four writes are precisely Construct(false, COUNT, fade, NULL, 0)
    // -- with the count zero its ignore-list loop does not run and its guard passes -- so it
    // is written as the call here, which is also the only way to reach the private members.
    // Nothing else about the record differs from Prepare's.
    // -------------------------------------------------------------------------
    void EventIconManager::ReleaseResources(CgsGui::StateInterface* lpStateInterface,
                                            f32 lfOptionalFadeDuration)
    {
        CGS_ASSERT(lpStateInterface != NULL, "lpStateInterface");   // cpp:173

        // `*(this + 2100) = 0` -- the count only; the console leaves the table itself alone.
        miNumEventIcons = 0;

        GuiEventDrawEventIcons lDrawIcons;
        lDrawIcons.Construct(false,
                             GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT,
                             lfOptionalFadeDuration,
                             NULL,
                             0);

        lpStateInterface->OutputViewState(lDrawIcons);       // AddEvent(..., channel 41, 68)
        lpStateInterface->OutputInternalState(lDrawIcons);   // AddEvent(..., channel 42, 68)
    }

    // @ 0x824F5938
    void EventIconManager::Update2DIcons(const EventIcon2D* lpaEventIcons, s32 liNumIcons)
    {
        CGS_ASSERT(liNumIcons <= KI_MAX_2DEVENTICONS, "liNumIcons <= KI_MAX_2DEVENTICONS");
        CGS_ASSERT(lpaEventIcons != NULL, "lpaEventIcons");

        std::memcpy(ma2DEventIcons, lpaEventIcons, sizeof(EventIcon2D) * liNumIcons);
        miNumEventIcons = liNumIcons;
    }

    // @ 0x824F59C8
    void EventIconManager::GetEventIconPositions(Vector2* lv2IconPositions, s32* lpiNumIcons)
    {
        CGS_ASSERT(lpiNumIcons != NULL, "lpiNumIcons");

        // One 16-byte lane store per icon: {x, y, 0, 0} (the X360 zeroes the zw pair
        // before the lvx/stvx copy -- Vector2::Set semantics).
        for (s32 liIconIndex = 0; liIconIndex < miNumEventIcons; ++liIconIndex)
        {
            lv2IconPositions[liIconIndex] =
                Vector2{ ma2DEventIcons[liIconIndex].mfEventIconPosX,
                         ma2DEventIcons[liIconIndex].mfEventIconPosY, 0.0f, 0.0f };
        }

        // The X360 re-checks the out-pointer before the count store (the assert above
        // does not gate).
        if (lpiNumIcons != NULL)
            *lpiNumIcons = miNumEventIcons;
    }

    // @ 0x824F5A78
    u32 EventIconManager::GetEventIDForIconIndex(s32 liIconIndex) const
    {
        CGS_ASSERT(liIconIndex >= 0, "liIconIndex >= 0");
        CGS_ASSERT(liIconIndex < miNumEventIcons, "liIconIndex < miNumEventIcons");

        return ma2DEventIcons[liIconIndex].muEventID;
    }
}
