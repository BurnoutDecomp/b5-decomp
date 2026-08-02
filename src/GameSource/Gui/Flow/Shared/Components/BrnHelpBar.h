#pragma once

// ===================================================================================
// BrnGui::HelpBar  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnHelpBar.h
//
// The shared GUI "help bar" component: a fixed bar of up to 7 help items, each carrying
// a localised name hash, plus a parallel array of up to 7 embedded APT animators. No
// prior reconstruction carries this component's full member layout and there is no
// DecFIGS DWARF for this TU; the recovered shape comes from the two X360 item accessors
// that own it (the constructor is homed separately):
//
//   GetItemNameHash @ 0x824E2280 - bounds-checks the item index (0 <= index < 7),
//       firing the "Invalid item index" assert (BrnHelpBar.h:197) on failure, then
//       returns by value the u32 name hash at this + 428*index + 0x110. The 428-byte
//       (0x1AC) item-record stride and the +0x110 field offset are fixed by the asm
//       (mulli 0x1AC / add / lwz 0x110); the item array sits at object +0x00.
//   GetAnimator @ 0x824E2330 - same 7-item bound (BrnHelpBar.h:241), then returns the
//       interior pointer to the animator at this + 648*index + 0xC40. The 648-byte
//       (0x288) animator stride is fixed by the asm (mulli 0x288 / add / addi 0xC40);
//       the animator array sits at object +0xC40.
//
// The two arrays are modeled as arrays of element structs so the accessors read/return
// the element by name. Each element's unrecovered interior (the parts these accessors do
// not touch) is a reserved byte-span sized to the attested stride. All access is by name.
// ===================================================================================

// LAYOUT CAVEAT for the HelpBar TU: the model below is KNOWN INCOMPLETE. The real object
// also carries live counts near +7700/+7744, and the OnlineGameOptions embed pins the
// console sizeof at 7760 (its members run 23616..31376). Nothing is resized here because
// every access is by name, but do not treat this class as fully mapped.
#include "types.hpp"

#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                  // GuiFlow (AppendExpectedAptComponent)
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h" // ButtonIconComponent::EPadButton

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    class GuiCache;

    class HelpBar
    {
    public:
        // Fixed help-item count (both accessors fault on index >= 7).
        static const s32 KI_MAX_ITEMS = 7;

        // Item-record stride (GetItemNameHash: 428*index, name hash at +0x110).
        static const s32 KI_ITEM_RECORD_STRIDE = 428;   // 0x1AC

        // One help item record. Stride 0x1AC (428); only the name hash at +0x110 is
        // touched by GetItemNameHash, the rest is the unrecovered item interior.
        struct ItemRecord
        {
            u8  maHead[0x110];    // +0x000..+0x10F  unrecovered item head
            u32 muNameHash;       // +0x110          localised name hash
            u8  maTail[0x1AC - 0x114];  // +0x114..+0x1AB  unrecovered item tail
        };

        // One embedded APT animator. Stride 0x288 (648); GetAnimator returns a pointer to
        // the whole record, so its interior is held opaque.
        struct Animator
        {
            u8  maStorage[0x288];   // +0x000..+0x287  unrecovered animator interior
        };

        // @ 0x82515328 - construct the help bar (homed by class:BrnGui::HelpBar). Heavy
        // embedded-aggregate ctor (see BrnHelpBar.cpp for why it is not yet reconstructed).
        HelpBar();

        // @ 0x824E2280 - return item `luIndex`'s name hash by value. Asserts
        // 0 <= luIndex < 7.
        u32 GetItemNameHash(u32 luIndex);

        // @ 0x824E2330 - return the pointer to item `luIndex`'s animator. Asserts
        // 0 <= luIndex < 7.
        Animator* GetAnimator(u32 luIndex);

        // ---- declared here, bodies owned by the class:BrnGui::HelpBar TU --------------
        // These are ledger-`reviewed` but defined nowhere in the tree yet (the committed
        // .cpp carries only the two accessors above), so consumers compile but will not
        // link until that TU lands. Signatures are X360-attested from the call sites and
        // the asserts.

        // @0x824EA4F8 - name the bar, then Construct + Animator-construct liNumItems
        // "<name><i>" sub-components. Asserts lpacName/lpStateInterface non-null and
        // liNumItems <= KI_MAX_ITEMS. (Args from the asm: r4 name, r5 count, r6 state
        // interface, r7 parent name.)
        void Construct(const char* lpacName, s32 liNumItems,
                       CgsGui::StateInterface* lpStateInterface, const char* lpacParentName);
        // @0x824E2570 - post-load wiring of the bar's apt sub-components.
        void SetupComponent();
        // @0x824E26C8 - register the bar's expected apt components on the cache.
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache);
        // @0x824E91A8 - reset the appended items (live count -> 0; per-item resets).
        void Clear();
        // @0x824E5E38 - append one {text, button} item; returns the item index. Both
        // button arguments are asserted < 0x10 ("Invalid button"). The second one is the
        // OPTIONAL second icon, and the call sites pass E_PADBUTTON_INVISIBLE (15) for
        // "none" -- it is NOT a pad bitmask (dumped 0x824E5E38: r20 is compared against
        // the same enum bound and indexes the same icon table as the first).
        // Signature is the DecFIGS DWARF's (BrnHelpBar.h:86/212) -- s32 return and two
        // ButtonIconComponent::EPadButton parameters, NOT the (void, u32, u32) an earlier
        // wave-I draft proposed.
        s32 AppendHelpBarItem(const char* lpacText,
                              ButtonIconComponent::EPadButton leButton,
                              ButtonIconComponent::EPadButton leSecondButton);
        // @0x824E92C0 - per-frame item-info / animator update.
        void Update(f32 lfTime);

    private:
        // ---- recovered layout (guest 32-bit offsets) -----------------------------------
        ItemRecord maItems[KI_MAX_ITEMS];   // +0x000..+0xBB3  (7 * 0x1AC)

        // Unrecovered body between the item records and the animator array.
        u8  maBodyReserved[0xC40 - KI_ITEM_RECORD_STRIDE * KI_MAX_ITEMS];   // +0xBB4..+0xC3F

        Animator maAnimators[KI_MAX_ITEMS]; // +0xC40..        (7 * 0x288)
    };

    // Element strides are load-bearing (they reproduce the X360 mulli factors).
    static_assert(sizeof(HelpBar::ItemRecord) == 0x1AC, "HelpBar item-record stride");
    static_assert(sizeof(HelpBar::Animator)   == 0x288, "HelpBar animator stride");
}
