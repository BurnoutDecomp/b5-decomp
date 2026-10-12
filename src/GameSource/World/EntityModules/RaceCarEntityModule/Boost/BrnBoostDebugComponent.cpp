#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostDebugComponent.h"

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnWorld
{
    void BoostDebugComponent::Construct(RaceCarEntityModule* lpRaceCarEntityModule)
    {
        CgsDev::DebugComponent::Construct();
        mpRaceCarEntityModule = lpRaceCarEntityModule;
        miBoostRules = 1;
        miActionTextModifiedTime = 0;
    }

    void BoostDebugComponent::Destruct()
    {
        if (mpRaceCarEntityModule == nullptr)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "mpRaceCarEntityModule != NULL",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/EntityModules/RaceCarEntityModule/Boost/BrnBoostDebugComponent.cpp",
                68);
            CgsDev::Assert::EndAssert();
        }
        mpRaceCarEntityModule = nullptr;
        // ARTIST822F3E18 calls the empty ICF body8284CB38. The current
        // generic DebugComponent::Destruct also clears mbActive, which this
        // original Boost destruction path does not store.
    }

    const char* BoostDebugComponent::GetName() const
    {
        return "Boost";
    }

    const char* BoostDebugComponent::GetPath() const
    {
        // The Boost vtable's GetPath slot is ICF-folded with
        // CrashPlayDebugComponent::GetPath @0x82312480.
        return "Gameplay";
    }

    void BoostDebugComponent::OnActivate()
    {
        BoostManager& lrBoostManager = mpRaceCarEntityModule->mBoostManager;
        BoostBurnout5& lrBoostBurnout5 = lrBoostManager.mBoostBurnout5;

        miBoostRules = static_cast<s32>(lrBoostManager.GetBoostStrategyId());

        RegisterFunction(&BoostDebugComponent::FillBoostCallback, this, "Fill/empty boost");
        RegisterFunction(&BoostDebugComponent::OnCrashCallback, this, "On crash");
        RegisterFunction(&BoostDebugComponent::OnTakedownCallback, this, "On takedown");
        RegisterFunction(&BoostDebugComponent::OnStuntCallback, this, "On Stunt");
        RegisterFunction(&BoostDebugComponent::OnPreBurnoutWow,
                         &lrBoostBurnout5.miChainSize, "Set Chains To 999");

        RegisterVariable(&lrBoostBurnout5.mbAllowBoostEarning, "Allow Boost Earning");
        RegisterVariable(&lrBoostBurnout5.mbAddRemoveChunkMode, "Is Boost Chunk Mode Active");
        SetChangeCallback(&lrBoostBurnout5.mbAddRemoveChunkMode,
                          &BoostDebugComponent::OnAddRemoveChunkMode, &lrBoostBurnout5);

        RegisterFunction(&BoostDebugComponent::UpdateBoostRulesCallback,
                         this, "Reload Attribsys Variables");
        RegisterBoostVariables();
    }

    void BoostDebugComponent::RegisterBoostVariables()
    {
        BoostBurnout5& lrBoostBurnout5 = mpRaceCarEntityModule->mBoostManager.mBoostBurnout5;

        RegisterVariable(&lrBoostBurnout5.mbInfiniteBoost, "Infinite boost");
        RegisterVariable(&lrBoostBurnout5.mfMaxBoost, "Max boost");
        RegisterVariable(&lrBoostBurnout5.mfBoostAmount, "Boost amount");
        RegisterVariable(&lrBoostBurnout5.mfHiddenBoost, "Hidden boost");
        RegisterVariable(&lrBoostBurnout5.mbBoostInterrupted, "IsInterrupted");
        RegisterVariable(&lrBoostBurnout5.mbBoosting, "Boosting");
        RegisterVariable(&lrBoostBurnout5.miChainSize, "Chain Size");
        RegisterVariable(&lrBoostBurnout5.mbIsTailgating, "IsTailgating");

        SetRange(&miBoostRules, 1, 4);
        SetReadOnly(&lrBoostBurnout5.mfMaxBoost, true);
        SetReadOnly(&lrBoostBurnout5.mfBoostAmount, true);
        SetReadOnly(&lrBoostBurnout5.mbBoosting, true);
    }

    void BoostDebugComponent::UnregisterBoostVariables()
    {
        BoostBurnout5& lrBoostBurnout5 = mpRaceCarEntityModule->mBoostManager.mBoostBurnout5;

        UnregisterVariable(&lrBoostBurnout5.mbInfiniteBoost);
        UnregisterVariable(&lrBoostBurnout5.mfMaxBoost);
        UnregisterVariable(&lrBoostBurnout5.mfBoostAmount);
        UnregisterVariable(&lrBoostBurnout5.mbBoosting);
        UnregisterVariable(&lrBoostBurnout5.mfHiddenBoost);
        UnregisterVariable(&lrBoostBurnout5.mbBoostInterrupted);
        UnregisterVariable(&lrBoostBurnout5.miChainSize);
        UnregisterVariable(&lrBoostBurnout5.mbIsTailgating);
        UnregisterVariable(&lrBoostBurnout5.mbAddRemoveChunkMode);
        UnregisterVariable(&lrBoostBurnout5.mbAllowBoostEarning);
    }

    void BoostDebugComponent::FillBoostCallback(void* lpUserData)
    {
        BoostDebugComponent* lpComponent = static_cast<BoostDebugComponent*>(lpUserData);
        BoostBurnout5& lrBoostBurnout5 = lpComponent->mpRaceCarEntityModule->mBoostManager.mBoostBurnout5;

        f32 lfBoostAmount = lrBoostBurnout5.mfMaxBoost;
        if (lrBoostBurnout5.mfBoostAmount >= lfBoostAmount)
        {
            lfBoostAmount = 0.0f;
        }
        lrBoostBurnout5.mfBoostAmount = lfBoostAmount;
    }

    void BoostDebugComponent::OnTakedownCallback(void* lpUserData)
    {
        BoostDebugComponent* lpComponent = static_cast<BoostDebugComponent*>(lpUserData);
        lpComponent->mpRaceCarEntityModule->mBoostManager.GetBoostStrategy()->OnTakedown();
    }

    void BoostDebugComponent::OnStuntCallback(void* lpUserData)
    {
        BoostDebugComponent* lpComponent = static_cast<BoostDebugComponent*>(lpUserData);
        lpComponent->mpRaceCarEntityModule->mBoostManager.GetBoostStrategy()->OnStuntCompletion(
            static_cast<BrnGameState::StuntElementType>(2));
    }

    void BoostDebugComponent::OnCrashCallback(void* lpUserData)
    {
        BoostDebugComponent* lpComponent = static_cast<BoostDebugComponent*>(lpUserData);
        lpComponent->mpRaceCarEntityModule->mBoostManager.GetBoostStrategy()->OnTakenDownByAIOrPlayer();
    }

    void BoostDebugComponent::UpdateBoostRulesCallback(void* lpUserData)
    {
        BoostDebugComponent* lpComponent = static_cast<BoostDebugComponent*>(lpUserData);
        lpComponent->UnregisterBoostVariables();
        lpComponent->mpRaceCarEntityModule->mBoostManager.Prepare();
        lpComponent->RegisterBoostVariables();
    }

    void BoostDebugComponent::OnAddRemoveChunkMode(void*, void* lpUserData)
    {
        BoostBurnout5* lpBoostBurnout5 = static_cast<BoostBurnout5*>(lpUserData);
        if (!lpBoostBurnout5->mbAddRemoveChunkMode)
        {
            lpBoostBurnout5->miBoostLevel = 4; //ARTIST822D5280: base+0x104
            lpBoostBurnout5->UpdateMaxBoost(false);
        }
    }

    void BoostDebugComponent::OnPreBurnoutWow(void* lpUserData)
    {
        *static_cast<s32*>(lpUserData) = 999;
    }
}
