#pragma once

// ===================================================================================
// BrnGui::PhotoBoothComponent  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnPhotoBoothComponent.h
//   class:BrnGui::PhotoBoothComponent
//
// The gamer-picture / photo-booth screen component: an IconComponent that owns a
// single GUI resource (mPhotoResourceToLoad), two help-item prompts, the live camera /
// cached-still NetworkTexture, and a pointer to the GUI cache it drives its resource
// load/unload through.
//
// CLASS SHAPE / enums / member names / member order verbatim from the DecFIGS DWARF
// (BrnPhotoBoothComponent.h / .cpp), gated on the X360 ledger. PhotoBoothComponent :
// public BrnGui::IconComponent. Guest byte offsets are references (the gate compiles
// 64-bit, so members are accessed BY NAME):
//   mPhotoResourceToLoad  component+0x94  (CgsGui::sResourceTuple; muId @+0x0 => +0x94)
//   mbResourceLoaded      component+0x9C  (bool)
//   mpGuiCache            component+0x404 (BrnGui::GuiCache*; guest word index 257)
//
// THIS TU bodies the three resource-plumbing methods proven store-for-store from the
// X360 asm (SetCachePointer @0x824B3490, EnsureResourcesAreLoaded @0x824B34F0,
// EnsureResourcesAreUnloaded @0x824B3578). The remaining DWARF methods are
// declaration-only (their bodies link from their own recon waves).
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"                       // BrnGui::IconComponent (base : CgsGui::GuiComponent)
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"                   // BrnGui::HelpItem (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"                 // BrnGui::ButtonIconComponent::EPadButton
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                                  // BrnGui::GuiFlow
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::sResourceTuple
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"           // CgsNetwork::NetworkTexture (mCachedPicture)

namespace BrnProgression { struct Profile; }  // SetProfilePointer arg (pointer only)

namespace BrnGui
{
    class GuiCache;   // resource load/unload target (BrnGuiCache.h; BrnGui::GuiCache)

    // DWARF BrnPhotoBoothComponent.h:45.
    class PhotoBoothComponent : public IconComponent
    {
    public:
        // DWARF h:48.
        enum EPhotoBoothStyle
        {
            E_PHOTOBOOTH_STYLE_BASIC         = 0,
            E_PHOTOBOOTH_STYLE_DMV_FULL_PAGE = 1,
            E_PHOTOBOOTH_STYLE_DMV_UPGRADE   = 2,
            E_PHOTOBOOTH_STYLE_COUNT         = 3,
        };

        // DWARF h:57.
        enum ETakePhotoStringType
        {
            E_TAKEPHOTOSTRING_NONE      = 0,
            E_TAKEPHOTOSTRING_TAKEPHOTO = 1,
            E_TAKEPHOTOSTRING_CONTINUE  = 2,
            E_TAKEPHOTOSTRING_COUNT     = 3,
        };

        // DWARF h:66.
        enum EBackStringType
        {
            E_BACKSTRING_NONE        = 0,
            E_BACKSTRING_BACK        = 1,
            E_BACKSTRING_USEOLDPHOTO = 2,
            E_BACKSTRING_CANCEL      = 3,
            E_BACKSTRING_COUNT       = 4,
        };

        // DWARF h:176.
        enum EPhotoState
        {
            E_PHOTOSTATE_NONE             = 0,
            E_PHOTOSTATE_GAMERPIC         = 1,
            E_PHOTOSTATE_VIDEOFEED        = 2,
            E_PHOTOSTATE_WAITINGFORSTILL  = 3,
            E_PHOTOSTATE_CACHEDSTILLIMAGE = 4,
            E_PHOTOSTATE_COUNT            = 5,
        };

        // ---- DWARF method set (declaration-only unless bodied in this TU) ----
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       ButtonIconComponent::EPadButton leBackButton,
                       ButtonIconComponent::EPadButton leConfirmButton,
                       ETakePhotoStringType leTakePhotoStringType,
                       EBackStringType leBackStringType, const char* lpacParentName); // DWARF h:85
        void AppendExpectedAptComponents(GuiFlow leFlow);                             // DWARF h:90
        void OnLoad();                                                                // DWARF h:94
        void ReleaseResources();                                                      // DWARF h:98
        void ShowComponent(bool lbShow);                                              // DWARF h:103
        void HideComponent(bool lbHide);                                              // DWARF h:108
        void SetButtonPromptVisible(bool lbVisible);                                  // DWARF h:113
        bool IsActive();                                                              // DWARF h:242
        bool Select();                                                                // DWARF h:121
        bool Cancel();                                                                // DWARF h:125

        // @0x824B3490 (this TU, DWARF h:260) -- latch the GUI cache pointer the
        // component drives its resource load/unload through (asserts non-NULL).
        void SetCachePointer(GuiCache* lpGuiCache);

        void SetProfilePointer(BrnProgression::Profile* lpProfile);                  // DWARF h:278
        void SetVisualStyle(EPhotoBoothStyle leStyle);                               // DWARF h:296

        // @0x824B34F0 (this TU, DWARF h:339) -- ask the cache to load the component's
        // photo resource; cache the load result in mbResourceLoaded and return it.
        bool EnsureResourcesAreLoaded();

        // @0x824B3578 (this TU, DWARF h:359) -- clear the loaded flag and ask the
        // cache to unload the component's photo resource; return the unload result.
        bool EnsureResourcesAreUnloaded();

        void HandleCompressedStillImageEvent(const void* lpEvent);                   // DWARF h:153 (GuiEventCamPicCompressed*)
        void SendPlayerPictureEvent();                                               // DWARF h:157

    private:
        const char* GetTakePhotoStringID() const;                                    // DWARF h:163

        // ---- DWARF member layout (h:200-:226; guest offsets in comments; access BY
        // NAME). The KA*_STRINGID / KAPC_* string tables and sacCachedPictureData
        // (h:221) are file-statics in the .cpp, NOT members. ----
        CgsGui::sResourceTuple          mPhotoResourceToLoad;  // h:200 +0x94  (muId @+0x94)
        bool                            mbResourceLoaded;      // h:201 +0x9C
        HelpItem                        mHelpItemLeft;         // h:203
        HelpItem                        mHelpItemCentral;      // h:204
        ButtonIconComponent::EPadButton meBackButton;         // h:206
        ButtonIconComponent::EPadButton meConfirmButton;      // h:207
        EPhotoState                     mePhotoState;          // h:209
        GuiCache*                       mpGuiCache;            // h:211 +0x404 (guest word index 257)
        BrnProgression::Profile*        mpProfile;            // h:213
        CgsNetwork::NetworkTexture      mCachedPicture;       // h:216
        ETakePhotoStringType            meTakePhotoStringType; // h:223
        EBackStringType                 meBackStringType;      // h:224
        bool                            mbVisible;             // h:226
    };
}
