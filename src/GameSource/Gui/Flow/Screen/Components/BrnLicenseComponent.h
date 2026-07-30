#ifndef BRN_LICENSE_COMPONENT_H
#define BRN_LICENSE_COMPONENT_H

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"                     // BrnGui::IconComponent (base)
#include "GameSource/Gui/BrnGuiTextField.h"                                    // BrnGui::TextField (mDateTextField, by value)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple (maResources)

// ============================================================================
// GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.h
//
// BrnGui::LicenseComponent -- the apt-driven "driver licence" screen component
// (the licence card shown by the Intro / CompletedGame / InstantResults / crash-nav
// driver-details flows). It shows the player's current licence rank artwork, the
// localised licence-issued date, and streams the licence's GUI resources in/out as
// the presentation state machine advances.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (class:BrnGui::LicenseComponent).
// No Feb-2007 source and no DecFIGS DWARF exist for this TU, so the class SHAPE is
// recovered from the X360 asm of the five ledger functions plus the base chain that
// the member offsets prove:
//
//   LicenseComponent : IconComponent : CgsGui::GuiComponent
//     GuiComponent  : vptr@+0x00, macName[128]@+0x04, muHashedName@+0x84,
//                     mpStateInterface@+0x88   (sizeof 0x8C)
//     IconComponent : + mpStateIdentifiers@+0x8C, muStateIndex@+0x90 (sizeof 0x94)
//
// so this class's own members begin at +0x94 -- exactly where the asm's first store
// lands. mpStateInterface (base, +0x88) is the state channel SetProfilePointer reaches
// for the language manager; SetState("idle") (base IconComponent, X360 @0x824E2B90) is
// the "idle" apt-state push SetRank performs.
//
// LAYOUT (AGENTS.md "LAYOUT RECOVERY WITH PADDING"): named anchors at the asm-proven
// guest `this+offset`; gaps reserved with explicit padding. Guest byte offsets in the
// trailing comments are 32-bit; the gate compiles 64-bit, so every member is accessed
// BY NAME (never by raw offset) and the x64 sizeof legitimately drifts from the guest.
// ============================================================================

namespace BrnGui { class GuiCache; }            // stored by pointer (mpGuiCache); body links from BrnGuiCache TU
namespace BrnProgression { class Profile; }     // stored by pointer (mpProfile); GetLicenceIssuedDate in the .cpp

namespace BrnGui
{
    class LicenseComponent : public IconComponent
    {
    public:
        // The licence-presentation state machine's states. Only the two values the
        // ledger asserts name are asm-attested (the full enum is not recovered):
        //   E_LICENSE_FIRST_RESOURCE_UNLOADED -- EnsureResourcesAreLoaded @0x824B3300 asserts
        //                                        against it (state 8).
        //   E_LICENSE_RESOURCES_UNLOADED      -- EnsureResourcesAreUnloaded @0x824B33F8 asserts
        //                                        state == it (state 16).
        enum ELicenseState
        {
            E_LICENSE_FIRST_RESOURCE_UNLOADED = 8,
            E_LICENSE_RESOURCES_UNLOADED      = 16,
        };

        // NOT DECLARED HERE (deliberately): BrnGui::LicenseComponent::Construct @0x8241A610.
        // Its X360 signature (name, stateInterface, parentName) is identical to the virtual
        // CgsGui::GuiComponent::Construct, so declaring it here makes it an OVERRIDE -- and
        // an override with no linked body puts a hole in this class's vtable, which breaks
        // every TU that embeds a LicenseComponent BY VALUE (BrnGui::Intro does). Until
        // BrnLicenseComponent.cpp can be mounted in the build (see the block comment in
        // tools/build/build_game_exe.bat for the four link gaps that stop it), the class
        // must keep inheriting GuiComponent's Construct unchanged. The recovered body is
        // preserved in the .cpp behind the same #if 0 note.

        // 0x824B31E8 -- latch the GUI cache the component streams its resources through
        // (asserts non-null; the store is unconditional).
        void SetCachePointer(GuiCache* lpGuiCache);

        // 0x824B3248 -- latch the player profile and, when it changes, refresh the
        // displayed licence-issued date field from the profile's licence date, formatted
        // for the current language.
        void SetProfilePointer(BrnProgression::Profile* lpProfile);

        // 0x8241A4B8 -- select the licence rank (0..5): push the "idle" apt state, store the
        // rank, and clear the rank-transition flag. Asserts the rank is in range.
        void SetRank(s32 liRank);

        // 0x824B3300 -- ensure the resources for the current licence state are loaded through
        // the GUI cache; returns whether they are all present.
        bool EnsureResourcesAreLoaded();

        // 0x824B33F8 -- ensure the current licence state's resources are unloaded through the
        // GUI cache; returns whether they are all released.
        bool EnsureResourcesAreUnloaded();

    private:
        // +0x94 : the licence-state resource tuples the cache loads/unloads. EnsureResourcesAreLoaded
        //         picks index 1 for the "first resource" state group (state in [8,15)), else 0;
        //         EnsureResourcesAreUnloaded hands the whole array + muNumResources to the cache.
        CgsGui::sResourceTuple   maResources[2];       // +0x94 .. +0xA3
        u32                      muNumResources;       // +0xA4  (count for EnsureResourcesAreUnloaded)
        GuiCache*                mpGuiCache;            // +0xA8
        BrnProgression::Profile* mpProfile;            // +0xAC

        u8   mPad_00B0[0x300 - 0xB0];                  // +0xB0 .. +0x2FF (unrecovered presentation members)
        TextField                mDateTextField;       // +0x300 (localised licence-issued date field)

        u8   mPad_0428[0x7A4 - 0x428];                 // +0x428 .. +0x7A3 (guest TextField sizeof 0x128)
        s32  miRank;                                   // +0x7A4  (selected licence rank 0..5)
        u8   mPad_07A8[0x7B1 - 0x7A8];                 // +0x7A8 .. +0x7B0
        bool mbRankTransitionActive;                   // +0x7B1  (cleared by SetRank; semantic not recovered)
        u8   mPad_07B2[0x7B4 - 0x7B2];                 // +0x7B2 .. +0x7B3
        s32  meCurrentLicenseState;                    // +0x7B4  (ELicenseState)
    };
}

#endif // BRN_LICENSE_COMPONENT_H
