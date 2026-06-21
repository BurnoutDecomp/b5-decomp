#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_ICE_MOVIE_PLAYER_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_ICE_MOVIE_PLAYER_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"               // Array<T,N> (mMoviePoolIndicies)
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"          // CgsContainers::ObjectPool<T,N,TIndex>
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"     // CgsResource::ID (IceMovie::mCgsID)
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex
#include "GameSource/Director/Utils/BrnVehicleRef.h"                  // BrnDirector::VehicleRef::EType
#include "GameSource/Director/Camera/Camera.h"                        // BrnDirector::Camera::Camera (mCamera, by value)

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
// FLAG (minimal-sliced unhomed deps): the camera-behaviour layer that the player drives
//   -- BrnDirector::Camera::BehaviourManager, ::BehaviourHandle<>, ::BehaviourInterpolate
//   (+ ::Parameters), ::BehaviourHelperIndex -- is NOT yet reconstructed. It is modelled
//   below with the minimum named members/methods the player bodies touch, accessed by
//   name (the camera-A/B setup is expressed as BehaviourInterpolate methods that take a
//   helper or camera directly, rather than exposing a separate CameraReference type).
//   Replace these minimal slices with their real homes when those Camera TUs are
//   reconstructed; the player's member NAMES are stable.
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
// FLAG: minimal slice of the Director camera-behaviour layer (no reconstructed home
// yet). Declared here so the player can name its interpolator members and the .cpp can
// drive them idiomatically. Methods are DECLARATION-ONLY (bodies land with the real
// Camera TUs); pointer/handle members are sized only as needed to be embeddable.
// ----------------------------------------------------------------------------
namespace Camera
{
    class BehaviourManager;

    // Identifies a slot in a BehaviourManager's helper-index table (the "from"/"to"
    // helper the player records for an interpolation). It is an assignable value type;
    // modelled here as a single index word.
    class BehaviourHelperIndex
    {
    public:
        BehaviourHelperIndex() : miIndex(-1) {}
        s32 miIndex;
    };

    // The per-behaviour interpolator that blends one camera source into another. The
    // player allocates two of these via the manager and drives their setup. The methods
    // are the named operations the player's bodies invoke -- DECLARATION-ONLY (the real
    // bodies land with the BehaviourInterpolate TU).
    // FLAG: minimal slice -- no reconstructed home yet.
    class BehaviourInterpolate
    {
    public:
        // Per-take interpolation curve/easing parameters. Opaque here; the player only
        // ever holds a pointer to one.
        class Parameters {};

        // 0 == cut / no pause updates, 2 == updates-during-pause.
        void SetInterpolationMode(s32 liMode);
        void SetParameters(const Parameters* lpParams);
        void SetupDuration(f32 lfDuration);
        bool HasFinished() const;
        void SetupCameraAFromCamera(const Camera& lrCamera);
        void SetupCameraAFromHelper(BehaviourHelperIndex lHelper, BehaviourManager& lrManager);
        void SetupCameraBFromCamera(const Camera& lrCamera);
        void SetupCameraBFromHelper(BehaviourHelperIndex lHelper, BehaviourManager& lrManager);
        void Setup();
        // The camera this interpolator is producing this frame (the player copies it into
        // its own mCamera while the interpolator is active).
        const Camera& GetCamera() const;
    };

    // A typed handle to a behaviour owned by a BehaviourManager. The player keeps two
    // (the in/out interpolators). All the player ever needs is to query allocation, reach
    // the live behaviour, and clear/release through the manager -- modelled with named
    // methods so callers never touch the handle's internals. The trailing storage sizes
    // the handle so it can be embedded by value.
    // FLAG: minimal slice -- no reconstructed home yet.
    template <typename TBehaviour>
    class BehaviourHandle
    {
    public:
        BehaviourHandle() : mbAllocated(false), muAllocationKey(0), mpManager(0), mpBehaviour(0) {}

        // True when this handle currently owns an allocated behaviour.
        bool IsAllocated() const { return mbAllocated; }

        // The manager that owns this handle's behaviour (NULL when unallocated).
        BehaviourManager* GetManager() const { return mpManager; }

        // The live behaviour (only valid while IsAllocated()).
        TBehaviour* GetBehaviour() const { return mpBehaviour; }

        // Drop the handle to its empty state (the player clears it after releasing).
        void Clear() { mbAllocated = false; muAllocationKey = 0; mpManager = 0; mpBehaviour = 0; }

    private:
        bool              mbAllocated;     // +0x00  owns a behaviour
        u32               muAllocationKey; // +0x04  manager-side allocation key
        BehaviourManager* mpManager;       // +0x08  owning manager
        TBehaviour*       mpBehaviour;     // +0x0C  resolved behaviour
    };

    // The owner of all live camera behaviours. The player allocates / releases its two
    // interpolators through it and sets per-behaviour pause-update state. The methods are
    // the named operations the player invokes -- DECLARATION-ONLY.
    // FLAG: minimal slice -- no reconstructed home yet.
    class BehaviourManager
    {
    public:
        // Allocate a fresh BehaviourInterpolate into lrHandle. The trailing selectors
        // mirror the recorded allocation request (priority / group / active).
        void NewBehaviourInterpolate(BehaviourHandle<BehaviourInterpolate>& lrHandle,
                                     s32 liArgA, s32 liArgB, s32 liArgC);
        // Release the behaviour a handle owns (leaves the handle ready to Clear()).
        void ReleaseBehaviour(BehaviourHandle<BehaviourInterpolate>& lrHandle);
        // Whether a behaviour keeps updating while the game is paused.
        void SetBehaviourUpdatesDuringPause(BehaviourHandle<BehaviourInterpolate>& lrHandle,
                                            bool lbUpdatesDuringPause);
    };
}

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
