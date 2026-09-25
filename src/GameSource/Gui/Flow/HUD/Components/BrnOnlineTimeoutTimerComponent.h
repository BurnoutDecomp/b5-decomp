#pragma once

// ===================================================================================
// BrnGui::OnlineTimeoutComponent -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/HUD/Components/BrnOnlineTimeoutTimerComponent.h
//
// The HUD "online timeout" countdown: an icon component ("OnlineEventTimeout_anim") that
// owns a counting-down timer field ("Timer_mc" / "TimerText_txt"). RaceMainHudState feeds
// it the GUI 108 time (SetTime, keeping the smallest time posted this frame) and ticks it
// once a frame (Update): a non-negative time is shown and the icon transitions in, a
// negative time transitions it out.
//
// Class shape and member names from the reference declaration: derives from
// FlaptIconComponent; mTimerField (the OnlineTimeOutTimerField typedef of
// FlaptTimerFieldComponent), mfNewTime, mbNewTimeSet, mbActive. Offsets from the console
// Construct / Update / SetTime stores (timer field +0x20, mfNewTime +0xA0, mbNewTimeSet
// +0xA4, mbActive +0xA5); the host layout is pointer-widened, members are used by name.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"       // FlaptIconComponent (base)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptTimerFieldComponent.h" // FlaptTimerFieldComponent (by value)

namespace CgsGui { struct StateInterface; }
namespace BrnFlapt { struct FileRef; }

namespace BrnGui
{
    struct OnlineTimeoutComponent : public FlaptIconComponent
    {
    public:
        typedef FlaptTimerFieldComponent OnlineTimeOutTimerField;

        void Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lacParentName);
        virtual void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile,
                             const char* lacParentName);
        void SetTime(f32 lfTime);
        void Update();
        void Show();

        bool IsActive() const { return mbActive; }

    private:
        void Transin();
        void Transout();

        // The timer field's colour ramp (the console Construct stores the /255 lanes inline).
        static const u8  KU_SAFERED     = 255;
        static const u8  KU_SAFEGREEN   = 204;
        static const u8  KU_SAFEBLUE    = 0;
        static const u8  KU_DANGERRED   = 153;
        static const u8  KU_DANGERGREEN = 16;
        static const u8  KU_DANGERBLUE  = 16;
        static const f32 KF_SAFE_BOUNDARY_TIME;     // 15.0f
        static const f32 KF_DANGER_BOUNDARY_TIME;   //  5.0f
        static const char KAC_TIMER_NAME[9];        // "Timer_mc"

        OnlineTimeOutTimerField mTimerField;   // +0x20
        f32                     mfNewTime;     // +0xA0
        bool                    mbNewTimeSet;  // +0xA4
        bool                    mbActive;      // +0xA5
    };
}
