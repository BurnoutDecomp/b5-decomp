#include "GameSource/Sound/Vehicles/BrnAIVehicleState.h"
#include "GameSource/Sound/Vehicles/BrnAIVehicleStateManager.h"
#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Vehicles/BrnAISoundDiag.h"   // [DIAG] NOT IN THE X360 BINARY

#include <cstring>

// =============================================================================
// BrnSound::Vehicles::AIVehicleState -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
//   AIVehicleState::Attach        @ 0x826CA998
//   AIVehicleState::UpdateParams  @ 0x826EFD18
//   AIVehicleState::Detach        @ 0x826CACB8
//   AIVehicleState::GetTypeName   @ 0x826840C8
//   AIVehicleState::CreateObject  @ 0x826E2E90 (export-set hole; ppcdis)
//   ~AIVehicleState               @ 0x826CA8F8
//   sTypeInfo registration        @ 0x82C61DA8 (CRT init bank; desc 0x82F2E89C)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// @ 0x826CA8F8 -- the single source-level side effect is DestroyEffects(); the
// vtable re-installs and the allocator-routed free are the compiler's
// deleting-destructor thunk.
AIVehicleState::~AIVehicleState()
{
    DestroyEffects();
}

// Descriptor 0x82F2E89C: {0x20000, "AIVehicleState", base BrnState (0x82F2E7DC),
// &CreateObject}. The base descriptor is passed as null here like the committed
// sibling PlayerVehicleState (BrnState has no in-tree descriptor); the base chain
// only breaks ties between two descriptors with the same low 16 bits AND the same
// state byte, which never happens for state 2.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* AIVehicleState::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> sTypeInfo(
        0x20000, "AIVehicleState", 0, &AIVehicleState::CreateObject );
    return &sTypeInfo;
}

// @ 0x826E2E90: MemBase::operator new(0x530, "AIVehicleState", flavour) ;
// VehicleState::VehicleState ; vtable 0x820B3A3C.
CgsSound::Logic::State* AIVehicleState::CreateObject( u32 /*auType*/ )
{
    return new AIVehicleState();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* AIVehicleState::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// @ 0x826840C8: returns off_82F2E8A0 == the descriptor's name.
const char* AIVehicleState::GetTypeName() const
{
    return "AIVehicleState";
}

// CRT init bank @0x82C61DA8: `addi r3, r11, 82F2E89C ; b State::AddToClassTypeInfoArray`.
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* const
    gpAIVehicleStateReg = CgsSound::Logic::State::AddToClassTypeInfoArray(
        AIVehicleState::GetStaticTypeInfo() );

// ---------------------------------------------------------------------------
// AIVehicleState::Attach(void*)  @ 0x826CA998  (DWARF cpp:47)
//
//   mAttachInfo = *(AttachInfo*)apv;                              ; two std @+1216/+1224
//   rc = module->mpBrnLogicInputBuffer (assert) ->GetVehicleInterface()
//            ->GetRaceCarState(mAttachInfo.muVehicleIndex);
//   XMemCpy(&mVehiclePhysicsData, rc, 1120);
//   mVehicleBoostInfo = 0 (+1232..+1259: three floats + the flag bytes);
//   k = IntClamp(GetAIEngineAssignment(index) - 1, 0, 4);          ; byte_82FFB838[index]
//   name = off_82F2CBDC[k]; assert strlen < 13 ("String too long", CgsStringUtils.h:55)
//   strncpy(mcaEngineComponentName[E_ENGINE],  name, 13);
//   strncpy(mcaEngineComponentName[E_EXHAUST], name, 13);          ; the SAME engine
//   mEngineComponentKey[E_ENGINE] = mEngineComponentKey[E_EXHAUST]
//       = StringToKey(ConvertI64ToA(dword_820AA4B4[k], buf, 10));
//   State::Attach(this, apv);
// ---------------------------------------------------------------------------
void AIVehicleState::Attach( void* apvAttachment )
{
    const AttachInfo* lpInfo = static_cast<const AttachInfo*>( apvAttachment );
    mAttachInfo = *lpInfo;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
    BrnSound::Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    CGS_ASSERT( lpInput != 0, "mpBrnLogicInputBuffer" );
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicles =
        lpInput->GetVehicleInterface();
    const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>( mAttachInfo.muVehicleIndex );
    const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState = lpVehicles->GetRaceCarState( leIndex );
    mVehiclePhysicsData = *lpRaceCarState;

    std::memset( mauVehicleBoostInfo, 0, sizeof( mauVehicleBoostInfo ) );

    // CgsSound::Utils::IntClamp(assignment - 1, 0, 4) -- the clamp is written out
    // (the helper has no in-tree home); `(4 * clamp) & 0x3FC` is the table stride.
    s32 liEngine = static_cast<s32>( VehicleStateManager::GetAIEngineAssignment( mAttachInfo.muVehicleIndex ) ) - 1;
    if ( liEngine < 0 )
        liEngine = 0;
    if ( liEngine > AIVehicleStateManager::KI_NUMBER_OF_AUDIO_AI_ENGINES - 1 )
        liEngine = AIVehicleStateManager::KI_NUMBER_OF_AUDIO_AI_ENGINES - 1;

    const char* lpcEngineName = AIVehicleStateManager::GetAIEngineName( liEngine );
    CGS_ASSERT( std::strlen( lpcEngineName ) < 13, "String too long" );
    std::strncpy( mcaEngineComponentName[E_ENGINE],  lpcEngineName, 13 );
    CGS_ASSERT( std::strlen( lpcEngineName ) < 13, "String too long" );
    std::strncpy( mcaEngineComponentName[E_EXHAUST], lpcEngineName, 13 );

    const u64 luKey = AIVehicleStateManager::GetAIEngineAttribKey( liEngine );
    std::memcpy( &mEngineComponentKey[E_ENGINE],  &luKey, sizeof( luKey ) );
    std::memcpy( &mEngineComponentKey[E_EXHAUST], &luKey, sizeof( luKey ) );

    // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG
    if ( AISoundDiagLive() )
    {
        *CgsDev::Log::gpDebugPrint
            << "[ai-sound-attach] car=" << static_cast<s32>( mAttachInfo.muVehicleIndex )
            << " assignment=" << static_cast<s32>( VehicleStateManager::GetAIEngineAssignment( mAttachInfo.muVehicleIndex ) )
            << " engine=" << lpcEngineName
            << " key=" << CgsDev::E_PRINTMODE_HEXONCE << luKey
            << " entity=" << CgsDev::E_PRINTMODE_HEXONCE << static_cast<u64>( mVehiclePhysicsData.mEntityId.muValue )
            << " state=" << GetInstanceID()
            << "\n";
    }

    CgsSound::Logic::State::Attach( apvAttachment );
}

// ---------------------------------------------------------------------------
// AIVehicleState::UpdateParams(f32)  @ 0x826EFD18  (DWARF cpp:106)
//
//   VehicleState::UpdateParams();
//   if (mbIsAttached && meUpdateState == E_UPDATE_ATTACHED
//       && !module->GetBrnInputStructure()->GetVehicleInterface()
//              ->IsRaceCarActive(mAttachInfo.muVehicleIndex))
//       Detach();                                                     ; vtable +0x18
// ---------------------------------------------------------------------------
void AIVehicleState::UpdateParams( f32 af32DeltaTime )
{
    VehicleState::UpdateParams( af32DeltaTime );

    if ( IsAttached() && mauUpdateState[0] == CgsSound::Logic::State::E_UPDATE_ATTACHED )
    {
        BrnSound::Module::SoundLogicModule* lpModule =
            static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicles =
            lpModule->GetBrnInputStructure()->GetVehicleInterface();
        if ( !lpVehicles->IsRaceCarActive( static_cast<EActiveRaceCarIndex>( mAttachInfo.muVehicleIndex ) ) )
        {
            // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG
            if ( AISoundDiagLive() )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-sound-detach] car=" << static_cast<s32>( mAttachInfo.muVehicleIndex )
                    << " no longer active\n";
            }
            Detach();
        }
    }
}

// ---------------------------------------------------------------------------
// AIVehicleState::Detach()  @ 0x826CACB8  (DWARF cpp:137)
//
//   for (i = 0; i < mpStateManager->miNumStates; ++i)
//     if (i != miInstNum) {
//       other = the manager's state with miInstNum == i;              ; +4 / +24 walk
//       if (other->meUpdateState == E_UPDATE_DETATCHING) return 0;   ; one detach at a time
//     }
//   if (meUpdateState != E_UPDATE_ATTACHED) return 0;
//   mpvAttachment = 0; mbIsAttached = 0; meUpdateState 5 -> 6 (previous kept);  ; State::Detach
//   bIsRaceCarActive = {0,0}; RaceCarState::Clear(&mVehiclePhysicsData); mAttachInfo = 0;
//   mcaEngineComponentName[0][0] = mcaEngineComponentName[1][0] = 0; mfMaxRpm = 0;
//   mEngineComponentKey[0] = mEngineComponentKey[1] = 0;                        ; VehicleState::Clear
//   return 1;
// ---------------------------------------------------------------------------
bool AIVehicleState::Detach()
{
    CgsSound::Logic::StateManager* lpManager = GetStateManager();
    const s32 liCount = lpManager->GetStateObjCount();
    for ( s32 liOther = 0; liOther < liCount; ++liOther )
    {
        if ( liOther == GetInstanceID() )
            continue;
        CgsSound::Logic::State* lpOther = lpManager->GetHeadState();
        while ( lpOther && lpOther->GetInstanceID() != liOther )
            lpOther = lpOther->GetNextState();
        if ( lpOther && lpOther->mauUpdateState[0] == CgsSound::Logic::State::E_UPDATE_DETATCHING )
            return false;
    }

    return VehicleState::Detach();
}

} // namespace Vehicles
} // namespace BrnSound
