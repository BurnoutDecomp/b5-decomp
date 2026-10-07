// ===================================================================================
// BrnGui::HelpBar  -- implementation
//   class:BrnGui::HelpBar
//
// Reconstructed from the console image; the assembly listing arbitrates over the decompiler
// output throughout. The constructor is compiler-generated (member construction only).
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpBar.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                           // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptObjectController.h" // ObjectController
#include "GameSource/Gui/BrnGuiCache.h"                                           // GuiCache

namespace BrnGui
{
    namespace
    {
        // Construct's per-item name buffer and format ("<bar name><index>").
        const s32  KI_ITEM_NAME_LENGTH    = 64;
        const char KAC_ITEM_NAME_FORMAT[] = "%s%d";

        // Construct's layout defaults (values read from the image).
        const f32 KF_DEFAULT_ANIMATION_TIME      = 0.4f;
        const f32 KF_DEFAULT_DELAY_BETWEEN_ITEMS = 0.1f;
        const f32 KF_DEFAULT_SPACER              = 25.0f;
        const f32 KF_DEFAULT_START_X             = -900.0f;
    }

    const f32  HelpBar::KF_ITEMWIDTH_INVALID = -1.0f;
    const char HelpBar::KAC_WIDTHVAR_NAME[8] = "mfWidth";

    // ------------------------------------------------------------ Construct
    void HelpBar::Construct(const char* lpacName, s32 liNumItems,
                            CgsGui::StateInterface* lpStateInterface, const char* lpacParentName)
    {
        CGS_ASSERT(lpacName != 0, "lacName");
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");
        CGS_ASSERT(liNumItems <= KI_MAX_HELPITEMS, "liNumItems <= KI_MAX_HELPITEMS");

        miNumItems = liNumItems;
        for (s32 liItem = 0; liItem < miNumItems; ++liItem)
        {
            char lacItemName[KI_ITEM_NAME_LENGTH];
            CgsCore::SPrintf(lacItemName, KI_ITEM_NAME_LENGTH, KAC_ITEM_NAME_FORMAT, lpacName, liItem);
            maItems[liItem].Construct(lacItemName, lpStateInterface, lpacParentName);
            maAnimators[liItem].Construct(&maItems[liItem]);
            mafItemWidths[liItem] = KF_ITEMWIDTH_INVALID;
        }

        miNumUsedItems  = 0;
        mbItemInfoValid = false;
        mCurrentTime    = 0.0f;
        mBasePosition.x = 0.0f;
        mBasePosition.y = 0.0f;
        mBasePosition.z = 0.0f;
        mBasePosition.w = 0.0f;
        mfAnimationTime     = KF_DEFAULT_ANIMATION_TIME;
        mfDelayBetweenItems = KF_DEFAULT_DELAY_BETWEEN_ITEMS;
        mfSpacer            = KF_DEFAULT_SPACER;
        mfStartX            = KF_DEFAULT_START_X;
    }

    // ------------------------------------------------------------ Destruct
    void HelpBar::Destruct()
    {
        for (s32 liItem = 0; liItem < miNumItems; ++liItem)
            maAnimators[liItem].Destruct();
    }

    // ------------------------------------------------------------ SetupComponent
    void HelpBar::SetupComponent()
    {
        Animator* lpAnimator = &maAnimators[0];
        CGS_ASSERT(lpAnimator != 0, "lpAnimator");

        CgsGui::ObjectController* lpObjectController = lpAnimator->GetObjectController();
        CGS_ASSERT(lpObjectController != 0, "lpObjectController");

        mBasePosition = lpObjectController->GetPos();
    }

    // ------------------------------------------------------------ AppendExpectedAptComponent
    void HelpBar::AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        lpGuiCache->ClearExpectedControlledAptComponentList();

        for (s32 liItem = 0; liItem < miNumItems; ++liItem)
        {
            Animator* lpAnimator = GetAnimator(liItem);
            CGS_ASSERT(lpAnimator != 0, "Invalid animator");

            CgsGui::ObjectController* lpObjectController = lpAnimator->GetObjectController();
            CGS_ASSERT(lpObjectController != 0, "Invalid controller object");

            lpGuiCache->AppendExpectedAptComponent(leFlow, static_cast<u32>(GetItemNameHash(liItem)));
            lpGuiCache->AppendExpectedControlledAptComponent(static_cast<u32>(GetItemNameHash(liItem)),
                                                             lpObjectController);
        }
    }

    // ------------------------------------------------------------ GetItemNameHash
    s32 HelpBar::GetItemNameHash(s32 liItem) const
    {
        CGS_ASSERT(liItem >= 0 && liItem < KI_MAX_HELPITEMS, "Invalid item index");

        return static_cast<s32>(maItems[liItem].GetNameHash());
    }

    // ------------------------------------------------------------ GetAnimator
    Animator* HelpBar::GetAnimator(s32 liItem)
    {
        CGS_ASSERT(liItem >= 0 && liItem < KI_MAX_HELPITEMS, "Invalid item index");

        return &maAnimators[liItem];
    }

    // ------------------------------------------------------------ Clear
    void HelpBar::Clear()
    {
        for (s32 liItem = 0; liItem < miNumItems; ++liItem)
        {
            maItems[liItem].SetItem("", ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                    ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }

        miNumUsedItems = 0;
        for (s32 liItem = 0; liItem < miNumItems; ++liItem)
            InvalidateItemInfo(liItem);
        mbItemInfoValid = false;
    }

    // ------------------------------------------------------------ AppendHelpBarItem
    s32 HelpBar::AppendHelpBarItem(const char* lpacText,
                                   ButtonIconComponent::EPadButton leButton,
                                   ButtonIconComponent::EPadButton leSecondButton)
    {
        CGS_ASSERT(miNumUsedItems >= 0 && miNumUsedItems < miNumItems, "Help Bar is full");
        CGS_ASSERT(lpacText != 0, "Invalid text");
        CGS_ASSERT(leButton >= 0 && leButton < ButtonIconComponent::E_PADBUTTON_COUNT, "Invalid button");
        CGS_ASSERT(leSecondButton >= 0 && leSecondButton < ButtonIconComponent::E_PADBUTTON_COUNT,
                   "Invalid button");

        if (miNumUsedItems >= miNumItems)
            return -1;

        const s32 liItem = miNumUsedItems;
        maItems[liItem].SetItem(lpacText, leButton, leSecondButton);
        ++miNumUsedItems;
        return liItem;
    }

    // ------------------------------------------------------------ SnapIn
    void HelpBar::SnapIn(EAlignment leAlignment)
    {
        CGS_ASSERT(mbItemInfoValid, "Cannot transition in before all the item info is set");

        f32 lfX = mBasePosition.x;

        f32 lfTotalWidth = 0.0f;
        for (s32 liItem = 0; liItem < miNumUsedItems; ++liItem)
        {
            lfTotalWidth += mafItemWidths[liItem];
            if (liItem > 0)
                lfTotalWidth += mfSpacer;
        }

        switch (leAlignment)
        {
            case E_ALIGNMENT_LEFT:
                lfX += lfTotalWidth;
                break;
            case E_ALIGNMENT_RIGHT:
                break;
            case E_ALIGNMENT_CENTRE:
                lfX += lfTotalWidth * 0.5f;
                break;
            default:
                CGS_ASSERT(false, "Unhandled case");
                break;
        }

        for (s32 liItem = 0; liItem < miNumUsedItems; ++liItem)
        {
            CgsGui::ObjectController* lpObjectController = maAnimators[liItem].GetObjectController();
            CGS_ASSERT(lpObjectController != 0, "lpObjectController");

            lpObjectController->SetObjectVariableBoolean("_visible", true);
            if (liItem != 0)
                lfX += mafItemWidths[liItem - 1];
            lfX += mfSpacer;
            lpObjectController->SetObjectVariableFloat("_x", lfX);
        }
    }

    // ------------------------------------------------------------ Update
    void HelpBar::Update(CgsGui::AnimChannelData::Time lTime)
    {
        UpdateItemInfo();
        for (s32 liItem = 0; liItem < miNumUsedItems; ++liItem)
            maAnimators[liItem].Update(lTime);
        mCurrentTime = lTime;
    }

    // ------------------------------------------------------------ InvalidateItemInfo
    void HelpBar::InvalidateItemInfo(s32 liItem)
    {
        CgsGui::ObjectController* lpObjectController = maAnimators[liItem].GetObjectController();
        CGS_ASSERT(lpObjectController != 0, "Invalid ObjectController");

        mafItemWidths[liItem] = KF_ITEMWIDTH_INVALID;
        lpObjectController->SetObjectVariableFloat(KAC_WIDTHVAR_NAME, KF_ITEMWIDTH_INVALID);
        mbItemInfoValid = false;
    }

    // ------------------------------------------------------------ UpdateItemInfo
    void HelpBar::UpdateItemInfo()
    {
        if (mbItemInfoValid)
            return;

        mbItemInfoValid = true;
        for (s32 liItem = 0; liItem < miNumUsedItems; ++liItem)
        {
            if (mafItemWidths[liItem] != KF_ITEMWIDTH_INVALID)
                continue;

            CgsGui::ObjectController* lpObjectController = maAnimators[liItem].GetObjectController();
            CGS_ASSERT(lpObjectController != 0, "Invalid ObjectController");

            const f32 lfWidth = lpObjectController->GetObjectVariableFloat(KAC_WIDTHVAR_NAME);
            if (lfWidth == KF_ITEMWIDTH_INVALID)
            {
                // apt has not laid this item out yet: try again next frame.
                mbItemInfoValid = false;
                break;
            }
            mafItemWidths[liItem] = lfWidth;
        }

        if (mbItemInfoValid)
            SnapIn(E_ALIGNMENT_RIGHT);
    }
}
