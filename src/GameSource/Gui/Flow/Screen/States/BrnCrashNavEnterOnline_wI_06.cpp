// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::ELoginQuestion
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (PlayAptMovie / OutputGuiEvent)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // BrnGui::GuiEventNetworkConnect
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event
#include "GameSource/Gui/BrnGuiCache.h"                           // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                   // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::RegisterForEvents

namespace BrnGui
{

    namespace
    {
        // The apt movie the screen puts up once its resources are in, and the level it
        // goes to. The name is a pooled literal (off_82F27B74) shared with other screens,
        // not a class static of this one.
        const char KAC_CONNECTING_MOVIE[]    = "ON_CONN";
        const s32  KI_CONNECTING_MOVIE_LEVEL = 3;

        // The GuiEventNetworkConnect payload word this screen produces. FLAG: the
        // producer-side enum is not recovered -- these are the two raw values the X360
        // stores (@0x824DD684 `li r10, 2`, reusing the register the sub-state store left
        // set, and @0x824DD6A8 `li r11, 0`), named after the sign-in flavour each goes
        // with.
        const s32 KI_CONNECT_TYPE_FULL     = 0;
        const s32 KI_CONNECT_TYPE_NO_TITLE = 2;
    }

    // ================================================================================
    // @0x824DD570 (cpp:746) -- polled from the screen's Update while it is still coming
    // up. Two sub-states: wait for the resource list, then wait for the apt components
    // HandleGuiCacheEvent registered. Once both are in, the screen either replays the
    // disconnect it had to shelve during the load or asks the network layer to connect.
    // ================================================================================
    void CrashNavEnterOnlineBase::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:748

        if (meSubState == E_SUBSTATE_LOADING_SCREEN)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie(KAC_CONNECTING_MOVIE, KI_CONNECTING_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
        }
        else if (meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            // The X360 re-tests the cache here (@0x824DD614) even though the assert above
            // already complained about it -- the assert does not stop the function.
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                HideAllComponents();

                meCurrentLoginQuestion = CgsGui::E_LOGIN_QUESTION_COUNT;
                meSubState             = E_SUBSTATE_CONNECTING;

                // A disconnect that arrived mid-load was shelved rather than acted on;
                // now that the sub-state has moved past the loading pair (both stores
                // above precede this call in the X360 too, @0x824DD654/658 before the
                // `bl` at @0x824DD668) it can be run for real. The handler is given the
                // stored payload record directly.
                if (mbCachedDisconnectEventToHandle)
                {
                    HandleDisconnectedEvent(
                        reinterpret_cast<const CgsModule::Event*>(&mCachedHandleDisconnectEvent));
                    return;
                }

                s32 liConnectType;
                if (meSignInType == E_SIGN_IN_TYPE_NO_TITLE)
                {
                    liConnectType = KI_CONNECT_TYPE_NO_TITLE;
                }
                else
                {
                    // cpp:794 -- anything that is not one of the two known flavours
                    // complains and then carries on into the full-sign-in value, which is
                    // exactly what the X360 does (the assert arm at @0x824DD68C runs
                    // straight on into `li r11, 0` at @0x824DD6A8).
                    if (meSignInType != E_SIGN_IN_TYPE_FULL)
                    {
                        CGS_ASSERT(false, "Unknown sign in type");
                    }
                    liConnectType = KI_CONNECT_TYPE_FULL;
                }

                GuiEventNetworkConnect lConnect(liConnectType);
                mpStateInterface->OutputGuiEvent(lConnect);
            }
        }
    }

    namespace
    {
        // ---- in-queue payload view -----------------------------------------------------
        // The gui-cache event (id 64). The queue hands the state the HEADER-STRIPPED
        // payload, so the cache pointer sits at +0x00 -- the X360 reads it with a bare
        // `lwz r11, 0(r27)` (@0x824BC43C) and again at @0x824BC4CC.
        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpCache;   // +0x00
        };
    }

    // ================================================================================
    // @0x824BC428 (cpp:451) -- the gui-cache event (id 64). The screen adopts the cache
    // the first time it is offered one, and immediately tells it which apt components it
    // is going to wait on before it will consider itself loaded.
    // ================================================================================
    void CrashNavEnterOnlineBase::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiCachePayload* lpPayload = reinterpret_cast<const GuiCachePayload*>(lpEvent);

        // cpp:451 -- the streamed-message form of the assert in the X360 image. The test
        // is on the CACHE the event carries, not on the event pointer, and it is
        // non-fatal: the adoption below runs either way.
        CGS_ASSERT(lpPayload->mpCache != 0, "Invalid cache in HandleGuiCacheEvent::Update");

        // Only the first cache offered is taken; a screen that already has one ignores
        // the event entirely (@0x824BC4C8 branches straight to the epilogue).
        if (mpGuiCache != 0)
        {
            return;
        }

        mpGuiCache = lpPayload->mpCache;
        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        // The nine name-taking registrations, in the X360's order (@0x824BC4DC onwards).
        // Each `bl sub_824F87C0` is handed the component's own name buffer directly
        // (r5 = this + component offset + 4, and +4 within a GuiComponent is macName),
        // i.e. GuiCache::AppendExpectedAptComponent(GuiFlow, const char*).
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageText.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTOSQuestion.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTOSText.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageButtonsAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTOSDisplayAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mShareInfoTogglesAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mButtonPromptAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mSignInBackgroundAnimation.GetName());

        // The two containers register their own rows (each knows how many it built).
        mShareInfoToggles.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);
        mMessageButtons.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
    }

    namespace
    {
        // The "this group/menu has no apt id" sentinel the two selectable containers get
        // at Construct time. The X360 materialises it as `li r8, -1` followed by
        // `clrldi r8, r8, 32` (@0x824C2B98/@0x824C2BA4 and @0x824C2C04/@0x824C2C10), i.e.
        // the zero-extended 32-bit -1 in a 64-bit register -- NOT a sign-extended -1LL.
        // Same value as SelectableGroup::KU64_NO_ID; spelled out here the way the sibling
        // BrnOnlineGameRoomPlayerInfo_wH_01.cpp Construct sites spell it.
        const u64 KU64_NO_APT_ID = 0xFFFFFFFFull;

        // The number of rows both selectable containers are built with (`li r6, 2`).
        const s32 KI_NUM_SELECTABLE_ROWS = 2;
    }

    // ================================================================================
    // @0x824C2A70 (cpp:148) -- vtable slot 0. Subscribe to the screen's twelve events,
    // build all eleven components, then reset the whole sub-state machine and wire up
    // the per-login-question show functions.
    // ================================================================================
    void CrashNavEnterOnlineBase::OnEnter()
    {
        // The X360 folds the count static to `li r5, 12`; it is the same value.
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The eleven Construct calls, in the X360's own order (@0x824C2A98 onwards). The
        // nine plain components go through GuiComponent::Construct's vtable slot with a
        // null parent name; the two selectable containers take the row count / parent /
        // apt-id form.
        mMessageAnimation.Construct(KAC_MESSAGE_ANIMATION_COMPONENT, mpStateInterface, 0);
        mMessageButtonsAnimation.Construct(KAC_MESSAGE_BUTTONS_ANIMATION_COMPONENT, mpStateInterface, 0);
        mTOSDisplayAnimation.Construct(KAC_TOS_DISPLAY_ANIMATION_COMPONENT, mpStateInterface, 0);
        mShareInfoTogglesAnimation.Construct(KAC_SHARE_INFO_ANIMATION_COMPONENT, mpStateInterface, 0);
        mButtonPromptAnimation.Construct(KAC_BUTTON_PROMPT_ANIMATION_COMPONENT, mpStateInterface, 0);
        mSignInBackgroundAnimation.Construct(KAC_SIGN_IN_BACKGROUND_ANIMATION_COMPONENT, mpStateInterface, 0);
        mMessageText.Construct(KAC_TEXTFIELD_COMPONENT, mpStateInterface, 0);
        mMessageButtons.Construct(KAC_MESSAGE_BUTTONS_COMPONENT, mpStateInterface,
                                  KI_NUM_SELECTABLE_ROWS, 0, KU64_NO_APT_ID);
        mTOSQuestion.Construct(KAC_TOS_QUESTION_COMPONENT, mpStateInterface, 0);
        mTOSText.Construct(KAC_TOS_TEXT_COMPONENT, mpStateInterface, 0);
        mShareInfoToggles.Construct(KAC_SHARE_INFO_TOGGLES_COMPONENT, mpStateInterface,
                                    KI_NUM_SELECTABLE_ROWS, 0, KU64_NO_APT_ID);

        // The thirteen initialisation stores (@0x824C2C24 onwards), in the X360's store
        // order. The three floats all come off the one pooled 0.0f (flt_82001CC0).
        mfTimeInState          = 0.0f;
        meSubState             = E_SUBSTATE_LOADING_SCREEN;
        mfLastAxisValue        = 0.0f;
        meCurrentLoginQuestion = CgsGui::E_LOGIN_QUESTION_COUNT;   // COUNT == no question showing
        mfScrollAmount         = 0.0f;
        mpGuiCache             = 0;
        mbJunkyardEntered      = false;
        meSignInType           = E_SIGN_IN_TYPE_COUNT;             // COUNT == not chosen yet
        mbIgnoreDisconnectedPopup      = false;
        mbIsInvalidatingCollisionWorld = false;

        // The cached disconnect record is emptied by blanking its reason text and
        // clearing the error code -- the X360 writes only those two fields (`stb 0x3754`
        // and `stw 0x3750`), not the whole 132 bytes.
        mCachedHandleDisconnectEvent.macReason[0] = 0;
        mCachedHandleDisconnectEvent.meLastError  = 0;
        mbCachedDisconnectEventToHandle           = false;

        // NOTE: miNumFilesToLoad is deliberately NOT initialised here -- the X360 leaves
        // it alone in OnEnter.

        InitialiseFunctionArrays();
    }
}
