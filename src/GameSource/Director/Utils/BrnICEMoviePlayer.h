#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_ICE_MOVIE_PLAYER_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_ICE_MOVIE_PLAYER_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT (BehaviourHandle::Prepare)
#include "GameShared/GameClasses/Containers/CgsArray.h"               // Array<T,N> (mMoviePoolIndicies)
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"          // CgsContainers::ObjectPool<T,N,TIndex>
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"     // CgsResource::ID (IceMovie::mCgsID)
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex
#include "GameSource/Director/Utils/BrnVehicleRef.h"                  // BrnDirector::VehicleRef::EType
#include "GameSource/Director/Camera/Camera.h"                        // BrnDirector::Camera::Camera (mCamera, by value)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"           // BrnDirector::Camera::BehaviourManager / BehaviourHandle<> / BehaviourInterpolate / BehaviourHelperIndex (canonical home)

// ============================================================================
// GameSource/Director/Utils/BrnICEMoviePlayer.h
//
// The director-side ICE (In-game Camera Editor) movie-player family. A "movie" is a
// single recorded ICE camera take, identified either by an explicit resource id or by
// an (ICE-group, take-index) pair that resolves to a generated name. A playlist owns a
// fixed-capacity pool of movies; the player walks a playlist's movies in order, driving
// ICEWrapper::PlayMovie for each and blending between them with camera interpolators.
//
// THIS FILE IS THE HOME for IceMovie / ICEMoviePlaylist (+ its DebugMenuRemoveData) /
// SharedPlaylists / ICEMoviePlayer. Member NAMES/TYPES come from the recovered C++-shaped
// declarations for this subsystem; member ORDER/placement is verified against the
// reconstructed function bodies' member accesses. Members are accessed BY NAME -- the
// struct-relative offsets quoted in comments are provenance only, never used as casts.
//
// Bodies for Construct/Prepare/Play/Stop/Update/Loop/CutToInterpolateOut/GetCamera/
// InterpolateFrom (player), GetMovieCount/DebugMenuNewMovie (playlist), GetCgsID
// (IceMovie) and SharedPlaylists::Construct land in BrnICEMoviePlayer.cpp. The rest of
// the recovered method set is DECLARATION-ONLY here (each lands a body with its own
// ledger TU; the per-TU `cl /c` gate does not link, so declarations suffice).
//
// The IceMovie and ICEMoviePlaylist::DebugMenuRemoveData element structs are defined in
// full so the separate AbstractPool/ObjectPool instantiation TUs can instantiate over
// them; those pool instantiations are NOT defined here.
//
// The camera-behaviour layer the player drives -- BrnDirector::Camera::BehaviourManager,
//   ::BehaviourHandle<>, ::BehaviourInterpolate (+ ::Parameters), ::BehaviourHelperIndex --
//   now has its canonical full-layout home at GameSource/Director/Camera/BrnBehaviourManager.h
//   (included above). The minimal slice that previously lived in this file was reconciled
//   into that home, so there is exactly ONE definition; these types are used here by name only.
// FLAG: BrnDirector::DebugComponent (the playlist's mpDebugComponent / SetDebugComponent)
//   is an unreconstructed dev-tools type -- forward-declared only (pointer member).
// ============================================================================

namespace BrnDirector
{

// Forward declaration -- only ever held by pointer here (dev-tools menu owner).
// FLAG: no reconstructed home yet for BrnDirector::DebugComponent.
class DebugComponent;

// Forward declaration -- the player holds a pointer to the director-side ICE owner,
// whose home is GameSource/Director/BrnDirectorICEWrapper.h. Forward-declared here to
// avoid pulling the whole ICE manager cascade into this header; the .cpp includes the
// real header for the PlayMovie/IsPlayingMovie calls.
class ICEWrapper;

// ----------------------------------------------------------------------------
// The Director camera-behaviour layer the player drives (BehaviourManager,
// BehaviourHandle<>, BehaviourInterpolate (+ Parameters), BehaviourHelperIndex) now has
// its canonical home at GameSource/Director/Camera/BrnBehaviourManager.h, included above.
// Those types are used here only by name (the player's interpolator members + the .cpp
// driving them). The previous minimal slice that lived in this file has been reconciled
// into that home so there is exactly ONE definition of BrnDirector::Camera::BehaviourManager.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// IceMovie -- one entry in a playlist: a single ICE camera take, referenced either by
// an explicit resource id (E_REF_TYPE_CGSID) or by an (ICE-group, take-index) pair that
// resolves to a generated take name (E_REF_TYPE_GROUP_TAKE). Layout verified against
// IceMovie::GetCgsID / ICEMoviePlayer::Play / ::Update member accesses.
// ----------------------------------------------------------------------------
struct IceMovie
{
    // Which ICE group a group/take reference belongs to (selects the generated name
    // prefix in MakeCGSId).
    enum EIceGroup
    {
        E_ICE_GROUP_INVALID        = -1,
        E_ICE_GROUP_EVENTS_START   = 0,
        E_ICE_GROUP_EVENTS_END     = 1,
        E_ICE_GROUP_WORLD_LANDMARK = 2,
        E_ICE_GROUP_WORLD_SIGNATURE= 3,
        E_ICE_GROUP_VEHICLE_CAR    = 4,
        E_ICE_GROUP_VEHICLE_RIV    = 5,
        E_ICE_GROUP_GENERIC_ALL    = 6,
        E_ICE_GROUP_COUNT          = 7
    };

    // How this movie names its take.
    enum ERefType
    {
        E_REF_TYPE_INVALID    = -1,
        E_REF_TYPE_CGSID      = 0,   // mCgsID holds the take id directly
        E_REF_TYPE_GROUP_TAKE = 1,   // (meGroup, muTakeIndex) generate the take name
        E_REF_TYPE_COUNT      = 2
    };

    void              Construct();
    // X360 visitor: `void Serialise<S>(S&)` -- writes/reads this movie entry through the
    // camera-tunings serialiser S; TextFileWriteSerialiser drives it (recursed into from
    // ICEMoviePlaylist::Serialise). The per-instance body is a separate TU.
    template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);
    const CgsResource::ID  GetCgsID() const;        // body in BrnICEMoviePlayer.cpp
    f32               GetStartPosition() const;
    VehicleRef::EType GetVehicleRefType() const;
    u32               GetVehicleIndex() const;
    void              SetMovie(CgsResource::ID lCgsID);
    void              SetMovie(const char* lpcName);
    void              SetMovie(EIceGroup leGroup, u32 luTakeIndex);
    void              SetStartPosition(f32 lfStartPosition01);
    void              SetVehicle(VehicleRef::EType leType, u32 luIndex);
    bool              GetShouldFlash() const;
    void              SetShouldFlash(bool lbShouldFlash);

    ERefType          meRefType;          // +0x00  CGSID vs GROUP_TAKE vs INVALID
    // +0x04 implicit padding to 8-byte-align the 64-bit id below.
    CgsResource::ID   mCgsID;             // +0x08  explicit take id (E_REF_TYPE_CGSID)
    EIceGroup         meGroup;            // +0x10  group for a generated name
    u32               muTakeIndex;        // +0x14  take index within the group
    f32               mfStartPosition01;  // +0x18  normalised start position [0..1]
    VehicleRef::EType meVehicleType;      // +0x1C  vehicle the take is anchored to
    u32               muVehicleIndex;     // +0x20  index within that vehicle class
    bool              mbPlayFlash;        // +0x24  fire the "2dFlash" hook on start
};

// ----------------------------------------------------------------------------
// ICEMoviePlaylist -- a fixed-capacity (20) ordered list of movies. Storage is a pool
// of IceMovie plus an order array of pool indices; the live count is the order array's
// element count. Layout verified against GetMovieCount (count word at the order array's
// +0x50, i.e. playlist +0x3D0) and the Play/Update pool/array accesses.
// ----------------------------------------------------------------------------
struct ICEMoviePlaylist
{
    static const s32 KI_CAPACITY = 20;

    // The per-movie remove command the dev menu builds (a back-pointer + the index to
    // remove). Defined in full so its pool instantiation TU can instantiate over it.
    struct DebugMenuRemoveData
    {
        bool operator==(const DebugMenuRemoveData& lrOther) const
        {
            return mpThisPlaylist == lrOther.mpThisPlaylist && miIndex == lrOther.miIndex;
        }

        ICEMoviePlaylist* mpThisPlaylist;   // +0x00  playlist the command acts on
        s32               miIndex;          // +0x04  movie index to remove
    };

    void                       Construct();
    // X360 visitor: `void Serialise<S>(S&)` -- writes/reads the whole playlist through the
    // camera-tunings serialiser S, recursing into each IceMovie::Serialise. TextFileWriteSerialiser
    // drives it (from SharedPlaylists::Serialise). The per-instance body is a separate TU.
    template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);
    void                       InsertMovieBefore(s32 liDesiredZeroBasedIndex, const IceMovie& lrMovie);
    void                       AddMovie(const IceMovie& lrMovie);
    void                       AddMovie(IceMovie::EIceGroup leGroup, u32 luTake, bool lbShouldFlash,
                                        VehicleRef::EType leVehicleType, u32 luVehicleIndex);
    s32                        GetMovieCount() const;     // body in BrnICEMoviePlayer.cpp
    s32                        GetCapacity() const;
    const IceMovie&            GetMovie(s32 liIndex) const;
    IceMovie&                  GetMovie(s32 liIndex);
    const char*                DebugGetMovieName(s32 liIndex) const;
    DebugMenuRemoveData        GetRemoveData(s32 liMovie);
    void                       DebugMenuRemoveMovie(void* lpRemoveData);
    void                       DebugMenuNewMovie(void* lpContext);   // body in BrnICEMoviePlayer.cpp
    const CgsContainers::ObjectPool<DebugMenuRemoveData, 20, s32>& GetDebugMenuRemoveDataPool() const;
    void                       SetDebugComponent(DebugComponent* lpDebugComponent);
    s32&                       GetDebugMenuNewMovieIndex();
    const char*                GetDebugName() const;
    void                       SetDebugName(const char* lpcDebugName);

    // +0x000  the live IceMovie storage (20 inline slots + free queue + occupancy).
    CgsContainers::ObjectPool<IceMovie, 20, s32> mMoviePool;
    // +0x380  the playback order: pool indices, count == number of movies in the list.
    Array<s32, 20>                               mMoviePoolIndicies;
    // +0x3D8  the dev-menu remove-command pool (one slot per movie).
    CgsContainers::ObjectPool<DebugMenuRemoveData, 20, s32> mDebugMenuRemoveData;
    s32                                          miDebugMenuNewMovieIndex;
    s32                                          miDebugSize;
    DebugComponent*                              mpDebugComponent;
    const char*                                  mpDebugName;
};

// ----------------------------------------------------------------------------
// SharedPlaylists -- the five director-owned playlists used across a race (the race
// intro, the post-race sequence, and three pause-camera playlists), each seeded with a
// fixed list of takes at Construct time.
// ----------------------------------------------------------------------------
struct SharedPlaylists
{
    static const u32 KU_NUM_PAUSE_PLAYLISTS = 3;

    void                    Construct();              // body in BrnICEMoviePlayer.cpp
    // X360 visitor: `void Serialise<S>(S&)` -- writes/reads the three pause playlists (each a nested
    // ICEMoviePlaylist::Serialise recursion) plus the current-pause-playlist index, through the
    // camera-tunings serialiser S. Body + explicit instantiations in BrnICEMoviePlayer.cpp.
    template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);
    const ICEMoviePlaylist& GetRaceIntroPlaylist() const;
    const ICEMoviePlaylist& GetPostRacePlaylist() const;
    const ICEMoviePlaylist& GetPausePlaylist() const;

    ICEMoviePlaylist mRaceIntroPlaylist;                          // +0x0000
    ICEMoviePlaylist mPostRacePlaylist;                           // +0x04E8
    ICEMoviePlaylist maPausePlaylists[KU_NUM_PAUSE_PLAYLISTS];    // +0x09D0
    u32              muCurrentPausePlaylist;                      // +0x1888
};

// ----------------------------------------------------------------------------
// ICEMoviePlayer -- plays a playlist of ICE movies through the director camera. Owns its
// own camera, a playlist, a pointer to the ICE wrapper it drives, two camera-behaviour
// interpolators (blend in / blend out) with their helper indices and parameters, the
// interpolation durations, and the playback/looping/interpolation state flags.
//
// LAYOUT NOTE: mCamera and mPlaylist are embedded BY VALUE; our rebuilt sizes differ
// from the source build, so (as with BrnDirectorICEWrapper.h) members are declared in
// their recorded ORDER and parity is BY NAME, not by reproducing absolute offsets. The
// struct-relative offsets quoted are provenance from the recorded member order.
// ----------------------------------------------------------------------------
struct ICEMoviePlayer
{
    void                Construct();
    bool                Prepare(ICEWrapper* lpICEWrapper);
    bool                HasReachedEnd() const;
    bool                IsLooping() const;
    bool                IsPlaying() const;
    void                Play();
    void                Loop();
    void                CutToInterpolateOut();
    void                Stop();
    void                Update(Camera::BehaviourManager& lrBehaviourManager);
    const Camera::Camera& GetCamera() const;
    void                InterpolateFrom(Camera::BehaviourManager& lrBehaviourManager,
                                        Camera::BehaviourHelperIndex lFromHelper,
                                        f32 lfDuration,
                                        const Camera::BehaviourInterpolate::Parameters* lpParams,
                                        bool lbUpdatesDuringPause);
    void                WhenFinishedInterpolateTo(Camera::BehaviourHelperIndex lToHelper,
                                                  f32 lfDuration, f32 lfMaxOverlapTime,
                                                  const Camera::BehaviourInterpolate::Parameters* lpParams,
                                                  bool lbUpdatesDuringPause);
    ICEMoviePlaylist&   GetPlaylist();
    void                SetPlaylist(const ICEMoviePlaylist& lrPlaylist);
    void                SetTargetRaceCar(EActiveRaceCarIndex leRaceCar, VehicleRef::EType leRefType);
    EActiveRaceCarIndex GetTargetRaceCar();

private:
    // Kick off the movie at miCurrentMovie on the ICE wrapper at its stored start
    // position and clear the first-frame flag (the shared "start this movie" step that
    // Play and Update's advance/loop branches both perform).
    void StartCurrentMovie();

    // Fire the "2dFlash" start hook on this player's camera for the current movie.
    // FLAG: reaches the camera-effects start-hook fields, which live inside the minimal
    // Camera slice's opaque cascade -- DECLARATION-ONLY here; its body lands when the
    // Camera effects layer is modelled. Declared so Update reads idiomatically by name
    // rather than poking camera internals through an offset.
    void ApplyFlashHookToCamera();

public:
    // +0x0000  the camera this player drives (the active take's framing).
    Camera::Camera   mCamera;
    // +0x0160  the playlist of movies being played.
    ICEMoviePlaylist mPlaylist;
    // +0x0648  the director-side ICE owner driven for playback (NULL until Prepare).
    ICEWrapper*      mpICEWrapper;

    // +0x064C / +0x0660  the in/out camera interpolators (blend into a take / blend out).
    Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInInterpolator;
    Camera::BehaviourHandle<Camera::BehaviourInterpolate> mOutInterpolator;

    // +0x0674 / +0x0678  the helper-table slots the interpolators read from / write to.
    Camera::BehaviourHelperIndex mFromBehaviourHelperIndex;
    Camera::BehaviourHelperIndex mToBehaviourHelperIndex;

    // +0x067C / +0x0680  per-take interpolation parameters (curve/easing) for in / out.
    const Camera::BehaviourInterpolate::Parameters* mpInterpolateInParams;
    const Camera::BehaviourInterpolate::Parameters* mpInterpolateOutParams;

    // +0x0684 / +0x0688 / +0x068C  interpolation timings (seconds).
    f32 mfInterpolateInDuration;
    f32 mfInterpolateOutDuration;
    f32 mfMaxInterpolateOutOverlapTime;

    // +0x0690  index of the movie currently playing within the playlist.
    s32 miCurrentMovie;

    // +0x0694 / +0x0698  which active race car / vehicle ref the takes anchor to.
    EActiveRaceCarIndex meTargetRaceCar;
    VehicleRef::EType   meTargetVehicleRefType;

    // +0x069C .. +0x06A3  playback / interpolation state flags.
    bool mbInterpolateIn;                    // +0x069C  blending into a take this frame
    bool mbInterpolateOutNow;                // +0x069D  blend-out requested now
    bool mbHasReachedEnd;                    // +0x069E  playlist finished (non-looping)
    bool mbIsLooping;                        // +0x069F  loop the playlist
    bool mbIsPlaying;                        // +0x06A0  playback is active
    bool mbShouldInterpolateOut;             // +0x06A1  interpolate out at the end
    bool mbInterpolateOutUpdatesDuringPause; // +0x06A2  out-interp ticks while paused
    bool mbFirstFrameOfPlaying;              // +0x06A3  next Update is the first play frame
};

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_ICE_MOVIE_PLAYER_H
