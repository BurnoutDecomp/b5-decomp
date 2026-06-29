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
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarBaseComponentStreamer.h"

namespace BrnWorld
{

class RaceCarStreamer;   // owner; back-pointer only

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

    void Construct( RaceCarStreamer* lpStreamer );
    void Destruct();

protected:
    virtual void OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent );
    virtual void OnAssetUnloading( s32 liActiveRaceCar );

private:
    RaceCarStreamingSound maEntries[RaceCarBaseComponentStreamer::KI_MAX_ACTIVE_RACE_CARS]; // :300

    // FLAG: mAudioCarLoadedDataQueue is InputBuffer_PreScene::AudioCarLoadedDataQueue
    // (RaceCarEntityModuleIO), a VariableEventQueue-backed IO type whose full layout is
    // owned by the RaceCarEntityModuleIO subsystem. RaceCarStreamer never touches it, so it
    // is modelled here as an opaque byte placeholder purely for member-presence/sequence.
    // Its exact byte size is NOT an X360 fact -- GROW into the real type when the audio
    // streamer TU + its IO queue land.
    u8 mPadAudioCarLoadedDataQueue[16]; // :302 (placeholder for AudioCarLoadedDataQueue)

    RaceCarStreamer* mpStreamer;        // :303
};

} // namespace BrnWorld
