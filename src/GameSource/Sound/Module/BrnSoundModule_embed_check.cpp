// Tiny embed check for the BrnSound-Module group: instantiate the reconstructed
// header surfaces so the gate exercises the buffer layouts (offset asserts), the
// lock-guarded accessor bodies, and the SoundLogicModule trigger-action search +
// registrar accessor. Not part of the shipped TU.
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"

namespace
{

void EmbedCheckIo(BrnSound::Module::Io::LogicInputBuffer& rIn,
                  BrnSound::Module::Io::RootInputBuffer&  rRoot)
{
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicle =
        rIn.GetVehicleInterface();
    (void)lpVehicle;

    BrnSound::Module::Io::PropUpdateNotificationQueue* lpQueue =
        rRoot.GetPropUpdateNotificationQueue();
    (void)lpQueue;
}

void EmbedCheckLogic(BrnSound::Module::SoundLogicModule& rModule)
{
    EntityId lEntityId;
    lEntityId.muValue = 0;

    const BrnGameState::GameStateModuleIO::SoundTriggerAction* lpAction =
        rModule.GetSoundTriggerAction(
            lEntityId,
            BrnGameState::GameStateModuleIO::SoundTriggerAction::E_TYPE_AT_ENTITY);
    (void)lpAction;

    // Reach the registrar through the IResourceRequester base too.
    BrnSound::Logic::IResourceRequester* lpReq = &rModule;
    BrnSound::Logic::ResourceRegistrar&  rReg  = lpReq->GetResourceRegistrar();
    (void)rReg;
}

} // namespace
