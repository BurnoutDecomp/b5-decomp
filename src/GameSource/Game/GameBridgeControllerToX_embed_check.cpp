// Embed-check for the BrnGame::BrnGameModule controller-bridge family. Forces the six bridge
// methods + the PadOutputInformation field layout to be referenced so the gate compiles them.
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeControllerToX.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"

namespace
{
    // Exercise the grown PadOutputInformation field surface the bridges read by name.
    void ExercisePadRecord(const CgsInput::InputIO::PadOutputInformation& rPad)
    {
        volatile f32 lf = rPad.mfStickLX + rPad.maActionInfo[0].mfValue;
        volatile u32 lu = rPad.muConnectionWord | rPad.meControllerState
                          | rPad.maActionInfo[0].muStatus | rPad.mbDisconnected;
        (void)lf; (void)lu;
    }

    // Take the addresses of the six bridge methods so they are emitted + type-checked.
    void ReferenceBridges()
    {
        const CgsInput::InputIO::PadOutputInformation* (BrnGame::BrnGameModule::*lpGet)(
            const CgsInput::InputIO::OutputBuffer*, s32*) = &BrnGame::BrnGameModule::GetPadInfoForPlayer0;
        void (BrnGame::BrnGameModule::*lpMap)(BrnGame::DebugControllerImage*,
            const CgsInput::InputIO::ActionInfo*) = &BrnGame::BrnGameModule::MapActionInfoToDebugController;
        void (BrnGame::BrnGameModule::*lpDir)(BrnDirector::DirectorIO::InputBuffer*,
            const CgsInput::InputIO::OutputBuffer*) = &BrnGame::BrnGameModule::BridgeControllerToDirector;
        void (BrnGame::BrnGameModule::*lpWorld)(BrnWorldIO::UpdateInputBuffer*,
            const CgsInput::InputIO::OutputBuffer*) = &BrnGame::BrnGameModule::BridgeControllerToWorld;
        void (BrnGame::BrnGameModule::*lpGs)(BrnGameState::GameStateModule*,
            const CgsInput::InputIO::OutputBuffer*, s32) = &BrnGame::BrnGameModule::BridgeControllerToGameState;
        void (BrnGame::BrnGameModule::*lpGui)(CgsGui::CgsGuiModuleIO::InputBuffer*,
            const CgsInput::InputIO::OutputBuffer*) = &BrnGame::BrnGameModule::BridgeControllerToGui;
        (void)lpGet; (void)lpMap; (void)lpDir; (void)lpWorld; (void)lpGs; (void)lpGui;
    }
}

extern "C" void GameBridgeControllerToX_embed_check()
{
    ReferenceBridges();
    (void)&ExercisePadRecord;
}
