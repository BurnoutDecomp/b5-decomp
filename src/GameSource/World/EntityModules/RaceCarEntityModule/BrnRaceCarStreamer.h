#pragma once

// ============================================================================
// BrnWorld::RaceCarStreamer
//   GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarStreamer.h
//   (DWARF home; matching .cpp beside it is this TU's real home).
//
// The per-race-car asset director. It owns the five concrete component streamers
// (graphics / attributes / physics / wheel-graphics / audio) BY VALUE, one
// shared maxLoadFlags[] bitfield per active race car, the loaded resource-pointer
// tables, and the "desired car" request bookkeeping. The component streamers call
// back into the On*Loaded / On*Unloading hooks here, which fold the per-resource
// load bit into maxLoadFlags[] and cache the loaded ResourcePtr.
//
// Declaration shape (members, the ELoadFlags enum + its values, method set,
// const/by-value vs by-ref returns) is from the DecFIGS DWARF for
// BrnRaceCarStreamer.h, gated on the X360 ledger. Behaviour + the bit/flag
// CONSTANTS are the X360 ARTIST asm (BURNOUT_X360_ARTIST.XEX); the Feb-2007
// partial source (BrnRaceCarStreamer.h) is the style/idiom reference. NOTE: the
// Feb-2007 layout is an OLDER revision (no audio streamer, different ELoadFlags
// values, a RefCountedId remap table); the ARTIST/DWARF layout reproduced here is
// authoritative and supersedes it.
//
// This TU bodies the 21 functions the dossier lists (ctor, the flag/active/loaded
// queries, GetCarModelId, the two resource getters, the ten On*Loaded/Unloading
// hooks, the two OnResource* helpers, SetAudioLoadDataStatus, HACKGetValidModelIds).
// The remaining DWARF methods (Construct/Prepare/Destruct/AddVehicleData/Update/...)
// are DECLARED here for home completeness and bodied by their own TUs -- GROW this
// header, do NOT fork it.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                                  // CgsID (typedef u64)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"           // CgsResource::ResourcePtr<T>
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarComponentStreamers.h" // the 5 component streamers

namespace BrnResource
{
    struct VehicleList;   // SharedClasses/DataLists/VehicleList.h
    class  WheelList;     // SharedClasses/DataLists/WheelList.h
}

// FLAG: the streamed resource payload types. ResourcePtr<T> only needs T as an
// incomplete type for member declaration + the by-name accesses this TU performs
// (it never dereferences them), so these are forward-declared. Their real homes
// (BrnVehicleGraphicsSpec.h / BrnWheelGraphicsSpec.h, and the deformation spec
// header) are not yet reconstructed; replace the forward decls with the includes
// when those homes land.
namespace BrnVehicle { struct GraphicsSpec; }
namespace BrnWheel   { struct GraphicsSpec; }
namespace BrnPhysics { namespace Deformation { struct StreamedDeformationSpec; } }

namespace BrnWorld
{

// BrnRaceCarStreamer.h:278 (DWARF: namespace-scope `extern const float32_t`). The minimum
// time (seconds) that must elapse since the last desired-car load before UpdateDesiredCars
// will start a new un-prioritised desired load. Defined in BrnRaceCarStreamer.cpp.
extern const f32 KF_TIME_BETWEEN_DESIRED_LOADS;

// Forward decls for the PreScene IO buffers (Update only; pointer params).
namespace RaceCarEntityModuleIO
{
    struct InputBuffer_PreScene;
    struct OutputBuffer_PreScene;
    struct OutputBuffer_Prepare;
    struct ResourceRequestInterface;
}

class RaceCarStreamer
{
public:
    // The streamed resource pointer aliases (Feb-2007 typedefs; DWARF uses the
    // expanded ResourcePtr<...> forms, identical type).
    typedef CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>                       GraphicsResourcePtr;
    typedef CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec> PhysicsResourcePtr;
    typedef CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>                         WheelGraphicsResourcePtr;

    // BrnRaceCarStreamer.h:263 (DWARF). Per-car resource load-state bitfield values.
    // Values are X360-authoritative (each is the immediate the ARTIST On*Loaded asm
    // stores: gfx=4, physics=8, attrs=16, wheelgfx=32, audio=64).
    enum ELoadFlags
    {
        E_LOADFLAG_ACTIVE           = 1,
        E_LOADFLAG_UNUSED           = 2,
        E_LOADFLAG_LOADEDGFX        = 4,
        E_LOADFLAG_LOADEDPHYSICS    = 8,
        E_LOADFLAG_LOADEDATTRS      = 16,
        E_LOADFLAG_LOADEDWHEELGFX   = 32,
        E_LOADFLAG_LOADEDAUDIO      = 64,
        E_LOADFLAG_ALLRESOURCES     = 124,  // gfx|physics|attrs|wheelgfx|audio
        E_LOADFLAG_ALL_EXCEPT_AUDIO = 60,   // gfx|physics|attrs|wheelgfx
    };

    static const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

    // @ 0x827E4BC8. Default ctor: install the contained streamers + zero the resource
    // tables and scalar bookkeeping (bodied by THIS TU).
    RaceCarStreamer();

    // ---- declared for home completeness; bodied by their own TUs ----
    void Construct();
    void Prepare( const BrnResource::VehicleList* lpVehicleList, const BrnResource::WheelList* lpWheelList );
    void Destruct();

    void AddVehicleData( s32 liActiveRaceCar, CgsID lModelId, CgsID lWheelId, bool lbActive );
    void PrefetchVehicleData( s32 liActiveRaceCar, CgsID lModelId, CgsID lWheelId );
    void RemoveVehicleData( s32 liActiveRaceCar );
    void SetRequiredVehicleData( s32 liActiveRaceCar, CgsID lModelId, CgsID lWheelId, bool lbActive );
    void SetDesiredVehicleData( s32 liActiveRaceCar, CgsID lModelId, CgsID lWheelId, s32 liPriority );

    void Update( const RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                 RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput, f32 lfTimeStep );

    // @ 0x82302038. SIGNATURE RECOVERED FROM THE ASM: the console's caller
    // (RaceCarEntityModule::SendStreamerEvents @0x82304F70) calls the output buffer's
    // GetResourceRequestInterface() FIRST and passes the INTERFACE, not the buffer --
    // `v4 = sub_822B6338(lpOutput); AppendGameDataRequests(this + 69888, v4)`. The earlier
    // `OutputBuffer_Prepare*` spelling would have taken the write lock in the wrong place.
    void AppendGameDataRequests(
            RaceCarEntityModuleIO::ResourceRequestInterface* lpInterface ) const;

    // ---- bodied by THIS TU ----
    bool  IsRaceCarLoaded( s32 liActiveRaceCar ) const;
    bool  IsRaceCarActive( s32 liActiveRaceCar ) const;
    bool  IsDesiredRaceCarLoadedForCarSelect( s32 liActiveRaceCar ) const;
    CgsID GetCarModelId( s32 liActiveRaceCar ) const;

    // [FLAG PC bring-up] NOT X360 functions. GetGraphicsResource / GetWheelGraphicsResource
    // below assert IsRaceCarLoaded(), i.e. ALL FIVE resource bits. As of 2026-08-01 a car
    // gets three of them (LOADEDGFX + LOADEDPHYSICS + LOADEDATTRS); LOADEDWHEELGFX waits on
    // the deferred `LoadWheel` GameData handler (id 36) and LOADEDAUDIO on the audio
    // streamer, so the all-resources predicate is still constant-false and the console's
    // accessors would fire a dev assert on every rendered frame. This trio lets the render
    // leg ask the narrower question ("is the BODY loaded?") and take the resource without
    // it. DELETE all three the moment the wheel + audio legs resolve.
    bool IsGraphicsLoadedBringUp( s32 liActiveRaceCar ) const
    {
        return ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_LOADEDGFX ) != 0;
    }
    const GraphicsResourcePtr& GetGraphicsResourceBringUp( s32 liActiveRaceCar ) const
    {
        return maGraphicsResources[liActiveRaceCar];
    }
    const WheelGraphicsResourcePtr& GetWheelGraphicsResourceBringUp( s32 liActiveRaceCar ) const
    {
        return maWheelGraphicsResources[liActiveRaceCar];
    }

    const GraphicsResourcePtr&      GetGraphicsResource( s32 liActiveRaceCar ) const;
    const PhysicsResourcePtr&       GetPhysicsResource( s32 liActiveRaceCar ) const;
    const WheelGraphicsResourcePtr& GetWheelGraphicsResource( s32 liActiveRaceCar ) const;

    void OnGraphicsLoaded( s32 liActiveRaceCar, GraphicsResourcePtr lResource );
    void OnPhysicsLoaded( s32 liActiveRaceCar, PhysicsResourcePtr lResource );
    void OnAttributesLoaded( s32 liActiveRaceCar );
    void OnWheelGraphicsLoaded( s32 liActiveRaceCar, WheelGraphicsResourcePtr lResource );
    void OnAudioLoaded( s32 liActiveRaceCar );

    void OnGraphicsUnloading( s32 liActiveRaceCar );
    void OnPhysicsUnloading( s32 liActiveRaceCar );
    void OnAttributesUnloading( s32 liActiveRaceCar );
    void OnWheelGraphicsUnloading( s32 liActiveRaceCar );
    void OnAudioUnloading( s32 liActiveRaceCar );

    bool                  GetAudioLoadDataStatus( s32 liActiveRaceCar );
    RaceCarAudioStreamer* GetAudioCarStreamer();
    void                  SetAudioLoadDataStatus( s32 liActiveRaceCar, bool lbStatus );

    const BrnResource::VehicleList* GetVehicleList() const;
    bool HACK_IsWaitingForAudioAfterCarSelect() const;
    void HACK_SetWaitingForAudioAfterCarSelect( bool lbWaiting );

    // @ 0x822A1950. Re-prefix the two ids: "VEH_" + decode(lrModelId) -> compress,
    // "WHE_" + decode(lrWheelId) -> compress, writing the results back through the refs.
    void HACKGetValidModelIds( CgsID& lrModelId, CgsID& lrWheelId );

private:
    bool    IsFlagSet( s32 liActiveRaceCar, ELoadFlags leFlag ) const;
    u8      GetFlags( s32 liActiveRaceCar ) const;
    void    OnResourceLoaded( s32 liActiveRaceCar, u8 lxResourceFlag );
    void    OnResourceUnloading( s32 liActiveRaceCar, u8 lxResourceFlag );
    void    UpdateDesiredCars();

private:
    const BrnResource::VehicleList* mpVehicleList;                          // :280
    u8                              maxLoadFlags[KI_MAX_ACTIVE_RACE_CARS];  // :282

    RaceCarGraphicsStreamer      mGraphicsStreamer;        // :284
    RaceCarAttributeStreamer     mAttributeStreamer;       // :285
    RaceCarPhysicsStreamer       mPhysicsStreamer;         // :286
    RaceCarWheelGraphicsStreamer mWheelGraphicsStreamer;   // :287
    RaceCarAudioStreamer         mAudioStreamer;           // :288

    GraphicsResourcePtr      maGraphicsResources[KI_MAX_ACTIVE_RACE_CARS];      // :291
    PhysicsResourcePtr       maPhysicsResources[KI_MAX_ACTIVE_RACE_CARS];       // :292
    bool                     mabAttribsLoaded[KI_MAX_ACTIVE_RACE_CARS];         // :293
    WheelGraphicsResourcePtr maWheelGraphicsResources[KI_MAX_ACTIVE_RACE_CARS]; // :294
    bool                     mabAudioLoaded[KI_MAX_ACTIVE_RACE_CARS];           // :295

    CgsID maDesiredCarIds[KI_MAX_ACTIVE_RACE_CARS];     // :297
    CgsID maDesiredWheelIds[KI_MAX_ACTIVE_RACE_CARS];   // :298
    s32   maDesiredPriorities[KI_MAX_ACTIVE_RACE_CARS]; // :299

    f32  mfTimeSinceLastLoad;                       // :301
    bool mbHACK_WaitingForAudioAfterCarSelect;      // :303
};

} // namespace BrnWorld
