#pragma once
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
namespace CgsPC::Debug
{
    // FLAG PC-platform leaf: bind the supported scalar portion of ARTIST's
    // traffic debug activation (82762530). Drawing/action controls whose engine
    // consumers are absent remain unregistered; see DEBUG_MENU_REPORT.md.
    struct TrafficControls
    {
        struct Component : CgsDev::DebugComponent
        {
            BrnTraffic::TrafficEntityModule* mpOwner = nullptr;
            const char* GetName() const override { return "Traffic"; }
            const char* GetPath() const override { return ""; }
            void OnActivate() override { TrafficControls::Register(*mpOwner); }
        };
        static void Attach(BrnTraffic::TrafficEntityModule& lrTraffic)
        {
            static Component sComponent;
            if (!sComponent.mpOwner)
            {
                sComponent.mpOwner = &lrTraffic;
                sComponent.Register();
            }
        }
        static void Register(BrnTraffic::TrafficEntityModule& lrTraffic)
        {
            CgsDev::DebugInterface lDebug;
            lDebug.RegisterVariable(&lrTraffic.mbDEBUGTurnTrafficOff, "Traffic", "Turn traffic off");
            lDebug.RegisterVariable(&lrTraffic.mbDEBUGStopTrafficMoving, "Traffic", "Stop traffic moving");
            lDebug.RegisterVariable(&lrTraffic.mbDEBUGDontRenderMeshes, "Traffic/DRAWING", "Hide Vehicle Meshes");
            lDebug.RegisterVariable(&lrTraffic.mfSpeedMultiplier, "Traffic", "Global speed multiplier");
            lDebug.SetRange(&lrTraffic.mfSpeedMultiplier, 0.0f, 50.0f);
            lDebug.SetStep(&lrTraffic.mfSpeedMultiplier, 0.05f);
            lDebug.RegisterVariable(&lrTraffic.mfDEBUGTrafficLightTimeMultiplier, "Traffic", "Global traffic light time multiplier");
            lDebug.SetRange(&lrTraffic.mfDEBUGTrafficLightTimeMultiplier, 0.0f, 50.0f);
            lDebug.SetStep(&lrTraffic.mfDEBUGTrafficLightTimeMultiplier, 0.05f);
            lDebug.RegisterVariable(&lrTraffic.mfGameModeDensityScale, "Traffic", "Game mode density scale");
            lDebug.SetRange(&lrTraffic.mfGameModeDensityScale, 0.0f, 16.0f);
            lDebug.SetStep(&lrTraffic.mfGameModeDensityScale, 0.05f);
            lDebug.RegisterVariable(&lrTraffic.mfTrafficAmountScale, "Traffic", "FINAL density scale");
            lDebug.SetReadOnly(&lrTraffic.mfTrafficAmountScale, true);
            lDebug.RegisterVariable(&lrTraffic.miDEBUGOverBudgetness, "Traffic/Stats", "Over budget-ness");
            lDebug.SetReadOnly(&lrTraffic.miDEBUGOverBudgetness, true);
            lDebug.RegisterVariable(&lrTraffic.mbDEBUGEnableAvoidance, "Traffic", "Enable Avoidance");
        }
    };
}
