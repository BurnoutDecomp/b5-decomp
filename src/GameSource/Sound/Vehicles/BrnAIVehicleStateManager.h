#ifndef BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_MANAGER_H
#define BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                 // EActiveRaceCarIndex
#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"           // BrnSound::Vehicles::VehicleStateManager (the DWARF base)
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"              // CgsSound::Logic::Content (the spec tables)

// =============================================================================
// BrnSound::Vehicles::AIVehicleStateManager
//   GameSource/Sound/Vehicles/BrnAIVehicleStateManager.{h,cpp}
//   (canonical home -- the FireAssert source-path strings in the bodies cite
//    ".../Sound/Vehicles/BrnAIVehicleStateManager.cpp")
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// AIVehicleStateManager is the sound-logic state manager that owns the AI-car
// (rival / other race car) engine audio: it preloads the five authored AI engine
// bundles and their loop-model / Ginsu content specs, keeps three AIVehicleStates
// and binds them each frame to the active race cars nearest the listener.
//
// DWARF (BrnAIVehicleStateManager.h:40):
//   struct AIVehicleStateManager : public BrnSound::Vehicles::VehicleStateManager
// -- the console's PrepareAIEngineLoading @0x826E2708 calls
// VehicleStateManager::AddRegistry(this, name, 1) on `this`, and the manager
// vtable (off_820B8C88) slot 10 is VehicleStateManager::IsStateAlias @0x82683D50.
// (Until 2026-09-15 this leaf derived BrnStateManager directly, so it had no
// IsStateAlias -- and could therefore never create a state, effect or control.)
//
// RTTI: ObjectID 2, descriptor 0x82F2E88C, base StateManager (0x82F2FAA0),
// registered from the CRT init bank at 0x82C61D98.
//
// X360 layout (32-bit; BY NAME on the host): +0x24 mePrepareState, +0x2C
// mpLogicModule, +0x90 IResourceRequester vptr, +0x94 miCpuMonitor (BrnStateManager),
// +0x98 meAIEngineLoadingState, +0x9C maLoopContentSpecs[5][10] (12-byte Content
// stride, "12*(10*i+j+13)"), +0x2F4 maGinsuAccelContentSpecs[5] ("12*(i+63)"),
// +0x330 maGinsuDecelContentSpecs[5] ("12*(i+68)"); sizeof 0x370 (CreateObject
// @0x82702358 allocates 880).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

class AIVehicleStateManager : public BrnSound::Vehicles::VehicleStateManager
{
public:
    // DWARF BrnAIVehicleStateManager.h:27. The number of authored AI engine slots.
    static const s32 KI_NUMBER_OF_AUDIO_AI_ENGINES = 5;

    // DWARF BrnAIVehicleStateManager.h:47. PrepareStates(0x4218, 3, 0) @0x826EFC18.
    static const u16 KI_NUMBER_OF_AUDIO_AI_CAR_STATES = 3;

    // DWARF BrnDualGinsuEffect.h KI_MAX_LOOPS = 10 (the assert text in
    // GetLoopModelContent / PrepareAIEngineLoading names it verbatim).
    static const s32 KI_MAX_LOOPS = 10;

    // The effect mask PrepareStates receives @0x826EFC18: effects 3
    // (DualGinsuExhaustEffect, a state-1 alias), 4 (AISkidEffect), 9 (InAirEffect,
    // alias) and 14 (CarStereoEffect) -- 16920 == 0x4218.
    static const s32 KI_AI_STATE_EFFECT_MASK = 0x4218;

    // DWARF BrnAIVehicleStateManager.h:112 -- the engine-loading sub-state machine
    // PrepareAIEngineLoading @0x826E2708 walks and ResourcesAreReady @0x82684038
    // advances (1 -> 2, 3 -> 4).
    enum EAIEngineLoadingState
    {
        E_AI_ENGINE_LOADING_BEGIN                              = 0,
        E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB            = 1,
        E_AI_ENGINE_LOADING_BEGIN_LOADING_ENGINE_COMPONENTS    = 2,
        E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS  = 3,
        E_AI_ENGINE_LOADING_CREATING_CONTENT_SPECS             = 4,
        E_AI_ENGINE_LOADING_WAITING_FOR_CONTENT_SPECS          = 5,
        E_AI_ENGINE_LOADING_FINISHED                           = 6,
    };

    // The five authored AI engine names (X360 rodata table off_82F2CBDC: AIROD_EX,
    // AI_CIVIC_EX, AI_GT_ENG, AI_MUST_EX, AI_F1_EX) and the matching vehicleengine
    // attribute-collection ids (dword_820AA4B4: 563494, 613976, 565137, 576566,
    // 564456) -- the collection key is Attrib::StringToKey of the id's decimal
    // string (rw::core::stdc::ConvertI64ToA(id, buf, 10) on the console). Shared
    // with AIVehicleState::Attach @0x826CA998, which indexes the same two tables by
    // the car's AI engine assignment.
    static const char* GetAIEngineName( s32 liAIEngineIndex );
    static u64         GetAIEngineAttribKey( s32 liAIEngineIndex );

    // ctor @ 0x82700EB8 (export-set hole; CreateObject @0x82702358 `bl`s it after
    // an 880-byte MemBase::operator new tagged "AIVehicleStateManager").
    AIVehicleStateManager();
    virtual ~AIVehicleStateManager();                                     // @ 0x82700FE0

    // ---- RTTI hooks. STATIC GetStaticTypeInfo / CreateObject so &CreateObject is
    // storable in ClassTypeInfo<StateManager>::mpfnCreateObject. ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;                              // @ 0x82684028
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );     // @ 0x82702358

    // ---- boot + per-frame virtuals ----
    virtual bool  Prepare();                                              // @ 0x826EFC18 (vtable +0x0C)
    virtual void  UpdateParams( f32 af32DeltaTime );                      // @ 0x826CA578 (vtable +0x18)
    virtual CgsSound::Logic::State* GetFreeState( void* apvAttachment );  // @ 0x826B1B28 (vtable +0x14)

    // ---- IResourceRequester ----
    virtual void ResourcesAreReady();                                     // @ 0x82684038

    // ---- content-spec accessors (DualGinsuEffect::Attach queries these). ----
    const CgsSound::Logic::Content* GetLoopModelContent( s32 liAIEngineIndex, u32 lLoopIndex ); // @ 0x826987A8
    const CgsSound::Logic::Content* GetAccelGinsuContent( s32 liAIEngineIndex );               // DWARF h:168
    const CgsSound::Logic::Content* GetDecelGinsuContent( s32 liAIEngineIndex );               // @ 0x82698910

private:
    // @ 0x826B1C70 (DWARF cpp:614). Reconcile the module-wide desired/attached
    // vehicle tables for the AI entries: unload when the desired entry became the
    // player or changed, "load" (post AudioCarDataLoadedEvent) a desired AI entry.
    void UpdateVehicleLoading( EActiveRaceCarIndex leActiveRaceCarIndex );

    // @ 0x826E2708 (DWARF cpp:167). The seven-state AI engine bundle loader.
    bool PrepareAIEngineLoading();

    EAIEngineLoadingState    meAIEngineLoadingState;                                                  // +0x98
    CgsSound::Logic::Content maLoopContentSpecs[KI_NUMBER_OF_AUDIO_AI_ENGINES][KI_MAX_LOOPS];         // +0x9C
    CgsSound::Logic::Content maGinsuAccelContentSpecs[KI_NUMBER_OF_AUDIO_AI_ENGINES];                 // +0x2F4
    CgsSound::Logic::Content maGinsuDecelContentSpecs[KI_NUMBER_OF_AUDIO_AI_ENGINES];                 // +0x330
};

} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_MANAGER_H
