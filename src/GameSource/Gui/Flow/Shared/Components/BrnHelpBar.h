#pragma once

// ===================================================================================
// BrnGui::HelpBar  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnHelpBar.h
//
// The shared GUI "help bar" component: a row of up to seven help items (a caption between
// two pad-button glyphs), each driven on screen by its own animator. Screens Construct it
// with the number of items they use, Append {text, button, button} items, and Update it
// each frame; once every item's apt width is known the bar lays the items out in a row.
//
// Class shape and member order are the debug-info's (BrnHelpBar.h:50), checked against
// the console: the constructor builds seven HelpItems from +0x8C (stride
// 0x1AC) and seven Animators from +0xC40 (stride 0x288); Construct and the layout methods
// reach mafItemWidths at +0x1DF8, miNumUsedItems +0x1E14, mCurrentTime +0x1E18,
// mBasePosition +0x1E20, the four layout floats +0x1E30..+0x1E3C, miNumItems +0x1E40 and
// mbItemInfoValid +0x1E44 (console sizeof 0x1E50). Members are reached by name.
// ===================================================================================
#include "types.hpp"
#include "BrnCommonTypes.h"                                               // Vector2

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"      // CgsGui::GuiComponent (base)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h" // CgsGui::AnimChannelData::Time
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiFlow (AppendExpectedAptComponent)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimator.h"           // BrnGui::Animator
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"         // ButtonIconComponent::EPadButton
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"           // BrnGui::HelpItem

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    class GuiCache;

    struct HelpBar : public CgsGui::GuiComponent
    {
        // debug-info BrnHelpBar.h:53 -- where SnapIn lines the items up.
        enum EAlignment
        {
            E_ALIGNMENT_LEFT   = 0,
            E_ALIGNMENT_RIGHT  = 1,
            E_ALIGNMENT_CENTRE = 2,
            E_ALIGNMENT_MAX    = 3,
        };

        // debug-info BrnHelpBar.h:62.
        static const s32 KI_MAX_HELPITEMS = 7;

        // construct liNumItems items named "<name><i>" (parented to
        // lpacParentName) with their animators, and reset the layout state. The bar itself is
        // not named.
        void Construct(const char* lpacName, s32 liNumItems,
                       CgsGui::StateInterface* lpStateInterface, const char* lpacParentName);
        // release every constructed item's animator.
        void Destruct();
        // latch the bar's on-screen base position from the first animator.
        void SetupComponent();
        // register every item as an expected apt component (and its animator's
        // object controller as a controlled one).
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache);

        // item liItem's name hash.
        s32 GetItemNameHash(s32 liItem) const;
        // item liItem's animator.
        Animator* GetAnimator(s32 liItem);

        // blank every item and forget the appended ones.
        void Clear();
        // append one {text, button, second button} item; returns its index,
        // or -1 when the bar is full. E_PADBUTTON_INVISIBLE is "no button".
        s32 AppendHelpBarItem(const char* lpacText,
                              ButtonIconComponent::EPadButton leButton,
                              ButtonIconComponent::EPadButton leSecondButton);
        // show the appended items in a row from the base position.
        void SnapIn(EAlignment leAlignment);
        // finish the item layout once the widths are known, then animate.
        void Update(CgsGui::AnimChannelData::Time lTime);

    private:
        // forget item liItem's width (here and in its apt clip).
        void InvalidateItemInfo(s32 liItem);
        // read the missing item widths back from apt; snap the bar in once
        // all are known.
        void UpdateItemInfo();

        // The invalid-width marker and the apt width variable's name.
        static const f32  KF_ITEMWIDTH_INVALID;
        static const char KAC_WIDTHVAR_NAME[8];

        // ---- data members (debug-info order) ------------------------------------------------
        HelpItem                       maItems[KI_MAX_HELPITEMS];        // +0x008C
        Animator                       maAnimators[KI_MAX_HELPITEMS];    // +0x0C40
        f32                            mafItemWidths[KI_MAX_HELPITEMS];  // +0x1DF8
        s32                            miNumUsedItems;                   // +0x1E14
        CgsGui::AnimChannelData::Time  mCurrentTime;                     // +0x1E18
        Vector2                        mBasePosition;                    // +0x1E20
        f32                            mfAnimationTime;                  // +0x1E30
        f32                            mfDelayBetweenItems;              // +0x1E34
        f32                            mfSpacer;                         // +0x1E38
        f32                            mfStartX;                         // +0x1E3C
        s32                            miNumItems;                       // +0x1E40
        bool                           mbItemInfoValid;                  // +0x1E44
    };
}
