#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

namespace BrnPhysics
{
namespace Props
{
    class PropManager;

    class PropDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(PropManager* lpPropManager);

        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;

    protected:
        const char* GetName() const override;
        void OnActivate() override;
        void OnRegister() override;

    private:
        void RenderStats(CgsDev::Debug2DImmediateRender* lpRender);

        static void OnChangeGravityScale(void* lpValue, void* lpUserData);
        static void OnChangeInertiaScale(void* lpValue, void* lpUserData);
        static void OnChangeAntiHerdUpwardScale(void* lpValue, void* lpUserData);
        static void OnChangeAntiHerdSideScale(void* lpValue, void* lpUserData);
        static void OnChangeAntiHerdHighSpeedSideScale(void* lpValue, void* lpUserData);
        static void OnChangeMaxSpeedForSideForce(void* lpValue, void* lpUserData);
        static void OnChangeAntiHerdSpeedClamp(void* lpValue, void* lpUserData);

        PropManager* mpPropManager;
        bool mbRenderStats;
        bool mbRenderWorldContacts;
        f32 mfGravityScale;
        f32 mfInertiaScale;
        f32 mfAntiHerdUpwardScale;
        f32 mfAntiHerdSideScale;
        f32 mfAntiHerdHighSpeedSideScale;
        f32 mfMaxSpeedForSideForce;
        f32 mfAntiHerdSpeedClamp;
    };
}
}
