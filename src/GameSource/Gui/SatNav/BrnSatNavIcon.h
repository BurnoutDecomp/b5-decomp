#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"        // Vector2 (rw::math::vpu::Vector2 - alignas(16) {x,y,z,w})
#include "GameSource/Gui/BrnGuiTextField.h"   // BrnGui::TextField (CrashNavMapIcon::mIconText, copied by operator=)

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
    // The Flapt animation component that hosts a SatNavMapIcon. The icon is embedded at +32
    // within this component, so SatNavMapIcon::SetState reaches back to it (container_of) to
    // drive the animation. Defined in another TU; declared here for that reach.
    struct FlaptIconComponent
    {
        void* GotoAndStopLabel(const void* lpLabel);
    };

    // Per-state Flapt animation-label table (X360 off_82F25A00), owned by another TU.
    extern void* const gaSatNavStateLabels[];

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

        virtual void      SetPosition(Vector2 lv2Position) { mv2Position = lv2Position; }
        virtual void      SetRotation(f32 lfRotationInRadians) { mfRotationInRadians = lfRotationInRadians; }
        virtual void      SetAlpha(f32 lfAlpha) { mfAlpha = lfAlpha; }
        virtual void      SetState(IconState leState) { meState = leState; }
        virtual Vector2   GetPosition() const { return mv2Position; }
        virtual IconState GetState() const { return meState; }

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

        f32 GetAlpha() const { return mfAlpha; }        // @ 0x827DD... (BrnSatNavIcon.h:511)
        f32 GetRotation() const { return mfRotationInRadians; }

        // @ 0x827DD... -- the icon's full constructor. A separate, not-yet-reconstructed
        // TU; declared-only here so the @0x827DD610 adjustor thunk can forward to it.
        CrashNavMapIcon* Construct(s32 liA, s32 liB, s32 liC);

        // @ 0x827DD610 (Construct`adjustor{144}') -- a compiler-emitted base-adjusting
        // thunk: shifts `this` back by 0x90 to the full object, then forwards to Construct.
        // This is the reconstructed TU; Construct (above) is out of scope.
        CrashNavMapIcon* ConstructAdjustor144(s32 liA, s32 liB, s32 liC);

        // @ 0x8244BA18 -- copy-assign from lrSource. The X360 byte-copies the icon EXCEPT
        // the +0x00 vtable slot (copy anchor is this+0x04), copies the embedded TextField
        // via TextField::operator=, then the two trailing dirty flags. Reconstructed as a
        // member-by-name copy of the modelled fields (the +0x00 vtable is not assigned --
        // matching the X360, and matching C++ copy-assign of a polymorphic object).
        CrashNavMapIcon& operator=(const CrashNavMapIcon& lrSource);

    private:
        // ADDITIVE GROW (brn-gui group, CrashNavMapIcon operator= @0x8244BA18): the icon's
        // by-value TextField, previously a comment-placeholder, is now a REAL member so
        // operator= can copy it (X360: TextField::operator=(this+0xC4, src+0xC4); the
        // 0x128-byte field then places the two dirty flags at +0x1EC/+0x1ED, exactly after
        // it). Adding it does not reorder/retype/remove any pre-existing REAL member (the
        // prior layout modelled only base fields + muId + the two dirty bytes).
        TextField mIconText;     // X360 +0xC4 (copied by operator= via TextField::operator=)

        bool mbIsDirty;          // X360 +0x1EC (raised by the Set* mutators / copied by operator=)
        bool mbDirtyIconState;   // X360 +0x1ED (copied by operator=)
    };

    // Sat-nav map icon. State changes drive the hosting Flapt component's animation.
    class SatNavMapIcon : public MapIconBrnBase
    {
    public:
        // @ 0x8280FFC0
        Vector2 GetPosition() const override { return mv2Position; }

        // @ 0x827DD7C0
        IconState GetState() const override { return meState; }

        // @ 0x827DD790 - on a state change, drive the hosting Flapt component to the new state's
        // animation label. The icon is embedded at +32 inside that component, so this reaches the
        // *containing* object (container_of) - not an own-field access.
        void SetState(IconState leState) override
        {
            if (leState != meState)
            {
                meState = leState;
                FlaptIconComponent* lpComponent =
                    reinterpret_cast<FlaptIconComponent*>(reinterpret_cast<char*>(this) - 32);
                lpComponent->GotoAndStopLabel(gaSatNavStateLabels[leState]);
            }
        }

    private:
        // [TextFieldRef mIconText: real member; type not yet reconstructed - omitted]
    };
}
