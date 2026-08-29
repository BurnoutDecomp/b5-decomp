#pragma once

#include <cstddef>            // offsetof (SatNavMapIcon::SetState container_of)
#include "types.hpp"
#include "BrnCommonTypes.h"        // Vector2 (rw::math::vpu::Vector2 - alignas(16) {x,y,z,w})
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // [F3] CgsGui::GuiComponent (CrashNavIconComponent base)
#include "GameSource/Gui/BrnGuiTextField.h"   // BrnGui::TextField (CrashNavMapIcon::mIconText, copied by operator=)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h" // the REAL BrnGui::FlaptIconComponent (SetState reach-back)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"   // BrnFlapt::TextFieldRef (SatNavMapIcon::mIconText; Construct zeroes it)

namespace CgsGui { struct StateInterface; }   // Construct parameter (pointer-only)
namespace BrnFlapt { struct FileRef; }        // Prepare parameter (const-ref only)

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Layout from the DecFIGS dwarfdump
// (GameSource/Gui/SatNav/BrnSatNavIcon.h): both icon classes derive from BrnGui::MapIconBrnBase,
// which owns the position/rotation/alpha/state as NAMED members (the X360 setters are VMX stores
// of those fields - position is the 16-byte Vector2). Reconstructed addresses:
//   CrashNavMapIcon::SetAlpha    0x827DD5D0   ::SetRotation 0x827DD5B0
//   CrashNavMapIcon::SetState    0x827DD5F0   ::SetPosition 0x827DD568
//   SatNavMapIcon::GetPosition   0x8280FFC0   ::GetState    0x827DD7C0   ::SetState 0x827DD790
//
// Partial-layout note (same convention as BrnGameModule): only the members the reconstructed
// methods touch are declared. Each icon's `mIconText` (BrnGui::TextField by value in
// CrashNavMapIcon, TextFieldRef in SatNavMapIcon) plus the Construct/Prepare/Update/SetIconText
// virtuals are part of the full class but are NOT reconstructed here - they pull the (not-yet-
// reconstructed) TextField/Flapt/StateInterface types and land with the full TU. Real member
// names/types/order are preserved; offsets are X360 references (semantic parity, not byte-exact).

namespace BrnGui
{
    struct CrashNavIconComponent;   // the pool element that OWNS a CrashNavMapIcon (below)
    class  MapIconManager;          // the pool owner (BrnMapIconManager.h) -- friend, see below

    // ⭐ ODR FORK RETIRED (H3b, 2026-08-25): this header used to declare a LOCAL
    // `struct FlaptIconComponent { void* GotoAndStopLabel(const void*); }` stub for the
    // reach-back below. The REAL class (BrnGuiFlaptIconComponent.h, included above) has
    // GotoAndStopLabel as a *virtual* taking `const char*` -- the local fork mangled the
    // SetState call site to a symbol no TU defines AND bypassed the vtable dispatch. The
    // custom-renderer precedent (one bad header = N hollow shells) applies; the real
    // header is now the single definition.

    // Per-state Flapt animation-label table (X360 off_82F25A00, 53 entries -- one per
    // MapIconBrnBase::IconState). Values read off the image (h3b_dump.txt); defined in
    // BrnSatNavIcon.cpp.
    extern const char* const gaSatNavStateLabels[];

    // Common base of the map-icon classes (DWARF: BrnGui::MapIconBrnBase). Polymorphic in the
    // real game; the base setters write the shared fields, derived classes refine them (dirty
    // tracking / animation). Only the reconstructed virtuals are declared (see partial note).
    class MapIconBrnBase
    {
    public:
        enum IconState
        {
            E_ICONSTATE_INVISIBLE = 0,
            E_ICONSTATE_PLAYER_OFFLINE = 1,
            E_ICONSTATE_PLAYER_ONLINE = 2,
            E_ICONSTATE_PLAYER_YELLOW = 3,
            E_ICONSTATE_PLAYER_RED = 4,
            E_ICONSTATE_PLAYER_BLUE = 5,
            E_ICONSTATE_PLAYER_PINK = 6,
            E_ICONSTATE_PLAYER_GREEN = 7,
            E_ICONSTATE_PLAYER_ORANGE = 8,
            E_ICONSTATE_PLAYER_PURPLE = 9,
            E_ICONSTATE_PLAYER_CYAN = 10,
            E_ICONSTATE_PLAYER_WHITE = 11,
            E_ICONSTATE_PLAYER_GRAY = 12,
            E_ICONSTATE_PLAYER_BLACK = 13,
            E_ICONSTATE_RIVAL = 14,
            E_ICONSTATE_RIVAL_YELLOW = 15,
            E_ICONSTATE_RIVAL_RED = 16,
            E_ICONSTATE_RIVAL_BLUE = 17,
            E_ICONSTATE_RIVAL_PINK = 18,
            E_ICONSTATE_RIVAL_GREEN = 19,
            E_ICONSTATE_RIVAL_ORANGE = 20,
            E_ICONSTATE_RIVAL_PURPLE = 21,
            E_ICONSTATE_RIVAL_CYAN = 22,
            E_ICONSTATE_RIVAL_WHITE = 23,
            E_ICONSTATE_RIVAL_GRAY = 24,
            E_ICONSTATE_RIVAL_BLACK = 25,
            E_ICONSTATE_SATNAV_LANDMARK = 26,
            E_ICONSTATE_SATNAV_LANDMARKBEATEN = 27,
            E_ICONSTATE_SATNAV_LANDMARKTRACKED = 28,
            E_ICONSTATE_SATNAV_LANDMARKTRACKEDBEATEN = 29,
            E_ICONSTATE_SATNAV_LANDMARKFINISH = 30,
            E_ICONSTATE_SATNAV_LANDMARKFINISH_PENDING = 31,
            E_ICONSTATE_SATNAV_LANDMARKSTART = 32,
            E_ICONSTATE_SATNAV_LANDMARK_PENDING = 33,
            E_ICONSTATE_CRASHNAV_LANDMARKFINISH = 34,
            E_ICONSTATE_CRASHNAV_LANDMARKSTART = 35,
            E_ICONSTATE_CRASHNAV_JUNKYARD = 36,
            E_ICONSTATE_CRASHNAV_BODYSHOP = 37,
            E_ICONSTATE_CRASHNAV_GAS_STATION = 38,
            E_ICONSTATE_CRASHNAV_PAINT_SHOP = 39,
            E_ICONSTATE_SATNAV_FREEBURN_CHALLENGE = 40,
            E_ICONSTATE_SATNAV_JUNKYARD = 41,
            E_ICONSTATE_SATNAV_CAR_PARK = 42,
            E_ICONSTATE_SATNAV_BODYSHOP = 43,
            E_ICONSTATE_SATNAV_GAS_STATION = 44,
            E_ICONSTATE_SATNAV_PAINT_SHOP = 45,
            E_ICONSTATE_CRASHNAV_ONLINE_START_POINT = 46,
            E_ICONSTATE_CRASHNAV_ONLINE_FINISH_POINT = 47,
            E_ICONSTATE_CRASHNAV_ONLINE_CHECKPOINT = 48,
            E_ICONSTATE_CRASHNAV_PRERACE_START_POINT = 49,
            E_ICONSTATE_CRASHNAV_PRERACE_FINISH_POINT = 50,
            E_ICONSTATE_CRASHNAV_CUSTOMRENDERED_START_POINT = 51,
            E_ICONSTATE_CRASHNAV_CUSTOMRENDERED_FINISH_POINT = 52,
            E_ICONSTATE_COUNT = 53,
        };

        virtual ~MapIconBrnBase() = default;

        // [H3c] the X360 vtable (image @0x820CEB40 crash-nav / @0x820CEB68 sat-nav) pins the
        // FULL base virtual set + order: Construct, SetPosition, SetRotation, SetAlpha,
        // SetState, SetIconText, GetPosition, GetState, Update (the DWARF's declaration
        // order, verbatim). The three rows added here (Construct/SetIconText/Update) carry
        // empty defaults: the sat-nav vtable's SetIconText and Update slots both hold the
        // ICF-folded empty function @0x8284CB38 (a bare blr), so an empty default IS the
        // X360 behaviour for the derived class that doesn't refine them.
        virtual void      Construct(const char* lpcName, CgsGui::StateInterface* lpStateInterface,
                                    const char* lpcParentName) { (void)lpcName; (void)lpStateInterface; (void)lpcParentName; }
        virtual void      SetPosition(Vector2 lv2Position) { mv2Position = lv2Position; }
        virtual void      SetRotation(f32 lfRotationInRadians) { mfRotationInRadians = lfRotationInRadians; }
        virtual void      SetAlpha(f32 lfAlpha) { mfAlpha = lfAlpha; }
        virtual void      SetState(IconState leState) { meState = leState; }
        virtual void      SetIconText(const char* lpcText, bool lbAlreadyLocalised) { (void)lpcText; (void)lbAlreadyLocalised; }
        virtual Vector2   GetPosition() const { return mv2Position; }
        virtual IconState GetState() const { return meState; }
        virtual void      Update() {}

    protected:
        Vector2   mv2Position;          // X360 +16 (16-byte VMX vector)
        f32       mfRotationInRadians;  // X360 +32
        f32       mfAlpha;              // X360 +36
        IconState meState;              // X360 +40
    };

    // Crash-nav map icon. Tracks a dirty flag so the renderer re-pushes changed icons; SetState
    // additionally flags the icon-state as dirty. (DWARF: muId + TextField mIconText precede the
    // two dirty flags; mIconText is omitted here - see partial note.)
    class CrashNavMapIcon : public MapIconBrnBase
    {
    public:
        u32 muId;                       // X360 +44

        // @ 0x827DD568 - only flags dirty when the position actually changes.
        void SetPosition(Vector2 lv2Position) override
        {
            if (lv2Position.x != mv2Position.x || lv2Position.y != mv2Position.y ||
                lv2Position.z != mv2Position.z || lv2Position.w != mv2Position.w)
            {
                mv2Position = lv2Position;
                mbIsDirty = true;
            }
        }

        // @ 0x827DD5B0
        void SetRotation(f32 lfRotationInRadians) override
        {
            if (lfRotationInRadians != mfRotationInRadians)
            {
                mfRotationInRadians = lfRotationInRadians;
                mbIsDirty = true;
            }
        }

        // @ 0x827DD5D0
        void SetAlpha(f32 lfAlpha) override
        {
            if (lfAlpha != mfAlpha)
            {
                mfAlpha = lfAlpha;
                mbIsDirty = true;
            }
        }

        // @ 0x827DD5F0 - a state change dirties both the icon and its icon-state.
        void SetState(IconState leState) override
        {
            if (leState != meState)
            {
                meState = leState;
                mbIsDirty = true;
                mbDirtyIconState = true;
            }
        }

        // @ 0x8244F5D0 [F3 2026-08-29] -- the per-frame COMMIT: push the dirty transform
        // fields out to the hosting component's apt view state, then (separately) the
        // icon-state label. This is what makes the crash-nav pool visible at all --
        // MapIconManager::UpdateCrashNavIcons @0x825212C0 ends every icon with it. The
        // console reaches the component as `this - 0x90`; the body uses the host's own
        // container offset (see the SatNavMapIcon::SetState note above). Defined in
        // BrnSatNavIcon.cpp, below CrashNavIconComponent.
        void Update() override;

        f32 GetAlpha() const { return mfAlpha; }        // @ 0x827DD... (BrnSatNavIcon.h:511)
        f32 GetRotation() const { return mfRotationInRadians; }

        // @ 0x827DD610 (`CrashNavMapIcon::Construct'adjustor{144}'`) -- the ICON subobject's
        // vtable Construct slot (MapIconBrnBase slot 0). The X360 body is the two-instruction
        // MSVC base-adjusting thunk `addi r3,r3,-0x90 ; b CrashNavMapIcon::Construct`: it
        // walks `this` from the icon back to the hosting element (144 == 0x90 == the icon's
        // offset inside CrashNavIconComponent) and tail-calls the REAL constructor
        // @0x824481A8, whose `this` is the ELEMENT (see CrashNavIconComponent::Construct).
        //
        // [2026-08-29] the earlier `Construct(s32,s32,s32)` / `ConstructAdjustor144(s32,s32,s32)`
        // pair here was a PLACEHOLDER SIGNATURE -- the export shows three real parameters
        // (name / state interface / parent name) and the thunk is the vtable override itself,
        // so the two spellings collapse into this one override. The `ConstructAdjustor144`
        // name is retired; nothing outside this file referenced it.
        void Construct(const char* lpcName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpcParentName) override;

        // @ 0x8244BA18 -- copy-assign from lrSource. The X360 byte-copies the icon EXCEPT
        // the +0x00 vtable slot (copy anchor is this+0x04), copies the embedded TextField
        // via TextField::operator=, then the two trailing dirty flags. Reconstructed as a
        // member-by-name copy of the modelled fields (the +0x00 vtable is not assigned --
        // matching the X360, and matching C++ copy-assign of a polymorphic object).
        CrashNavMapIcon& operator=(const CrashNavMapIcon& lrSource);

    private:
        // [F3 2026-08-29] The element ctor @0x824481A8 runs with the ELEMENT as `this` and
        // writes the icon's text field + the two dirty bytes directly; the manager's
        // SetOwnerParameters bind pass @0x82520CE8 raises the same two bytes inline after
        // SetState. Both are console-inlined stores into this object, so both writers get
        // member access by name rather than a fabricated setter.
        friend struct CrashNavIconComponent;
        friend class  MapIconManager;

        // ADDITIVE GROW (brn-gui group, CrashNavMapIcon operator= @0x8244BA18): the icon's
        // by-value TextField, previously a comment-placeholder, is now a REAL member so
        // operator= can copy it (X360: TextField::operator=(this+0xC4, src+0xC4); the
        // 0x128-byte field then places the two dirty flags at +0x1EC/+0x1ED, exactly after
        // it). Adding it does not reorder/retype/remove any pre-existing REAL member (the
        // prior layout modelled only base fields + muId + the two dirty bytes).
        TextField mIconText;     // icon +0x34 == element +0xC4 (operator= / the element ctor's
                                 // virtual `(**(elem+0xC4))(elem+0xC4,"Icon",iface,elem+4)`)

        bool mbIsDirty;          // icon +0x15C == element +0x1EC (Set* mutators / Update clears it)
        bool mbDirtyIconState;   // icon +0x15D == element +0x1ED
    };

    // ADDITIVE GROW [F3 2026-08-29] -- the element of MapIconManager's 50-slot crash-nav
    // icon pool (DWARF BrnMapIconManager.h:435 `mCrashNavIcons`, X360 manager+0x9A0,
    // element stride 0x1F0). The element is a GuiComponent whose CrashNavMapIcon half
    // starts at element+0x90, X360-attested three ways:
    //   * `CrashNavMapIcon::Construct'adjustor{144}'` @0x827DD610 does `addi r3,r3,-0x90`
    //     -- 144 == 0x90 is the icon subobject's offset inside the element.
    //   * CrashNavMapIcon::Update @0x8244F5D0 calls CgsGui::GuiComponent::
    //     AddOutputAptViewState on `this - 0x90` four times.
    //   * CrashNavMapIcon::Construct @0x824481A8 takes the ELEMENT as `this`: it runs
    //     CgsGui::GuiComponent::Construct on it, then the icon's own text field at
    //     element+0xC4 (== icon+0x34 == mIconText) and the icon fields at element+0xB0 /
    //     +0xB4 / +0xB8 (== icon rotation / alpha / state) and +0x1EC / +0x1ED (the two
    //     dirty flags).
    // The X360 GuiComponent base is 0x90 bytes; on the host it is wider (the name buffer
    // plus a 64-bit state-interface pointer), so the icon is a NAMED MEMBER and every
    // reach-back goes through offsetof -- never a literal 0x90.
    struct CrashNavIconComponent : public CgsGui::GuiComponent
    {
        CrashNavMapIcon mIcon;   // X360 element+0x90 (host placement differs)

        // @ 0x824481A8 [F3 2026-08-29] -- THE element constructor. IDA labels the symbol
        // `BrnGui::CrashNavMapIcon::Construct`, but every offset in its body is
        // ELEMENT-relative (GuiComponent::Construct on `this`; the text field at +0xC4;
        // the icon fields at +0xB0/+0xB4/+0xB8; the dirty bytes at +0x1EC/+0x1ED), so on
        // the host it is the element's override of GuiComponent::Construct (vtable slot 0)
        // -- which is exactly what MapIconManager::SetOwnerParameters calls:
        // `(**(icon-0x90))(icon-0x90, name, iface, parentName)`. The icon-side vtable slot
        // holds the `adjustor{144}` thunk into here (CrashNavMapIcon::Construct above).
        // Body in BrnSatNavIcon.cpp.
        void Construct(const char* lpcName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpcParentName) override;
    };

    // Sat-nav map icon. Set* changes push straight through the hosting Flapt component's
    // MovieClipRef (the X360 reaches it at `this - 0x18` == component +0x08 == mAptRef;
    // here via container_of + the public GetMovieClipRef accessor -- a raw console offset
    // applied to a host object is exactly the wheel-blanking defect class).
    class SatNavMapIcon : public MapIconBrnBase
    {
    public:
        // @ 0x82448340 [H3c] -- the pool bind: assert the name/interface, construct the
        // HOSTING FlaptIconComponent (the X360 calls FlaptIconComponent::Construct on
        // `this - 32`), zero the text-field ref and poison the cached transform state
        // (rotation FLT_MAX / alpha -1 / state E_ICONSTATE_COUNT) so the first real Set*
        // always pushes through to the clip. Body in BrnSatNavIcon.cpp (needs the
        // component type complete).
        void Construct(const char* lpcName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpcParentName) override;

        // @ 0x827DD7C8 - push a changed position through to the apt clip (the X360
        // compares the {x,y} lanes -- the vrlimi merge replicates them across the
        // vector -- and on a change stores the full lane + MovieClipRef::SetPosition).
        void SetPosition(Vector2 lv2Position) override;

        // @ 0x827DD748 - push a changed rotation through to the apt clip.
        void SetRotation(f32 lfRotationInRadians) override;

        // @ 0x827DD768 - store a changed alpha. The X360 tail re-dispatches through its
        // OWN vtable slot (+0xC == SetAlpha itself); the second pass compares equal and
        // returns, so the net behaviour is the store -- the alpha never reaches the clip.
        void SetAlpha(f32 lfAlpha) override;

        // @ 0x8284CB38 (both) [H3c] -- the sat-nav vtable's SetIconText and Update slots
        // hold the same ICF-folded empty function (bare blr): both are no-ops. The
        // declarations exist in the DWARF (BrnSatNavIcon.cpp:395 / :350) but this build's
        // bodies are empty -- the inherited empty base defaults ARE the X360 behaviour,
        // so no override is declared here.

        // @ 0x8280FFC0
        Vector2 GetPosition() const override { return mv2Position; }

        // @ 0x827DD7C0
        IconState GetState() const override { return meState; }

        // @ 0x827DD790 - on a state change, drive the hosting Flapt component to the new state's
        // animation label. The icon is embedded inside a SatNavIconComponent (X360 at +32), so
        // this reaches the *containing* object (container_of) - not an own-field access. Body
        // below the host type (it needs SatNavIconComponent complete). ⭐ H3b: the console's
        // literal `this - 32` is replaced by the HOST layout's own offset -- a raw console
        // offset applied to a host object is exactly the wheel-blanking defect class.
        void SetState(IconState leState) override;

    private:
        // [H3c] DWARF BrnSatNavIcon.h:378 -- the icon's text-field handle (X360 icon
        // +0x30..+0x3B; Construct @0x82448340 zeroes its three pointer words).
        BrnFlapt::TextFieldRef mIconText;
    };

    // The Flapt animation component that hosts one SatNavMapIcon (the MapIconManager's
    // 16-element sat-nav icon pool, X360 element stride 0x60 with the icon at +32:
    // SetOwnerParameters @0x82520CE8 constructs the FlaptIconComponent base, then
    // SatNavMapIcon::Prepare on element+32's container).
    struct SatNavIconComponent : public FlaptIconComponent
    {
        SatNavMapIcon mIcon;   // X360 +32 (host offset differs -- pointers widen; reached by name)

        // @ 0x82448488 [H3c] (IDA "SatNavMapIcon::Prepare"; the X360 `this` is the
        // ELEMENT base -- SetOwnerParameters calls it on icon-32). Bind the named clip
        // through FlaptIconComponent::Prepare (no parent), then reset the embedded icon:
        // state COUNT (a direct store, forcing the SetState(INVISIBLE) below to fire the
        // label jump), rotation 0, alpha 0, state INVISIBLE, position zero.
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);
    };

    // @ 0x827DD790 (see the in-class comment).
    inline void SatNavMapIcon::SetState(IconState leState)
    {
        if (leState != meState)
        {
            meState = leState;
            SatNavIconComponent* lpComponent = reinterpret_cast<SatNavIconComponent*>(
                reinterpret_cast<char*>(this) - offsetof(SatNavIconComponent, mIcon));
            lpComponent->GotoAndStopLabel(gaSatNavStateLabels[leState]);
        }
    }

    // @ 0x827DD7C8 (see the in-class comment). The X360 vrlimi-merge compare reduces to
    // the {x,y} lane pair (lanes 2/3 are replaced by rotated copies of 0/1 on BOTH sides).
    inline void SatNavMapIcon::SetPosition(Vector2 lv2Position)
    {
        if (lv2Position.x != mv2Position.x || lv2Position.y != mv2Position.y)
        {
            mv2Position = lv2Position;
            SatNavIconComponent* lpComponent = reinterpret_cast<SatNavIconComponent*>(
                reinterpret_cast<char*>(this) - offsetof(SatNavIconComponent, mIcon));
            lpComponent->GetMovieClipRef().SetPosition(lv2Position);
        }
    }

    // @ 0x827DD748 (see the in-class comment).
    inline void SatNavMapIcon::SetRotation(f32 lfRotationInRadians)
    {
        if (lfRotationInRadians != mfRotationInRadians)
        {
            mfRotationInRadians = lfRotationInRadians;
            SatNavIconComponent* lpComponent = reinterpret_cast<SatNavIconComponent*>(
                reinterpret_cast<char*>(this) - offsetof(SatNavIconComponent, mIcon));
            lpComponent->GetMovieClipRef().SetRotation(lfRotationInRadians);
        }
    }

    // @ 0x827DD768 (see the in-class comment -- the store IS the whole net behaviour).
    inline void SatNavMapIcon::SetAlpha(f32 lfAlpha)
    {
        if (lfAlpha != mfAlpha)
        {
            mfAlpha = lfAlpha;
        }
    }
}
