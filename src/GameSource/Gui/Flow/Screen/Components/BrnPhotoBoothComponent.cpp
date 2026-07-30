// ===================================================================================
// BrnGui::PhotoBoothComponent  -- implementation
//   class:BrnGui::PhotoBoothComponent
//
// Construct                      @ 0x8241ABC0   AppendExpectedAptComponents @ 0x8241ACB0
// OnLoad                         @ 0x8243CD68   ReleaseResources            @ 0x8243CF20
// ShowComponent                  @ 0x8243D030   HideComponent               @ 0x8241AD58
// SetButtonPromptVisible         @ 0x82427870   GetTakePhotoStringID        @ 0x8241ADB0
// Select                         @ 0x8243D1B0   Cancel                      @ 0x8243D330
// HandleCompressedStillImageEvent@ 0x8243D4B0   SendPlayerPictureEvent      @ 0x8243D6F8
// SetCachePointer                @ 0x824B3490   EnsureResourcesAreLoaded    @ 0x824B34F0
// EnsureResourcesAreUnloaded     @ 0x824B3578
// SetVisualStyle / SetProfilePointer -- header inlines on X360 (DWARF h:296 / h:278); they
//   have no out-of-line body in the image, so they are bodied here (see each note).
//   Reconstructed store-for-store from the X360 pseudocode/asm.
//
// NO LIVE VISION CAMERA ON PC -- and none is needed. The picture PRODUCER is X360-only
// (BrnNetwork::CameraX360; XUserReadGamerPictureByKey @0x82927650), and nothing on PC posts
// BrnGui::GuiEventCamStatus (id 570), so GuiCache::GetCamStatus() stays 0. That is exactly an
// Xbox 360 with no camera plugged in, and retail has three FIRST-CLASS absent-picture paths
// for it, all reproduced verbatim below:
//   * ShowComponent / OnLoad / SetButtonPromptVisible take their else-branch: the centre
//     prompt becomes KAC_CONTINUE_STRINGID, the photo state is E_PHOTOSTATE_GAMERPIC(1) and
//     the "output the live video feed" event is never sent.
//   * SendPlayerPictureEvent's case 1 does nothing while the camera is absent.
//   * HandleCompressedStillImageEvent only latches in E_PHOTOSTATE_WAITINGFORSTILL(3), which
//     is reachable only from E_PHOTOSTATE_VIDEOFEED(2) -- unreachable without a camera. The
//     image-pushing state 4 is therefore unreachable too.
// No placeholder art is substituted anywhere.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnPhotoBoothComponent.h"
#include "GameSource/Gui/BrnGuiCache.h"                  // BrnGui::GuiCache (+ GetCamStatus)
#include "GameSource/Gui/BrnGuiShared.h"                 // EGuiResourceId + gGuiResourceIdentifier
#include "GameSource/GameState/Progression/BrnProfile.h" // BrnProgression::Profile::SetPlayerLicencePicture
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"      // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie / GetOutputEventQueue
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the streamed unhandled-photostate asserts)
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h" // CgsDev::Assert::Begin/Fire/EndAssert + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

#include <cstddef>   // offsetof (the x64 payload-offset words of the wire records)

namespace BrnGui
{
    // E_GUI_RESOURCEID_NUM -- the terminating count of the GUI resource-id enum. Now homed
    // in BrnGuiShared.h; the X360 asserts the component's photo resource id is a real id
    // (not the sentinel count) before touching the cache.
    static const u32 KU_GUI_RESOURCEID_NUM = static_cast<u32>(E_GUI_RESOURCEID_NUM);   // 237 (0xED), attested by cmplwi

    // -------------------------------------------------------------------------------
    // File-scope statics (BrnPhotoBoothComponent.cpp rodata / bss). Names/types from
    // the DecFIGS DWARF (cpp:27/:29/:30/:32/:41/:49/:51/:53/:54); string values are X360
    // rodata verbatim. The two apt child-icon names seed each embedded HelpItem;
    // sacCachedPictureData backs the DXT1 mugshot the NetworkTexture wraps.
    // -------------------------------------------------------------------------------
    static const char KAC_HELPITEM_LEFT_NAME[]   = "buttonPrompt0";   // cpp:29 (char[14])
    static const char KAC_HELPITEM_CENTRE_NAME[] = "buttonPrompt1";   // cpp:30 (char[14])
    static char       sacCachedPictureData[9600];                     // cpp:27 (char[9600]) @0x82FB0170

    // cpp:32 -- KAPC_ANIMATION_FRAMES[5] @0x82F253A4. ONE table, proven by the
    // table-relative loads: ShowComponent @0x8243D030 addresses off_82F253A4 and then reads
    // (off_82F253A8 - 0x82F253A4)(r11); HideComponent @0x8241AD58 reads
    // (off_82F253B4 - 0x82F253A4)(r11) and (off_82F253B0 - 0x82F253A4)(r11) off the same
    // base. The strings were read out of the image this wave -- they are NOT guesses.
    enum EAnimationFrame
    {
        E_ANIMATIONFRAME_TRANSIN   = 0,   // ShowComponent(false)  -- animate in
        E_ANIMATIONFRAME_IDLE      = 1,   // ShowComponent(true)   -- already on screen
        E_ANIMATIONFRAME_TAKEPIC   = 2,   // Select() in E_PHOTOSTATE_VIDEOFEED
        E_ANIMATIONFRAME_TRANSOUT  = 3,   // HideComponent(false)  -- animate out
        E_ANIMATIONFRAME_INVISIBLE = 4,   // HideComponent(true)   -- gone immediately
        E_ANIMATIONFRAME_COUNT     = 5,
    };
    static const char* const KAPC_ANIMATION_FRAMES[E_ANIMATIONFRAME_COUNT] =
    {
        "transIn",     // @0x8204A780
        "idle",        // @0x8204AE94
        "takePic",     // @0x82048E0C
        "transOut",    // @0x8204A75C
        "invisible",   // @0x8204B4F8
    };

    // cpp:41 -- KAPC_BACK_STRINGIDS[4] @0x82F253B8, indexed by meBackStringType.
    static const char* const KAPC_BACK_STRINGIDS[PhotoBoothComponent::E_BACKSTRING_COUNT] =
    {
        "",                             // E_BACKSTRING_NONE        (@0x820046A7)
        "$PHOTOBOOTH_CPT_BACK",         // E_BACKSTRING_BACK
        "$PHOTOBOOTH_CPT_USEOLDPHOTO",  // E_BACKSTRING_USEOLDPHOTO
        "$PHOTOBOOTH_CPT_CANCEL",       // E_BACKSTRING_CANCEL
    };

    static const char KAC_CONTINUE_STRINGID[]    = "$CAPS_BUTTON_CONTINUE";     // cpp:49 (char[22])
    static const char KAC_TAKEPHOTO_STRINGID[]   = "$PHOTOBOOTH_CPT_TAKEPHOTO"; // cpp:51 (char[26])
    static const char KAC_RETAKEPHOTO_STRINGID[] = "$PHOTOBOOTH_CPT_RETRY";     // cpp:53 (char[22])
    static const char KAC_CONFIRM_STRINGID[]     = "$PHOTOBOOTH_CPT_CONFIRM";   // cpp:54 (char[24])

    namespace
    {
        // The apt level the component's OWN movie is mounted at (OnLoad and
        // ReleaseResources both pass 5; the intro screen movie itself is level 3).
        const s32 KI_PHOTOBOOTH_APT_LEVEL = 5;

        // The queue channels the X360 AddEvent calls name.
        const s32 KI_CHANNEL_GUI_OUT    = 40;   // GuiEventOut     (OutputGuiEvent<T>)
        const s32 KI_CHANNEL_VIEW_STATE = 41;   // GuiOutViewState (OutputViewState<T>)

        // The empty-string literal the X360 passes as `&unk_820046A7` (a bare "" in .rodata)
        // wherever a prompt is being blanked rather than indexed out of a table.
        const char KAC_EMPTY_STRING[] = "";

        // ---- BrnGui::GuiEventNetworkOutputPlayerTexture (id 264) ------------------
        // The record OutputGuiEvent<GuiEventNetworkOutputPlayerTexture> @0x82436D40 builds:
        // { payloadBytes = 8, type = 0x108 (264), payloadOffset = 12 } + { mode, playerIndex },
        // posted on channel 40 with size 0x14 (20). Payload is two words, no pointers, so the
        // x64 record is the same shape; the header words are still derived, not assumed.
        // Modes attested at the six X360 emit sites:
        enum EPlayerTextureMode
        {
            E_PLAYERTEXTURE_OFF       = 0,   // ReleaseResources / HandleCompressedStillImageEvent
            E_PLAYERTEXTURE_VIDEOFEED = 1,   // OnLoad / ShowComponent / Cancel (camera present)
            E_PLAYERTEXTURE_GAMERPIC  = 4,   // OnLoad / ShowComponent / SendPlayerPictureEvent (no camera)
        };

        struct GuiEventNetworkOutputPlayerTextureRecord : public CgsGui::GuiEvent<264>
        {
            s32 meMode;          // payload +0x00
            s32 miPlayerIndex;   // payload +0x04 (every emit site passes -1)

            GuiEventNetworkOutputPlayerTextureRecord(s32 leMode, s32 liPlayerIndex)
                : CgsGui::GuiEvent<264>(), meMode(leMode), miPlayerIndex(liPlayerIndex)
            {
                const size_t luOffset = offsetof(GuiEventNetworkOutputPlayerTextureRecord, meMode);
                muHeader0 = static_cast<u32>(sizeof(*this) - luOffset);   // X360 8
                muHeader2 = static_cast<u32>(luOffset);                   // X360 12
            }
        };

        // ---- BrnGui::GuiEventRequestCompressedCamPic (id 568) ---------------------
        // OutputGuiEvent<GuiEventRequestCompressedCamPic> @0x82436D90 builds
        // { payloadBytes = 12, type = 0x238 (568), payloadOffset = 12 } + three words, on
        // channel 40 with size 0x18 (24). The third word is a POINTER, so on x64 the payload
        // is wider than the console's 12 -- both header words are derived from the real
        // layout (see the project rule on console byte-size literals for pointer records).
        struct GuiEventRequestCompressedCamPicRecord : public CgsGui::GuiEvent<568>
        {
            s32                          miReserved0;      // payload +0x00 (the only call site passes 0)
            renderengine::PixelFormat    mePixelFormat;    // payload +0x04 (0x1A200052 == PIXELFORMAT_DXT1)
            CgsNetwork::NetworkTexture*  mpTargetTexture;  // payload +0x08 console / +0x08..0x0F x64

            GuiEventRequestCompressedCamPicRecord(renderengine::PixelFormat lePixelFormat,
                                                  CgsNetwork::NetworkTexture* lpTargetTexture)
                : CgsGui::GuiEvent<568>()
                , miReserved0(0), mePixelFormat(lePixelFormat), mpTargetTexture(lpTargetTexture)
            {
                const size_t luOffset = offsetof(GuiEventRequestCompressedCamPicRecord, miReserved0);
                muHeader0 = static_cast<u32>(sizeof(*this) - luOffset);   // X360 12
                muHeader2 = static_cast<u32>(luOffset);                   // X360 12
            }
        };

        // ---- BrnGui::GuiEventNetworkPlayerImage (id 258) --------------------------
        // OutputViewState<GuiEventNetworkPlayerImage>: { payloadBytes = 8, type = 258,
        // payloadOffset = 12 } + { texture, index }, channel 41, size 20. First payload word
        // is a POINTER -- header words derived, as above. PhotoBoothComponent passes
        // (&mCachedPicture, 0); LicenseComponent passes (the profile's licence picture, 1).
        struct GuiEventNetworkPlayerImageRecord : public CgsGui::GuiEvent<258>
        {
            const CgsNetwork::NetworkTexture* mpTexture;   // payload +0x00
            s32                               miIndex;     // payload +0x08 x64 (+0x04 console)

            GuiEventNetworkPlayerImageRecord(const CgsNetwork::NetworkTexture* lpTexture, s32 liIndex)
                : CgsGui::GuiEvent<258>(), mpTexture(lpTexture), miIndex(liIndex)
            {
                const size_t luOffset = offsetof(GuiEventNetworkPlayerImageRecord, mpTexture);
                muHeader0 = static_cast<u32>(sizeof(*this) - luOffset);   // X360 8
                muHeader2 = static_cast<u32>(luOffset);                   // X360 12
            }
        };

        // ---- BrnGui::GuiEventCamPicCompressed (id 569) ----------------------------
        // The IN-queue payload view. The state queue hands the consumer the header-stripped
        // record, so this is the same three-word shape HandleCompressedStillImageEvent
        // @0x8243D4B0 walks: *a2 = the compressed byte count, a2[1] = the pixel format,
        // a2[2] = the pixel bytes (CopyPixelData(dest, a2[2], *a2, a2[1])).
        struct GuiEventCamPicCompressedPayload
        {
            s32                       miCompressedPixelSize;   // +0x00
            renderengine::PixelFormat mePixelFormat;           // +0x04
            const char*               mpacCompressedPixels;    // +0x08
        };

        // The X360 open-codes this five-word post at all six emit sites; it is factored to a
        // file-static free function here (rather than a member) so the class keeps exactly the
        // DWARF's method set.
        void PostPlayerTextureEvent(CgsGui::StateInterface* lpStateInterface, s32 leMode)
        {
            GuiEventNetworkOutputPlayerTextureRecord lRecord(leMode, -1);
            lpStateInterface->GetOutputEventQueue()->AddEvent(&lRecord, KI_CHANNEL_GUI_OUT,
                                                             static_cast<s32>(sizeof(lRecord)));
        }
    }

    // @ 0x824B3490 -- latch the GUI cache pointer the component drives its resource
    // load/unload through (asserts non-NULL). stw r31,0x404(this) => mpGuiCache.
    void PhotoBoothComponent::SetCachePointer(GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGuiCache != 0, "NULL != lpGuiCache");
        mpGuiCache = lpGuiCache;
    }

    // @ 0x824B34F0 -- ask the GUI cache to load the component's single photo resource,
    // latch the result in mbResourceLoaded, and return it.
    bool PhotoBoothComponent::EnsureResourcesAreLoaded()
    {
        CGS_ASSERT(mPhotoResourceToLoad.muId != KU_GUI_RESOURCEID_NUM,
                   "E_GUI_RESOURCEID_NUM != mPhotoResourceToLoad.muId");
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        mbResourceLoaded = mpGuiCache->EnsureResourceIsLoaded(mPhotoResourceToLoad);
        return mbResourceLoaded;
    }

    // @ 0x824B3578 -- clear the loaded flag, then ask the GUI cache to unload the
    // component's photo resource and return the unload result. The asm snapshots
    // mpGuiCache, zeroes mbResourceLoaded, then makes the (tail) call in that order.
    bool PhotoBoothComponent::EnsureResourcesAreUnloaded()
    {
        CGS_ASSERT(mPhotoResourceToLoad.muId != KU_GUI_RESOURCEID_NUM,
                   "E_GUI_RESOURCEID_NUM != mPhotoResourceToLoad.muId");
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        GuiCache* lpGuiCache = mpGuiCache;
        mbResourceLoaded = false;
        return lpGuiCache->EnsureResourceIsUnloaded(mPhotoResourceToLoad);
    }

    // ===============================================================================
    // Behavioural methods (reconstructed store-for-store / call-for-call from the X360
    // pseudocode+asm). Only the functions whose collaborators are homed are bodied here;
    // the apt-view-event / OutputGuiEvent<>-driven siblings link from later waves.
    //
    // Construct              @ 0x8241ABC0
    // GetTakePhotoStringID   @ 0x8241ADB0
    // HideComponent          @ 0x8241AD58
    // AppendExpectedAptComponents @ 0x8241ACB0
    // ===============================================================================

    // @0x8241ABC0 -- build the photo-booth component: run the IconComponent base Construct
    // (no state-identifier table), seed the single apt photo resource tuple (id = sentinel,
    // type = APT) as not-yet-loaded, construct the two flanking help-item prompts parented
    // to this component's name, latch the button/string style selectors, null the cache /
    // profile back-pointers, reset the photo state, and wrap the DXT1 mugshot buffer in the
    // cached-picture NetworkTexture.
    void PhotoBoothComponent::Construct(const char* lpacName,
                                        CgsGui::StateInterface* lpStateInterface,
                                        ButtonIconComponent::EPadButton leBackButton,
                                        ButtonIconComponent::EPadButton leConfirmButton,
                                        ETakePhotoStringType leTakePhotoStringType,
                                        EBackStringType leBackStringType,
                                        const char* lpacParentName)
    {
        // IconComponent::Construct(name, stateInterface, /*stateIdentifiers*/ nullptr, parentName).
        IconComponent::Construct(lpacName, lpStateInterface, nullptr, lpacParentName);

        mPhotoResourceToLoad.muId   = KU_GUI_RESOURCEID_NUM;         // 0x94 <- 0xED (no resource yet)
        mPhotoResourceToLoad.meType = CgsGui::E_GUI_RESOURCETYPE_APT; // 0x98 <- 4
        mbResourceLoaded            = false;                         // 0x9C <- 0

        // The two apt child prompts are Constructed (virtual) parented to this item's name.
        mHelpItemLeft.Construct(KAC_HELPITEM_LEFT_NAME, lpStateInterface, GetName());
        mHelpItemCentral.Construct(KAC_HELPITEM_CENTRE_NAME, lpStateInterface, GetName());

        meBackButton          = leBackButton;          // 0x3F8
        meConfirmButton       = leConfirmButton;       // 0x3FC
        meTakePhotoStringType = leTakePhotoStringType; // 0x428
        meBackStringType      = leBackStringType;      // 0x42C

        mpGuiCache  = nullptr;             // 0x404
        mpProfile   = nullptr;             // 0x408
        mePhotoState = E_PHOTOSTATE_NONE;  // 0x400

        // Wrap the DXT1 (160x120) mugshot buffer: format 0x1A200052 == PIXELFORMAT_DXT1.
        mCachedPicture.Construct();
        mCachedPicture.Prepare(sacCachedPictureData, sizeof(sacCachedPictureData),
                               160, 120, renderengine::PIXELFORMAT_DXT1);

        mbVisible = false;                 // 0x430
    }

    // @0x8241ADB0 (cpp:619) -- map the take-photo-prompt style to its localisation string id.
    // The X360 default path streams "Unhandled takephotostringtype " << meTakePhotoStringType
    // << " in PhotoBoothComponent::GetTakePhotoStringID().\n" (cpp:637) into the assert buffer,
    // then falls through to the TAKEPHOTO id (the asm's shared return label). The streamed form
    // is reproduced -- an unhandled value is useless without the value.
    const char* PhotoBoothComponent::GetTakePhotoStringID() const
    {
        switch (meTakePhotoStringType)
        {
            case E_TAKEPHOTOSTRING_CONTINUE:
                return KAC_CONTINUE_STRINGID;
            case E_TAKEPHOTOSTRING_TAKEPHOTO:
                break;
            default:
            {
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unhandled takephotostringtype " << static_cast<s32>(meTakePhotoStringType)
                           << " in PhotoBoothComponent::GetTakePhotoStringID().\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // cpp:637
                CgsDev::Assert::EndAssert();
                break;
            }
        }
        return KAC_TAKEPHOTO_STRINGID;
    }

    // @0x8241AD58 (cpp:290) -- drive the component off-screen: push the apt state identifier
    // (INVISIBLE for an immediate hide, TRANSOUT for the transition-out animation) and clear
    // mbVisible. Both identifiers come out of KAPC_ANIMATION_FRAMES (indices 4 and 3).
    void PhotoBoothComponent::HideComponent(bool lbHide)
    {
        SetState(lbHide ? KAPC_ANIMATION_FRAMES[E_ANIMATIONFRAME_INVISIBLE]
                        : KAPC_ANIMATION_FRAMES[E_ANIMATIONFRAME_TRANSOUT]);
        mbVisible = false;
    }

    // ===============================================================================
    // Behaviour
    // ===============================================================================

    // @0x8243CD68 (cpp:155) -- the component's own apt resource has finished loading: mount
    // its movie at level 5 by NAME (gGuiResourceIdentifier indexed by the resource id the
    // owner selected through SetVisualStyle), then seed the prompts and the initial photo
    // state from whether a Live Vision camera is attached.
    void PhotoBoothComponent::OnLoad()
    {
        CGS_ASSERT(mbResourceLoaded == true, "true == mbResourceLoaded");   // cpp:157

        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[mPhotoResourceToLoad.muId],
                                       KI_PHOTOBOOTH_APT_LEVEL);

        if (mpGuiCache->GetCamStatus() != 0)
        {
            mHelpItemLeft.SetItem(KAPC_BACK_STRINGIDS[meBackStringType], meBackButton,
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mHelpItemCentral.SetItem(GetTakePhotoStringID(), meConfirmButton,
                                     ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_VIDEOFEED);
            mePhotoState = E_PHOTOSTATE_VIDEOFEED;
        }
        else
        {
            mHelpItemLeft.SetItem(KAPC_BACK_STRINGIDS[E_BACKSTRING_NONE],
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mHelpItemCentral.SetItem(KAC_CONTINUE_STRINGID, meConfirmButton,
                                     ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_GAMERPIC);
            mePhotoState = E_PHOTOSTATE_GAMERPIC;
        }
    }

    // @0x8243CF20 (cpp:204) -- unmount the component's apt movie (empty name at the same
    // level 5), stop the player-texture output, and release the resource through the cache.
    // Note it does NOT clear mbResourceLoaded -- EnsureResourcesAreUnloaded is the method
    // that does that.
    void PhotoBoothComponent::ReleaseResources()
    {
        CGS_ASSERT(mPhotoResourceToLoad.muId != KU_GUI_RESOURCEID_NUM,
                   "E_GUI_RESOURCEID_NUM != mPhotoResourceToLoad.muId");   // cpp:206
        CGS_ASSERT(mpGuiCache, "mpGuiCache");                              // cpp:207

        mpStateInterface->PlayAptMovie(KAC_EMPTY_STRING, KI_PHOTOBOOTH_APT_LEVEL);
        PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_OFF);

        mpGuiCache->EnsureResourceIsUnloaded(mPhotoResourceToLoad);
    }

    // @0x8243D030 (cpp:234) -- bring the component on screen. lbShow selects the apt state
    // IDENTIFIER only: true means "it is already there, sit in IDLE", false means "play the
    // TRANSIN animation" -- the same true/false-means-instant/animated split HideComponent
    // uses. (The two identifiers were read out of the image this wave: off_82F253A8 ==
    // "idle", off_82F253A4 == "transIn".) The prompt/state seeding is OnLoad's, repeated.
    void PhotoBoothComponent::ShowComponent(bool lbShow)
    {
        SetState(lbShow ? KAPC_ANIMATION_FRAMES[E_ANIMATIONFRAME_IDLE]
                        : KAPC_ANIMATION_FRAMES[E_ANIMATIONFRAME_TRANSIN]);

        if (mpGuiCache->GetCamStatus() != 0)
        {
            mHelpItemLeft.SetItem(KAPC_BACK_STRINGIDS[meBackStringType], meBackButton,
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mHelpItemCentral.SetItem(GetTakePhotoStringID(), meConfirmButton,
                                     ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_VIDEOFEED);
            mePhotoState = E_PHOTOSTATE_VIDEOFEED;
        }
        else
        {
            mHelpItemLeft.SetItem(KAPC_BACK_STRINGIDS[E_BACKSTRING_NONE],
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mHelpItemCentral.SetItem(KAC_CONTINUE_STRINGID, meConfirmButton,
                                     ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_GAMERPIC);
            mePhotoState = E_PHOTOSTATE_GAMERPIC;
        }

        mbVisible = true;
    }

    // @0x82427870 (cpp:316) -- blank or restore the two button prompts (the intro hides them
    // while a voice-over is playing). The restore path is the same camera / no-camera split;
    // the blank path pushes the empty string with both buttons invisible.
    void PhotoBoothComponent::SetButtonPromptVisible(bool lbVisible)
    {
        if (lbVisible)
        {
            if (mpGuiCache->GetCamStatus() != 0)
            {
                mHelpItemLeft.SetItem(KAPC_BACK_STRINGIDS[meBackStringType], meBackButton,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
                mHelpItemCentral.SetItem(GetTakePhotoStringID(), meConfirmButton,
                                         ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            }
            else
            {
                mHelpItemLeft.SetItem(KAPC_BACK_STRINGIDS[E_BACKSTRING_NONE],
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
                mHelpItemCentral.SetItem(KAC_CONTINUE_STRINGID, meConfirmButton,
                                         ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            }
        }
        else
        {
            mHelpItemLeft.SetItem(KAC_EMPTY_STRING,
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mHelpItemCentral.SetItem(KAC_EMPTY_STRING,
                                     ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                     ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }
    }

    // @0x8243D1B0 (cpp:353) -- the confirm button. Returns true when the owning state should
    // advance past the photo booth.
    //   GAMERPIC(1)         : nothing to take -- hide (animated) and advance. THIS is the PC
    //                         path, and it is the console's own no-camera path.
    //   VIDEOFEED(2)        : play the TAKEPIC frame and ask the network module for a
    //                         compressed still; wait for it.
    //   WAITINGFORSTILL(3)  : ignore (the still has not arrived).
    //   CACHEDSTILLIMAGE(4) : commit the cached picture to the profile, hide and advance.
    bool PhotoBoothComponent::Select()
    {
        switch (mePhotoState)
        {
        case E_PHOTOSTATE_GAMERPIC:
            HideComponent(false);
            return true;

        case E_PHOTOSTATE_VIDEOFEED:
        {
            SetState(KAPC_ANIMATION_FRAMES[E_ANIMATIONFRAME_TAKEPIC]);

            GuiEventRequestCompressedCamPicRecord lRecord(renderengine::PIXELFORMAT_DXT1,
                                                          &mCachedPicture);
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lRecord, KI_CHANNEL_GUI_OUT,
                                                             static_cast<s32>(sizeof(lRecord)));
            mePhotoState = E_PHOTOSTATE_WAITINGFORSTILL;
            return false;
        }

        case E_PHOTOSTATE_WAITINGFORSTILL:
            return false;

        case E_PHOTOSTATE_CACHEDSTILLIMAGE:
            mpProfile->SetPlayerLicencePicture(&mCachedPicture);
            HideComponent(false);
            return true;

        default:
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Unhandled photostate " << static_cast<s32>(mePhotoState)
                       << " in PhotoBoothComponent::Select()\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // cpp:403
            CgsDev::Assert::EndAssert();
            return false;
        }
        }
    }

    // @0x8243D330 (cpp:422) -- the back button. Returns true when the owning state should
    // advance. GAMERPIC(1) and WAITINGFORSTILL(3) swallow it; VIDEOFEED(2) backs straight
    // out; CACHEDSTILLIMAGE(4) throws the still away and returns to the live feed.
    bool PhotoBoothComponent::Cancel()
    {
        switch (mePhotoState)
        {
        case E_PHOTOSTATE_GAMERPIC:
        case E_PHOTOSTATE_WAITINGFORSTILL:
            return false;

        case E_PHOTOSTATE_VIDEOFEED:
            HideComponent(false);
            return true;

        case E_PHOTOSTATE_CACHEDSTILLIMAGE:
            PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_VIDEOFEED);
            mHelpItemLeft.SetItem(KAPC_BACK_STRINGIDS[meBackStringType], meBackButton,
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mHelpItemCentral.SetItem(GetTakePhotoStringID(), meConfirmButton,
                                     ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mePhotoState = E_PHOTOSTATE_VIDEOFEED;
            return false;

        default:
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Unhandled photostate " << static_cast<s32>(mePhotoState)
                       << " in PhotoBoothComponent::Cancel()\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // cpp:464
            CgsDev::Assert::EndAssert();
            return false;
        }
        }
    }

    // @0x8243D4B0 (cpp:484) -- a compressed still has come back from the network module.
    // Only latched in WAITINGFORSTILL(3); every other state drops it. Unreachable on PC (see
    // the file note): nothing can put the component into WAITINGFORSTILL without a camera.
    void PhotoBoothComponent::HandleCompressedStillImageEvent(const void* lpEvent)
    {
        const GuiEventCamPicCompressedPayload* lpCompressedPicEvent =
            reinterpret_cast<const GuiEventCamPicCompressedPayload*>(lpEvent);

        CGS_ASSERT(lpCompressedPicEvent != 0, "lpCompressedPicEvent");   // cpp:486
        CGS_ASSERT(mpProfile != 0, "mpProfile");                         // cpp:487

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)            // cpp:489
            *CgsDev::Log::gpDebugPrint << "Cached Picture size: " << static_cast<u32>(sizeof(sacCachedPictureData)) << "\n";
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)            // cpp:490
            *CgsDev::Log::gpDebugPrint << "CompressedPixelSize: " << lpCompressedPicEvent->miCompressedPixelSize << "\n";
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)            // cpp:491
        {
            const char* lpacPixels = lpCompressedPicEvent->mpacCompressedPixels;
            *CgsDev::Log::gpDebugPrint << "CompressedPixels: "
                                       << (lpacPixels != 0 ? lpacPixels : "<NULLSTRING>") << "\n";
        }

        if (mePhotoState == E_PHOTOSTATE_WAITINGFORSTILL)
        {
            mCachedPicture.CopyPixelData(lpCompressedPicEvent->mpacCompressedPixels,
                                         lpCompressedPicEvent->miCompressedPixelSize,
                                         lpCompressedPicEvent->mePixelFormat);

            PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_OFF);

            mHelpItemLeft.SetItem(KAC_RETAKEPHOTO_STRINGID, meBackButton,
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mHelpItemCentral.SetItem(KAC_CONFIRM_STRINGID, meConfirmButton,
                                     ButtonIconComponent::E_PADBUTTON_INVISIBLE);

            mePhotoState = E_PHOTOSTATE_CACHEDSTILLIMAGE;
        }
    }

    // @0x8243D6F8 (cpp:531) -- called EVERY frame by the owning state. It re-synchronises the
    // component with the camera's live presence and keeps the view fed:
    //   NONE(1 == not entered)      : nothing.
    //   GAMERPIC(1)                 : a camera has APPEARED -> switch to the live feed.
    //   VIDEOFEED(2)/WAITINGFOR(3)  : the camera has GONE -> fall back to the gamer picture.
    //                                 On PC GetCamStatus() is always 0, so this branch is the
    //                                 one that would fire -- but states 2/3 are unreachable
    //                                 without a camera in the first place.
    //   CACHEDSTILLIMAGE(4)         : keep pushing the cached still at the view.
    void PhotoBoothComponent::SendPlayerPictureEvent()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:533

        switch (mePhotoState)
        {
        case E_PHOTOSTATE_NONE:
            break;

        case E_PHOTOSTATE_GAMERPIC:
            if (mpGuiCache->GetCamStatus() != 0)
            {
                mHelpItemLeft.SetItem(KAPC_BACK_STRINGIDS[meBackStringType], meBackButton,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
                mHelpItemCentral.SetItem(GetTakePhotoStringID(), meConfirmButton,
                                         ButtonIconComponent::E_PADBUTTON_INVISIBLE);
                PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_VIDEOFEED);
                mePhotoState = E_PHOTOSTATE_VIDEOFEED;
            }
            break;

        case E_PHOTOSTATE_VIDEOFEED:
        case E_PHOTOSTATE_WAITINGFORSTILL:
            if (mpGuiCache->GetCamStatus() == 0)
            {
                mHelpItemLeft.SetItem(KAPC_BACK_STRINGIDS[E_BACKSTRING_NONE],
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
                mHelpItemCentral.SetItem(KAC_CONTINUE_STRINGID, meConfirmButton,
                                         ButtonIconComponent::E_PADBUTTON_INVISIBLE);
                PostPlayerTextureEvent(mpStateInterface, E_PLAYERTEXTURE_GAMERPIC);
                mePhotoState = E_PHOTOSTATE_GAMERPIC;
            }
            break;

        case E_PHOTOSTATE_CACHEDSTILLIMAGE:
        {
            GuiEventNetworkPlayerImageRecord lRecord(&mCachedPicture, 0);
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lRecord, KI_CHANNEL_VIEW_STATE,
                                                              static_cast<s32>(sizeof(lRecord)));
            break;
        }

        default:
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Unhandled photostate " << static_cast<s32>(mePhotoState)
                       << " in PhotoBoothComponent::SendPlayerPictureEvent()\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // cpp:602
            CgsDev::Assert::EndAssert();
            break;
        }
        }
    }

    // DWARF h:296 -- a HEADER INLINE on X360 (no out-of-line body exists in the image; the
    // store is folded straight into each caller). The four call sites each write a DIFFERENT
    // one of the three E_GUI_RESOURCEID_APT_COMPONENT_PHOTOBOOTH_* ids into
    // mPhotoResourceToLoad.muId (component+0x94), one per EPhotoBoothStyle:
    //   CrashNavDriverDetails::UpdateSetupLicense @0x824C1D9C  li 91  -> BASIC
    //   Intro::HandleIncomingEvents               @0x824C2084  li 92  -> DMV_FULL_PAGE
    //   CompletedGame::Update                     @0x824DACD4  li 93  -> DMV_UPGRADE
    //   InstantResultsState::Update               @0x824DF860  li 93 / 91 (conditional)
    // Every caller passes a compile-time-constant style, so the console cannot distinguish a
    // switch from a base+offset; the switch form is used here because no style->id table
    // exists in the DWARF's static list for this TU.
    void PhotoBoothComponent::SetVisualStyle(EPhotoBoothStyle leStyle)
    {
        switch (leStyle)
        {
        case E_PHOTOBOOTH_STYLE_DMV_FULL_PAGE:
            mPhotoResourceToLoad.muId = E_GUI_RESOURCEID_APT_COMPONENT_PHOTOBOOTH_DMV;
            break;
        case E_PHOTOBOOTH_STYLE_DMV_UPGRADE:
            mPhotoResourceToLoad.muId = E_GUI_RESOURCEID_APT_COMPONENT_PHOTOBOOTH_DMV_UPGRADE;
            break;
        case E_PHOTOBOOTH_STYLE_BASIC:
        default:
            mPhotoResourceToLoad.muId = E_GUI_RESOURCEID_APT_COMPONENT_PHOTOBOOTH_BASIC;
            break;
        }
    }

    // DWARF h:278 -- also a header inline on X360: Intro::HandleIncomingEvents @0x824C1F68
    // fires the assert with the file/line of BrnPhotoBoothComponent.h:280 from inside its own
    // body, which is what proves the inlining. Store plus assert, nothing else.
    void PhotoBoothComponent::SetProfilePointer(BrnProgression::Profile* lpProfile)
    {
        CGS_ASSERT(lpProfile != 0, "NULL != lpProfile");   // BrnPhotoBoothComponent.h:280
        mpProfile = lpProfile;
    }

    // @0x8241ACB0 -- register the component's three apt sub-components (itself and its two
    // help-item prompts, by name) as "expected" on the given GUI flow layer, so the cache
    // waits for them to finish initialising before reporting the flow ready.
    void PhotoBoothComponent::AppendExpectedAptComponents(GuiFlow leFlow)
    {
        CGS_ASSERT(mpGuiCache != nullptr, "NULL != mpGuiCache");
        CGS_ASSERT((E_GUIFLOW_FIRST <= leFlow) && (E_GUIFLOW_COUNT > leFlow),
                   "(E_GUIFLOW_FIRST <= leFlow) && (E_GUIFLOW_COUNT > leFlow)");

        mpGuiCache->AppendExpectedAptComponent(leFlow, GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mHelpItemLeft.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mHelpItemCentral.GetName());
    }
}
