#ifndef CGS_APT_ANIMATOR_H
#define CGS_APT_ANIMATOR_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"                          // Array<AnimChannel,6>
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimChannel.h"      // CgsGui::AnimChannel (live channels)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"         // CgsGui::AnimData / AnimChannelData
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptObjectController.h" // CgsGui::ObjectController (held bridge)

// Pointer-only collaborator (only a pointer is stored / passed through):
namespace CgsGui { struct GuiComponent; }

// ============================================================================
// GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimator.h
//
// CgsGui::Animator - drives a set of keyframed CgsGui::AnimChannel tracks onto an
// apt (Scaleform-style) GUI display object through a held ObjectController. Given
// a target GuiComponent and a table of per-channel ActionScript variable names, it
// builds an ObjectController bridge to the component, registers it, then per frame
// interpolates each active channel and pushes the value onto the object's script
// variable (SetAptValues).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Member set / order / names and the
// channel-name constants are from the DecFIGS DWARF (CgsAptAnimator.h, a high-
// confidence near-ancestor of ARTIST); the X360 word offsets confirm the layout:
//
//   _vptr                 +0x00  (virtual SetAnimation/SetAnimationChannel/SetAptValue[s])
//   mpTarget       [+0x04] GuiComponent*   - X360 a1[1]   (h:125)
//   mpaChannelTypeNames[+0x08] const char(*)[KI_MAX_CHANNEL_NAME_LEN]
//                                          - X360 a1[2]; channel c's variable name is
//                                            &mpaChannelTypeNames[c][0] (base + 10*c,
//                                            confirmed by SetAnimationChannel's
//                                            10*channel byte index) (h:128)
//   miNumChannels  [+0x0C] s32             - X360 a1[3]   (h:129)
//   mpObjectController[+0x10] ObjectController* - X360 a1[4] (h:131)
//   maAnimChannels [+0x14] Array<AnimChannel,6> - X360 a1+5; its live-count word at
//                                            a1[59] (+0xEC == +0x14 + 6*0x24) (h:133)
//
// Note (asm vs DWARF): the DWARF spells mpaChannelTypeNames as an inline char*[10],
// but the X360 stores a single pointer at +0x08 and lays miNumChannels at +0x0C, so
// on the ARTIST spine it is a pointer into a flat char[N][KI_MAX_CHANNEL_NAME_LEN]
// name table (asm is rung-1 authority); modelled as char(*)[KI_MAX_CHANNEL_NAME_LEN].
//
// x64 note: members are accessed BY NAME; the host (x64) ABI widens pointers so the
// absolute byte offsets differ from the X360's - expected and fine.
// ============================================================================

namespace CgsGui
{
    struct Animator
    {
        // CgsAptAnimator.h:53 (DWARF) - per-channel ActionScript variable-name length.
        static const s32 KI_MAX_CHANNEL_NAME_LEN = 10;

        // CgsAptAnimator.h:127 (DWARF) - the default channel->script-variable name
        // table (one fixed-width name per AnimData::AnimatorChannel). Defined in the .cpp.
        static const char maDefaultChannelTypeNames[6][KI_MAX_CHANNEL_NAME_LEN];

        // Default the animator into the un-constructed state (no target/controller).
        Animator()
            : mpTarget(0)
            , mpaChannelTypeNames(0)
            , miNumChannels(0)
            , mpObjectController(0)
        {
        }

        // The X360 Animator vtable holds only the four animation virtuals below (the
        // DecFIGS DWARF lists no virtual destructor for Animator); the destructor is
        // non-virtual so the vtable slot order is SetAnimationChannel(0) / SetAnimation(1)
        // / SetAptValues(2) / SetAptValue(3) - matching the internal virtual dispatch
        // (Update calls vtable[2] SetAptValues, SetAptValues calls vtable[3] SetAptValue).
        ~Animator() {}

        // CgsAptAnimator.cpp:57 (X360 0x82858F58) - bind the target component + the
        // channel-name table, default the channel list, build + register the
        // ObjectController bridge to the target.
        void Construct(GuiComponent* lpTarget,
                       const char (*lpaChannelTypeNames)[KI_MAX_CHANNEL_NAME_LEN],
                       s32 liNumChannels);

        // CgsAptAnimator.cpp:96 (X360 0x82850280) - unregister + drop the bridge.
        void Destruct();

        // CgsAptAnimator.cpp:156 (X360 0x828534E8) - advance every active channel by
        // lTime, then push the interpolated values onto the apt object (SetAptValues
        // via the virtual sink).
        void Update(AnimChannelData::Time lTime);

        // CgsAptAnimator.h:159 (DWARF) - the held ObjectController bridge.
        ObjectController* GetObjectController() { return mpObjectController; }

        // CgsAptAnimator.cpp:183 (X360 0x82853678) - set one keyframed channel: copy the
        // channel data, offset its times by lStartTime, fold the current object value in
        // for object-space animation modes, and arm the live AnimChannel.
        virtual void SetAnimationChannel(AnimData::AnimatorChannel eChannel,
                                         const AnimChannelData* lpChannelData,
                                         AnimChannelData::Time lStartTime);

        // CgsAptAnimator.cpp:242 (X360 0x828538D0) - drive the whole animation: stop every
        // channel, then arm each channel the AnimData supplies via SetAnimationChannel.
        virtual void SetAnimation(AnimData* lpAnimData, AnimChannelData::Time lStartTime);

    protected:
        // CgsAptAnimator.cpp:272 (X360 0x828539E8) - push every active channel's current
        // interpolated value onto the apt display object (SetAptValue per channel).
        virtual void SetAptValues();

        // CgsAptAnimator.cpp:301 (X360) - set one named apt float variable on the object.
        // Bodied by its own ledger TU; declared for the SetAptValues virtual dispatch.
        virtual void SetAptValue(const char* lpacVariable, f32 lfValue);

    private:
        GuiComponent*     mpTarget;            // [+0x04] target component (h:125)
        const char      (*mpaChannelTypeNames)[KI_MAX_CHANNEL_NAME_LEN]; // [+0x08] name table (h:128)
        s32               miNumChannels;       // [+0x0C] active channel count (h:129)
        ObjectController* mpObjectController;   // [+0x10] apt control bridge (h:131)
        Array<AnimChannel, 6> maAnimChannels;   // [+0x14] live channels (h:133)
    };
}

#endif // CGS_APT_ANIMATOR_H
