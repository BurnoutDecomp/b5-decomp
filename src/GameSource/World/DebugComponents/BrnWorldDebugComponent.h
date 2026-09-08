#pragma once

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

namespace BrnWorldIO { struct DebugController; }

namespace CgsDev
{
    struct Debug2DImmediateRender;
    struct Debug3DImmediateRender;
}

namespace BrnWorld
{
    class WorldModule;

    // Paradise's world-module development panel: graphics/ShaderLOD controls,
    // collision-world requests, the AI-player switch and the vehicle cannon HUD.
    // Declaration shape and member order are from the DecFIGS DWARF; behaviour is
    // reconstructed from the X360 ARTIST routines in BrnWorldDebugComponent.cpp.
    class WorldDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(WorldModule* lpWorldModule);
        void Update(const BrnWorldIO::DebugController* lpDebugController);

        void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay) override;
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay) override;

        bool HaveDebugController() const { return mbHaveDebugController; }
        bool GetWantsDebugControllerFocus() const { return HaveDebugController(); }

    protected:
        const char* GetName() const override;
        bool IsSimple() const override;
        void OnActivate() override;
        void OnRegister() override;

    private:
        void DrawVehicleGun(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        void DrawTarget(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        void DrawInactiveMessage(CgsDev::Debug2DImmediateRender* lpDisplay) const;
        void DrawActiveMessage(CgsDev::Debug2DImmediateRender* lpDisplay) const;

        void PrimeGun();
        void UnPrimeGun();
        void FireGun();

        static void ClearStoredFile(void* lpThis);
        static void UnPrimeGunCallback(void* lpThis);
        static void PrimeGunCallback(void* lpThis);
        static void FireGunCallback(void* lpThis);
        static void TriggerCollWorldValidate(void* lpThis);
        static void TriggerCollWorldInvalidate(void* lpThis);
        static void AIDrivesPlayerChanged(void* lpValue, void* lpThis);

        WorldModule* mpWorldModule;
        bool mbShowTarget;
        bool mbHaveDebugController;
        bool mbVehicleGunIsPrimed;
        Vector3 mVehiclePosition;
        Vector3 mVehicleDirection;
        f32 mfVehicleVelocity;
        bool mbAIDrivesPlayer;
    };
}
