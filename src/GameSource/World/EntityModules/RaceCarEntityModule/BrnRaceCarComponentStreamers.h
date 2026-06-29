#pragma once

// ============================================================================
// BrnWorld race-car component streamers
//   GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarComponentStreamers.h
//   (DWARF home).
//
// The five concrete per-component streamers owned BY VALUE by BrnWorld::RaceCarStreamer
// (graphics / attributes / physics / wheel-graphics / audio). Each derives the shared
// BrnWorld::RaceCarBaseComponentStreamer (which itself derives BaseStreamer<8>) and adds
// a back-pointer to its owning RaceCarStreamer; the audio leaf additionally owns the
// per-car streaming-sound state table and the audio-loaded-data input queue.
//
// Declaration shape (members, the two OnAsset* pure-virtual overrides, the streaming-sound
// state enum + record) is taken from the DecFIGS DWARF for BrnRaceCarComponentStreamers.h,
// gated on the X360 ledger. This header is reconstructed primarily so RaceCarStreamer can
// hold the five streamers by value with a coherent layout and call the base
// GetDesiredAsset()/GetLoadedAsset() accessors through them; the leaves' own method BODIES
// live in the dedicated BrnRaceCarComponentStreamers.cpp TU. GROW this header (add the
// Construct/Destruct/AddEntry/... bodies' decls as their TU lands) -- do NOT fork it.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (typedef u64)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"   // CgsModule::EventQueue<T,N>
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarBaseComponentStreamer.h"
// AudioCarDataLoadedEvent (the queue element type for mAudioCarLoadedDataQueue).
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

namespace BrnWorld
{

class RaceCarStreamer;   // owner; back-pointer only

// The PreScene IO buffers the audio streamer's Update() pumps (pointer params only --
// forward-declared here, fully homed in BrnRaceCarEntityModuleIO.h, #included by the .cpp).
namespace RaceCarEntityModuleIO
{
    struct InputBuffer_PreScene;
    struct OutputBuffer_PreScene;
}

// BrnRaceCarComponentStreamers.h:47 (DWARF). Streams the per-car vehicle graphics spec.
class RaceCarGraphicsStreamer : public RaceCarBaseComponentStreamer
{
public:
    void Construct( RaceCarStreamer* lpStreamer );
    void Destruct();

protected:
    virtual void OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent );
    virtual void OnAssetUnloading( s32 liActiveRaceCar );

private:
    RaceCarStreamer* mpStreamer;   // BrnRaceCarComponentStreamers.h:74
};

// BrnRaceCarComponentStreamers.h:87 (DWARF). Streams the per-car physics/deformation spec.
class RaceCarPhysicsStreamer : public RaceCarBaseComponentStreamer
{
public:
    void Construct( RaceCarStreamer* lpStreamer );
    void Destruct();

protected:
    virtual void OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent );
    virtual void OnAssetUnloading( s32 liActiveRaceCar );

private:
    RaceCarStreamer* mpStreamer;   // BrnRaceCarComponentStreamers.h:116
};

// BrnRaceCarComponentStreamers.h:129 (DWARF). Streams the per-car attribute set.
class RaceCarAttributeStreamer : public RaceCarBaseComponentStreamer
{
public:
    void Construct( RaceCarStreamer* lpStreamer );
    void Destruct();

protected:
    virtual void OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent );
    virtual void OnAssetUnloading( s32 liActiveRaceCar );

private:
    RaceCarStreamer* mpStreamer;   // BrnRaceCarComponentStreamers.h:157
};

// BrnRaceCarComponentStreamers.h:170 (DWARF). Streams the per-car wheel graphics spec.
class RaceCarWheelGraphicsStreamer : public RaceCarBaseComponentStreamer
{
public:
    void Construct( RaceCarStreamer* lpStreamer );
    void Destruct();

protected:
    virtual void OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent );
    virtual void OnAssetUnloading( s32 liActiveRaceCar );

private:
    RaceCarStreamer* mpStreamer;   // BrnRaceCarComponentStreamers.h:198
};

// BrnRaceCarComponentStreamers.h:213 (DWARF). Streams the per-car streaming-sound bundles
// and drives the attach/detach state machine.
class RaceCarAudioStreamer : public RaceCarBaseComponentStreamer
{
public:
    // BrnRaceCarComponentStreamers.h:268 (DWARF). One per-car streaming-sound slot.
    struct RaceCarStreamingSound
    {
        // BrnRaceCarComponentStreamers.h:271 (DWARF).
        enum RaceCarStreamingSoundState
        {
            E_RACECARSTREAMINGSOUND_IDLE              = 0,
            E_RACECARSTREAMINGSOUND_LOADINGBUNDLES    = 1,
            E_RACECARSTREAMINGSOUND_LOADEDBUNDLES     = 2,
            E_RACECARSTREAMINGSOUND_ATTACH            = 3,
            E_RACECARSTREAMINGSOUND_ATTACHING         = 4,
            E_RACECARSTREAMINGSOUND_ATTACHED          = 5,
            E_RACECARSTREAMINGSOUND_LOADEDANDATTACHED = 6,
            E_RACECARSTREAMINGSOUND_DETACH            = 7,
            E_RACECARSTREAMINGSOUND_DETACHING         = 8,
            E_RACECARSTREAMINGSOUND_DETACHED          = 9,
            E_RACECARSTREAMINGSOUND_UNLOADBUNDLES     = 10,
            E_RACECARSTREAMINGSOUND_UNLOADINGBUNDLES  = 11,
            E_RACECARSTREAMINGSOUND_COUNT             = 12,
        };

        CgsID                      mDesiredId;        // :289
        CgsID                      mLoadedBundleId;   // :290
        RaceCarStreamingSoundState meState;           // :292
        s8                         miUserID;          // :294
        bool                       mbLoadedIsPlayer;  // :296
        bool                       mbDesiredIsPlayer; // :297
    };

    // The per-car streaming-sound (un)load events the streamer publishes to / consumes
    // from the PreScene IO buffers. Identical to RaceCarEntityModuleIO::AudioCarLoadedDataQueue
    // (defined the same way in BrnRaceCarEntityModuleIO.h); spelled out here so this header
    // needs only the lightweight AudioCarDataLoadedEvent home, not the full IO buffer header.
    typedef CgsModule::EventQueue<RaceCarEntityModuleIO::AudioCarDataLoadedEvent, 16> AudioCarLoadedDataQueue;

    // @ 0x822ECA68. Forward the base streamer Construct (sound pool, asset set
    // E_ASSETSET_SOUND), construct the audio-loaded-data queue, then initialise the eight
    // per-car streaming-sound slots (state IDLE, miUserID = slot index).
    void Construct( RaceCarStreamer* lpStreamer );
    void Destruct();

    // @ 0x822ECC00. Pump the streaming-sound state machine for all eight cars: consume the
    // input buffer's "data (un)loaded" replies to advance ATTACHING->ATTACHED /
    // DETACHING->DETACHED, drive each slot one step, then flush this frame's outgoing
    // (un)load requests onto the output buffer.
    void Update( const RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                 RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

    // @ 0x822A5730. Register the streaming-sound asset luAssetID for race-car luUserId.
    // Returns true. lbIsPlayer records whether this is the player's car.
    bool AddEntry( CgsID luAssetID, u64 luUserId, bool lbIsPlayer );

    // @ 0x822A58D0. True when car liActiveRaceCar's streaming sound has reached the
    // LOADEDANDATTACHED state. lAssetId / lbIsPlayer are validation-only (unused here).
    bool IsVehicleAssetLoaded( CgsID lAssetId, s32 liActiveRaceCar, bool lbIsPlayer );

protected:
    virtual void OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent );
    virtual void OnAssetUnloading( s32 liActiveRaceCar );

    // @ 0x822C08C8 / 0x822A55C0. The audio streamer fully overrides the base load/unload
    // completion hooks: it drives its OWN per-car streaming-sound bit set + slot states
    // rather than the base maDesiredAssets/maLoadedAssets bookkeeping.
    virtual void OnLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent, s32 liListIndex );
    virtual void OnUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent, s32 liListIndex );

private:
    // @ 0x822D5080 / 0x822D4E40. Emit a load / unload streaming-sound request for the given
    // slot onto mAudioCarLoadedDataQueue (consumed by the audio subsystem next frame).
    bool SendLoadRequest( RaceCarStreamingSound* lpEntry );
    bool SendUnLoadRequest( RaceCarStreamingSound* lpEntry );

private:
    RaceCarStreamingSound maEntries[RaceCarBaseComponentStreamer::KI_MAX_ACTIVE_RACE_CARS]; // :300

    // The per-car audio-loaded-data event queue (RaceCarEntityModuleIO). The X360 Construct
    // calls EventQueue<AudioCarDataLoadedEvent,16>::Construct on it (@0x822E3670); Update
    // appends it onto the output buffer's queue and clears it each frame.
    AudioCarLoadedDataQueue mAudioCarLoadedDataQueue; // :302

    RaceCarStreamer* mpStreamer;        // :303
};

} // namespace BrnWorld
