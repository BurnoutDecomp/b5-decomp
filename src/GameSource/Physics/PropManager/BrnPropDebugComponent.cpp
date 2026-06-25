#include "GameSource/Physics/PropManager/BrnPropDebugComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/Physics/PropManager/BrnPropTuning.h"

namespace BrnPhysics
{
namespace Props
{
    void PropDebugComponent::Construct(PropManager* lpPropManager)
    {
        CgsDev::DebugComponent::Construct();
        CGS_ASSERT(lpPropManager, "lpPropManager != NULL");

        mpPropManager = lpPropManager;
        mbRenderStats = true;
        mbRenderWorldContacts = false;
    }

    const char* PropDebugComponent::GetName() const
    {
        return "Prop Manager";
    }

    void PropDebugComponent::OnActivate()
    {
        const char* KPC_ANTI_HERDING_GROUP = "Anti herding...";

        RegisterVariable(&mfAntiHerdUpwardScale, KPC_ANTI_HERDING_GROUP, "Upward Scale");
        SetStep(&mfAntiHerdUpwardScale, 0.01f);
        SetChangeCallback(&mfAntiHerdUpwardScale, OnChangeAntiHerdUpwardScale, this);
        mfAntiHerdUpwardScale = gfAntiHerdUpwardScale;

        RegisterVariable(&mfAntiHerdSideScale, KPC_ANTI_HERDING_GROUP, "Side Scale");
        SetStep(&mfAntiHerdSideScale, 0.01f);
        SetChangeCallback(&mfAntiHerdSideScale, OnChangeAntiHerdSideScale, this);
        mfAntiHerdSideScale = gfAntiHerdSideScale;

        RegisterVariable(&mfAntiHerdHighSpeedSideScale, KPC_ANTI_HERDING_GROUP,
                         "High Speed Side Scale");
        SetStep(&mfAntiHerdHighSpeedSideScale, 0.01f);
        SetChangeCallback(&mfAntiHerdHighSpeedSideScale,
                          OnChangeAntiHerdHighSpeedSideScale, this);
        mfAntiHerdHighSpeedSideScale = gfAntiHerdHighSpeedSideScale;

        RegisterVariable(&mfMaxSpeedForSideForce, KPC_ANTI_HERDING_GROUP,
                         "Max Speed for side force");
        SetChangeCallback(&mfMaxSpeedForSideForce, OnChangeMaxSpeedForSideForce, this);
        mfMaxSpeedForSideForce = gfMaxSpeedForSideForce;

        RegisterVariable(&mfAntiHerdSpeedClamp, KPC_ANTI_HERDING_GROUP, "Speed Clamp");
        SetChangeCallback(&mfAntiHerdSpeedClamp, OnChangeAntiHerdSpeedClamp, this);
        mfAntiHerdSpeedClamp = gfAntiHerdSpeedClamp;

        RegisterVariable(&mfInertiaScale, "Inertia Scale");
        SetRange(&mfInertiaScale, 0.0f, 10.0f);
        SetStep(&mfInertiaScale, 0.1f);
        SetChangeCallback(&mfInertiaScale, OnChangeInertiaScale, this);
        mfInertiaScale = gfInertiaScale;

        RegisterVariable(&mfGravityScale, "Gravity Scale");
        SetRange(&mfGravityScale, 0.0f, 10.0f);
        SetStep(&mfGravityScale, 0.1f);
        SetChangeCallback(&mfGravityScale, OnChangeGravityScale, this);
        mfGravityScale = gfGravityScale;

        RegisterVariable(&gfLinearDrag, "Linear Drag");
        SetRange(&gfLinearDrag, 0.0f, 1.0f);
        SetStep(&gfLinearDrag, 0.001f);

        RegisterVariable(&gfAngularDrag, "Angular Drag");
        SetRange(&gfAngularDrag, 0.0f, 1.0f);
        SetStep(&gfAngularDrag, 0.001f);

        RegisterVariable(&gfMaxLinearVelocity, "Max Linear Velocity");
        SetRange(&gfMaxLinearVelocity, 0.0f, 1000.0f);
        SetStep(&gfMaxLinearVelocity, 1.0f);

        RegisterVariable(&gfMaxAngularVelocity, "Max Angular Velocity");
        SetRange(&gfMaxAngularVelocity, 0.0f, 1000.0f);
        SetStep(&gfMaxAngularVelocity, 1.0f);

        RegisterVariable(&gfRestitution, "Restitution");
        SetRange(&gfRestitution, 0.0f, 1.0f);
        SetStep(&gfRestitution, 0.01f);

        RegisterVariable(&mbRenderStats, "Render prop manager stats");
        RegisterVariable(&mpPropManager->mbRenderCentreOfMass, "Render prop centre of mass");
        RegisterVariable(&mbRenderWorldContacts, "Render prop contacts");
        RegisterVariable(&mpPropManager->mbDisableFreezing, "Disable prop freezing");
    }

    void PropDebugComponent::OnRegister()
    {
        CgsDev::DebugComponent::OnRegister();
    }

    void PropDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        if (mbRenderStats)
            RenderStats(lpRender);
    }
}
}
