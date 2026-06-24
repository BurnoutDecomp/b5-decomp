#ifndef BRN_GUI_MOVIE_MANAGER_H
#define BRN_GUI_MOVIE_MANAGER_H

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"      // CgsGraphics::MoviePlayer, Im2dRenderBuffer
#include "GameShared/GameClasses/Graphics/Resources/CgsVideoDataResource.h"  // CgsResource::VideoDataResource
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"           // CgsResource::ResourcePtr
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"          // CgsResource::Pool (holds VIDEOLIST.BUNDLE)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"             // CgsModule::Event, VariableEventQueue
#include "GameSource/Gui/BrnGuiVideoEvents.h"                                // BrnGui::GuiEventPlayVideo/StopVideo (508/509)

// BrnGui::MovieManager -- the GUI-side movie controller, reconstructed from the X360 ARTIST build:
//   Construct        0x824F9598   Prepare       0x82514780   Update    0x82507A98
//   RecvEvent        0x824F9688   QueueNextMovie 0x824FF898   ctor      0x827DEFF0
// Faithful state machine (EMovieManagerState) + members; the boot/attract videos are driven through it by
// the GUI HUD-flow states (BrnGui::BootVideos/BootLegal/BootAttract), and the data comes from
// VIDEOS\VIDEOLIST.BUNDLE.
//
// [PC IO LAYER] The X360 loads the bundle + acquires resources ASYNCHRONOUSLY (GameDataModule +
// EVENT_ACQUIRERESOURCE). The PC port already has a SYNCHRONOUS bundle path -- CgsResource::BundleLoader +
// Pool, the same one LoadAndSetDebugFont uses -- so Prepare loads VIDEOS\VIDEOLIST.BUNDLE in one shot into
// mMoviePool, and the per-video "acquire" is a synchronous Pool::FindResource(id). This is faithful to the
// bundle FORMAT + per-resource fixup/import; only the IO is PC-shaped (marked), exactly like the font.
//
// [STILL STUBBED -- marked per-call in the .cpp] collision-world + car-pool memory reclaim, the MovieAllocator
// (Heap+Linear over a reserved block), and XMP/sound. Reconstructed as marked stubs so the state machine is
// faithful + compiles; they gate console memory behaviour but not PC playback.
namespace BrnGui
{
    class MovieManager
    {
    public:
        // One queued/playing video request (DecFIGS BrnGui::MovieManager::VideoDefinition).
        struct VideoDefinition
        {
            f32  mafRectangle[4];                 // +0x00 mv4Rectangle (left/top/right/bottom)
            u32  mVideoResourceId;                // +0x10 VideoDataResource id (0 = none)
            u32  mSoundStreamName;                // +0x14 mSoundStreamName [stub: CgsSound::Playback::Name -> id]
            s32  miCrossfadeInFrames;             // +0x18 (Prepare seeds from dword_830082A8 default)
            s32  miCrossfadeOutFrames;            // +0x1C
            u32  muField20;                       // +0x20 (Prepare zeroes / Copy copies; @0x82472990/@0x824EAFD8)
            bool mbPreload;                       // +0x24
            bool mbKeepMemoryWhenFinished;        // +0x25
            bool mbDisableCustomSoundtracks;      // +0x26

            void Construct();
            void Prepare();                       // X360 VideoDefinition::Prepare = reset to defaults
            void Release();
            void Copy(const VideoDefinition* lpOther);
        };

        // The 19 manager states (DecFIGS EMovieManagerState; values authoritative).
        enum EMovieManagerState
        {
            E_MOVIEMANAGERSTATE_DESTRUCTED = 0,
            E_MOVIEMANAGERSTATE_CONSTRUCTED = 1,
            E_MOVIEMANAGERSTATE_PREPARING_LOADING_BUNDLE = 2,
            E_MOVIEMANAGERSTATE_PREPARED = 3,
            E_MOVIEMANAGERSTATE_STOP_MOVIE = 4,
            E_MOVIEMANAGERSTATE_REQUESTING_AUDIO = 5,
            E_MOVIEMANAGERSTATE_WAITING_FOR_AUDIO = 6,
            E_MOVIEMANAGERSTATE_PLAYING_MOVIE = 7,
            E_MOVIEMANAGERSTATE_RELEASING_MOVIE_PLAYER = 8,
            E_MOVIEMANAGERSTATE_DELAYING_RETURN_OF_MEMORY = 9,
            E_MOVIEMANAGERSTATE_DELAYING_VALIDATE = 10,
            E_MOVIEMANAGERSTATE_RETURNING_MEMORY = 11,
            E_MOVIEMANAGERSTATE_REPORTING_FINISHED = 12,
            E_MOVIEMANAGERSTATE_IDLE = 13,
            E_MOVIEMANAGERSTATE_REQUESTING_MOVIEDATARESOURCE = 14,
            E_MOVIEMANAGERSTATE_WAITING_FOR_MOVIEDATARESOURCE = 15,
            E_MOVIEMANAGERSTATE_WAITING_FOR_MEMORY = 16,
            E_MOVIEMANAGERSTATE_PREPARING_MOVIE_PLAYER = 17,
            E_MOVIEMANAGERSTATE_RELEASED = 18,
            E_MOVIEMANAGERSTATE_COUNT = 19,
        };

        // Collision-world / car-pool memory states (DecFIGS; the movie reclaims their memory while it plays).
        enum ECollisionWorldState
        {
            E_COLLISIONWORLDSTATE_VALID = 0, E_COLLISIONWORLDSTATE_INVALIDATE = 1, E_COLLISIONWORLDSTATE_INVALIDATING = 2,
            E_COLLISIONWORLDSTATE_INVALID = 3, E_COLLISIONWORLDSTATE_VALIDATE = 4, E_COLLISIONWORLDSTATE_VALIDATING = 5,
        };
        enum ECarPoolState
        {
            E_CARPOOLSTATE_VALID = 0, E_CARPOOLSTATE_INVALIDATE = 1, E_CARPOOLSTATE_INVALIDATING = 2,
            E_CARPOOLSTATE_INVALID = 3, E_CARPOOLSTATE_VALIDATE = 4, E_CARPOOLSTATE_VALIDATING = 5,
        };

        // KI_VIDEO_DATA_RESOURCE_EVENT_ID (DWARF + ARTIST Update assert 0x1AFFEED).
        enum { KI_VIDEO_DATA_RESOURCE_EVENT_ID = 28311277 };

        void Construct();
        bool Prepare(s32 leLanguage);                                     // requests VIDEOS\VIDEOLIST.BUNDLE
        bool Release();
        void Destruct();

        void Update();
        void Render(CgsGraphics::Im2dRenderBuffer* lpIm2dRenderBuffer);
        void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventId);

        u32  GetPlayingMovie() const { return mPlayingMovie.mVideoResourceId; }
        u32  GetQueuedMovie() const  { return mQueuedMovie.mVideoResourceId; }
        bool IsMovieQueued() const   { return mQueuedMovie.mVideoResourceId != 0; }
        u32  PendingVideoDataResourceRequest() const;   // the GuiModule polls this to issue the acquire
        EMovieManagerState GetState() const { return meState; }

        // The manager parks in REPORTING_FINISHED (12) when a video ends -- Update keeps it there (X360
        // Update case 12 self-loops). The GUI flow that owns the manager acknowledges the finish (sends the
        // boot state its videoFinished) and returns the manager to IDLE so the next queued video can play.
        // [bridge] BrnGui::GuiModule::UpdateBootVideoFlow performs that GUI-side reset here.
        bool HasFinishedReporting() const { return meState == E_MOVIEMANAGERSTATE_REPORTING_FINISHED; }
        void AcknowledgeFinishedAndReturnToIdle() { if (HasFinishedReporting()) meState = E_MOVIEMANAGERSTATE_IDLE; }

        // The receiver queue the GUI pushes GuiEventPlayVideo/StopVideo (and the GameDataModule its
        // bundle/acquire responses) onto (X360 EventReceiverQueue<1024,16>).
        CgsModule::VariableEventQueue<1024, 16>* GetReceiverQueue() { return &mReceiverQueue; }

    private:
        bool QueueNextMovie();                          // VideoDataResource -> GetVideoFile(lang) -> player name
        bool LoadVideoListBundle();                     // [PC IO] sync BundleLoader of VIDEOS\VIDEOLIST.BUNDLE
        bool AcquireVideoDataResource(u32 luResId);     // [PC IO] sync Pool::FindResource -> mpVideoDataResource
        void HandlePlayVideoEvent(const GuiEventPlayVideo* lpPlayVideoEvent);
        void HandleStopVideoEvent(const GuiEventStopVideo* lpStopVideoEvent);

        // ---- [STUBBED SUBSYSTEMS] reconstructed as marked stubs (see .cpp) -------------------------
        void RequestInvalidationOfCollisionWorld();     // [stub: collision-world memory reclaim]
        void RequestValidationOfCollisionWorldState();
        void RequestInvalidationOfCarPool();            // [stub: car-pool memory reclaim]
        void RequestValidationOfCarPool();
        bool PrepareMovieAllocator();                   // [stub: MovieAllocator Heap+Linear]
        void DestroyMemoryResourceAndDescriptor();      // [stub: MovieAllocator]

        // ---- members (DecFIGS order; x64-laid-out, not the X360 32-bit offsets) ---------------------
        s32                      miMoveMemoryReleaseDelay;   // delay counter (Update states 9/10)
        EMovieManagerState       meState;
        ECollisionWorldState     meCollisionWorldState;
        ECarPoolState            meCarPoolState;
        s32                      meLanguage;                 // CgsLanguage::ELanguage (0 = English)
        CgsGraphics::MoviePlayer mMoviePlayer;
        uintptr_t                muFirstCollisionBlockAddress;
        u32                      muNumCollisionBlocks;
        CgsModule::VariableEventQueue<1024, 16> mReceiverQueue;
        VideoDefinition          mPlayingMovie;
        VideoDefinition          mQueuedMovie;
        bool                     mbKeepMemoryWhenFinished;   // X360 +0xD55
        bool                     mbUsesXMPMusic;             // X360 +0xD56 [stub: XMP background music]
        CgsResource::ResourcePtr<CgsResource::VideoDataResource> mpVideoDataResource;
        char                     macMovieNameBuffer[256];    // "VIDEOS\<name>" (mapcBuffer)
        const char*              mpcLanguageCode;
        bool                     mbStopVideoStraightAway;

        // [PC IO] VIDEOS\VIDEOLIST.BUNDLE lives here (loaded synchronously, like the debug font's pool); the
        // VideoDataResources are looked up out of it by id. mapPoolBacking holds the three malloc'd heaps.
        CgsResource::Pool        mMoviePool;
        void*                    mapPoolBacking[3];
        bool                     mbBundleLoaded;
        // [omitted, stubbed: mDescriptor / mResourceAllocatorResource / mCarPoolResource /
        //  mCarPoolResourceDescriptor / mAllocator(MovieAllocator) -- the memory-management objects]
    };

    // The movie manager currently owning the screen (the GUI HUD flow sets it; BrnRendererModule::Render
    // draws it through the renderer's Im2d each frame while non-null -- interim until the GUI render path).
    extern MovieManager* gpActiveMovieManager;
}

#endif
