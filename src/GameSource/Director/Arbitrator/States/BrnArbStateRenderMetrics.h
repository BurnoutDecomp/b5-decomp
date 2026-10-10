#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RENDER_METRICS_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RENDER_METRICS_H

#include "types.hpp"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<>

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateRenderMetrics.h
//
// BrnDirector::ArbStateRenderMetrics -- the arbitrator state the dev tools' StartRenderMetrics
// command switches the director into. It allocates one BehaviourRenderMetrics (the probe camera
// the tool steers over GameTalk), copies the camera that behaviour produces into the state's own
// camera every frame, and -- once the roaming state accepts a Prepare -- hands control back to
// roaming and releases itself. Embedded by value in the Arbitrator.
//
// Layout (declaration order; console offsets, the host Camera and handle are wider):
//   +0x010  ArbitratorState::mCamera
//   +0x180  mCam      (BehaviourHandle, 0x14)
//   +0x194  meState
//
// Console vtable: Construct, Prepare, Update, Release, Destruct (the base's empty body),
// CanRun (the base's), GetName.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera { class BehaviourRenderMetrics; }

    class ArbStateRenderMetrics : public ArbitratorState
    {
    public:
        // The state machine; the Update switch key.
        enum EState
        {
            E_STATE_INACTIVE            = 0,
            E_STATE_PREPARING           = 1,
            E_STATE_ACTIVE              = 2,
            E_STATE_CHANGING_TO_ROAMING = 3,
            E_STATE_RELEASING           = 4,

            E_NUM_STATES                = 5
        };

        // ---- ArbitratorState virtual overrides -------------------------------------------
        void        Construct() override;
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;
        const char* GetName() const override;

    private:
        Camera::BehaviourHandle<Camera::BehaviourRenderMetrics> mCam;      // +0x180
        EState                                                  meState;   // +0x194
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RENDER_METRICS_H
