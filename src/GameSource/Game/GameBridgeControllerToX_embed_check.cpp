// Compile-only check for the six BrnGame::BrnGameModule bridge signatures and the
// X360-attested scalar layout of the input pad record.
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeControllerToX.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"
#include <cstddef>

using CgsInput::InputIO::ActionInfo;
using CgsInput::InputIO::PadOutputInformation;

static_assert(sizeof(ActionInfo) == 8, "ActionInfo is an 8-byte X360 value/status pair");
static_assert(offsetof(ActionInfo, mfValue) == 0, "ActionInfo value @ +0");
static_assert(offsetof(ActionInfo, muStatus) == 4, "ActionInfo status @ +4");
static_assert(sizeof(PadOutputInformation) == 0x3A4, "pad record stride is 932 bytes");
static_assert(offsetof(PadOutputInformation, mfStickLX) == 0x00, "stick axis @ +0");
static_assert(offsetof(PadOutputInformation, maActionInfo) == 0x18, "ActionInfo table @ +0x18");
static_assert(offsetof(PadOutputInformation, muConnectionWord) == 0x398, "connection/player word @ +0x398");
static_assert(offsetof(PadOutputInformation, meControllerState) == 0x39C, "controller state @ +0x39C");
static_assert(offsetof(PadOutputInformation, mbDisconnected) == 0x3A0, "disconnected byte @ +0x3A0");

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
        void (BrnGame::BrnGameModule::*lpGs)(
            BrnGameState::GameStateModuleIO::PreWorldInputBuffer*,
            const CgsInput::InputIO::OutputBuffer*, bool) =
                &BrnGame::BrnGameModule::BridgeControllerToGameState;
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
