#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

namespace BrnWorld
{
    class RaceCarEntityModule;

    // The boost page embedded in RaceCarEntityModule. Member names and shapes follow
    // DecFIGS; the registered state and callback behavior follow ARTIST.
    class BoostDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(RaceCarEntityModule* lpRaceCarEntityModule);
        void Destruct();

    protected:
        const char* GetName() const ;
        const char* GetPath() const ;
        void OnActivate() ;

    private:
        void RegisterBoostVariables();
        void UnregisterBoostVariables();

        static void FillBoostCallback(void* lpUserData);
        static void OnTakedownCallback(void* lpUserData);
        static void OnStuntCallback(void* lpUserData);
        static void OnCrashCallback(void* lpUserData);
        static void UpdateBoostRulesCallback(void* lpUserData);
        static void OnAddRemoveChunkMode(void* lpValue, void* lpUserData);
        static void OnPreBurnoutWow(void* lpUserData);

        RaceCarEntityModule* mpRaceCarEntityModule;
        s32 miBoostRules;
        s32 miActionTextModifiedTime;
    };
}
