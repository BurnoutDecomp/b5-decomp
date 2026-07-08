// ===================================================================================
// BrnGui::PhotoBoothComponent  -- resource-plumbing implementation
//   class:BrnGui::PhotoBoothComponent
//
// SetCachePointer            @ 0x824B3490
// EnsureResourcesAreLoaded   @ 0x824B34F0
// EnsureResourcesAreUnloaded @ 0x824B3578
//   Reconstructed store-for-store from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnPhotoBoothComponent.h"
#include "GameSource/Gui/BrnGuiCache.h"                  // BrnGui::GuiCache
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

namespace BrnGui
{
    // E_GUI_RESOURCEID_NUM -- the terminating count of the GUI resource-id enum
    // (BrnGuiShared.h). The X360 asserts the component's photo resource id is a real
    // id (not the sentinel count) before touching the cache. That enum is not committed
    // to this tree, so the count is spelled by its X360-attested guest value (0xED).
    static const u32 KU_GUI_RESOURCEID_NUM = 237;   // 0xED, attested by cmplwi

    // -------------------------------------------------------------------------------
    // File-scope statics (BrnPhotoBoothComponent.cpp rodata / bss). Names/types from
    // the DecFIGS DWARF (cpp:27/:29/:30); string values are X360 rodata verbatim
    // (aButtonprompt0 / aButtonprompt1). The two apt child-icon names seed each embedded
    // HelpItem; sacCachedPictureData backs the DXT1 mugshot the NetworkTexture wraps.
    // -------------------------------------------------------------------------------
    static const char KAC_HELPITEM_LEFT_NAME[]   = "buttonPrompt0";   // cpp:29 (char[14])
    static const char KAC_HELPITEM_CENTRE_NAME[] = "buttonPrompt1";   // cpp:30 (char[14])
    static char       sacCachedPictureData[9600];                     // cpp:27 (char[9600])

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

    // @0x8241ADB0 -- map the take-photo-prompt style to its localisation string id. The X360
    // default path streams "Unhandled takephotostringtype <n> in ...GetTakePhotoStringID()"
    // into the assert buffer; per the project rule the CgsDev::StrStream path is reconstructed
    // with the committed CGS_ASSERT macro (the streamed value is dropped) and the function then
    // falls through to the TAKEPHOTO id (matching the asm's shared return label).
    const char* PhotoBoothComponent::GetTakePhotoStringID() const
    {
        switch (meTakePhotoStringType)
        {
            case E_TAKEPHOTOSTRING_TAKEPHOTO:
                return "$PHOTOBOOTH_CPT_TAKEPHOTO";
            case E_TAKEPHOTOSTRING_CONTINUE:
                return "$CAPS_BUTTON_CONTINUE";
            default:
                CGS_ASSERT(false,
                    "Unhandled takephotostringtype in PhotoBoothComponent::GetTakePhotoStringID().\n");
                return "$PHOTOBOOTH_CPT_TAKEPHOTO";
        }
    }

    // @0x8241AD58 -- drive the component off-screen: push the apt state identifier ("invisible"
    // for an immediate hide, "transOut" for the transition-out animation) and clear mbVisible.
    void PhotoBoothComponent::HideComponent(bool lbHide)
    {
        SetState(lbHide ? "invisible" : "transOut");
        mbVisible = false;
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
