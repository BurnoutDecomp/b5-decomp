#include "GameSource/Gui/BrnGuiMovieManager.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"      // BundleLoader (PC sync loader)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h"  // RegisterAllResourceTypes
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"           // E_RESOURCETYPE_VIDEODATA
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT (VideoDefinition::Copy)

#include <cstdlib>   // malloc / free
#include <cstdio>    // snprintf (boot-video diagnostics)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"   // ResolveResourceType

// BrnGui::MovieManager -- faithful reconstruction from the X360 ARTIST build (Construct 0x824F9598,
// Prepare 0x82514780, Update 0x82507A98, RecvEvent 0x824F9688, QueueNextMovie 0x824FF898). The real
// Update is entangled with subsystems not yet reconstructed; those calls are kept as [STUB]s (marked
// per-call) so the state machine is faithful + compiles. The chain cannot actually PLAY until the
// GameDataModule (which loads VIDEOLIST.BUNDLE + fulfils the resource acquire) and the collision/car-pool/
// MovieAllocator/audio subsystems land. The state machine is reconstructed single-step-per-Update (the
// X360 chains a few transitions within one call for efficiency; functionally equivalent over frames).

namespace BrnGui
{
    MovieManager* gpActiveMovieManager = 0;

    namespace
    {
        // X360 QueueNextMovie: StrCpy "VIDEOS\\" then StrCat the VideoFile name. The player opens this path.
        void BuildMoviePath(char* lpacBuffer, u32 luBufferSize, const char* lpcName)
        {
            const char* lpcDir = "VIDEOS\\";
            u32 lu = 0;
            for (; lpcDir[lu] != 0 && lu < luBufferSize - 1u; ++lu) lpacBuffer[lu] = lpcDir[lu];
            for (u32 li = 0; lpcName[li] != 0 && lu < luBufferSize - 1u; ++li, ++lu) lpacBuffer[lu] = lpcName[li];
            lpacBuffer[lu] = 0;
        }
    }

    // ---- VideoDefinition -----------------------------------------------------------------------------
    void MovieManager::VideoDefinition::Construct() { Prepare(); }

    // VideoDefinition::Prepare -- X360 ARTIST @0x82472990 (boot-trace executed).
    //   stvx128 v0 -> +0x00 : rectangle = {0.0, 0.0, 1.0, 1.0} (flt_82001CC0=0.0f, flt_82001C98=1.0f)
    //   std r3   -> +0x10 : CgsResource::ID::HashString(&unk_820046A7) -- a default video-name hash.
    //                       The literal string at 0x820046A7 is not in the export (address only), so the
    //                       hash argument is an honest placeholder; the call structure is faithful.
    //   stw      -> +0x18 : miCrossfadeInFrames = dword_830082A8 (a .data crossfade-default global)
    //   stw 0    -> +0x1C, +0x20 ; stb 0 -> +0x24, +0x25, +0x26
    void MovieManager::VideoDefinition::Prepare()
    {
        mafRectangle[0] = 0.0f; mafRectangle[1] = 0.0f;
        mafRectangle[2] = 1.0f; mafRectangle[3] = 1.0f;
        mVideoResourceId = static_cast<u32>(CgsResource::ID::HashString(
            reinterpret_cast<const u8*>("")));   // [unrecoverable: literal @0x820046A7]
        mSoundStreamName = 0;                     // high dword of the +0x10 std (32-bit hasher -> 0)
        miCrossfadeInFrames = 0;                  // dword_830082A8 default (.data; value not in export)
        miCrossfadeOutFrames = 0;
        muField20 = 0;
        mbPreload = false;
        mbKeepMemoryWhenFinished = false;
        mbDisableCustomSoundtracks = false;
    }

    void MovieManager::VideoDefinition::Release() { mVideoResourceId = 0; }

    // VideoDefinition::Copy -- X360 ARTIST @0x824EAFD8. Asserts src non-null, then copies the
    // VMX rectangle (+0x00, 16B), the 64-bit id (+0x10, std), +0x18/+0x1C/+0x20 (stw), and the
    // 3 bool bytes (+0x24/+0x25/+0x26). Member-by-name; equivalent to a full memberwise copy.
    void MovieManager::VideoDefinition::Copy(const VideoDefinition* lpOther)
    {
        CGS_ASSERT(lpOther != 0, "null src defined");
        if (lpOther == 0) return;
        mafRectangle[0] = lpOther->mafRectangle[0];
        mafRectangle[1] = lpOther->mafRectangle[1];
        mafRectangle[2] = lpOther->mafRectangle[2];
        mafRectangle[3] = lpOther->mafRectangle[3];
        mVideoResourceId = lpOther->mVideoResourceId;
        mSoundStreamName = lpOther->mSoundStreamName;
        miCrossfadeInFrames = lpOther->miCrossfadeInFrames;
        miCrossfadeOutFrames = lpOther->miCrossfadeOutFrames;
        muField20 = lpOther->muField20;
        mbPreload = lpOther->mbPreload;
        mbKeepMemoryWhenFinished = lpOther->mbKeepMemoryWhenFinished;
        mbDisableCustomSoundtracks = lpOther->mbDisableCustomSoundtracks;
    }

    // ---- lifecycle (ARTIST Construct 0x824F9598 / Prepare 0x82514780) --------------------------------
    void MovieManager::Construct()
    {
        miMoveMemoryReleaseDelay = 0;
        mMoviePlayer.Construct();
        mPlayingMovie.Prepare();
        mQueuedMovie.Prepare();
        mReceiverQueue.Construct();
        meCollisionWorldState = E_COLLISIONWORLDSTATE_VALID;
        meCarPoolState = E_CARPOOLSTATE_VALID;
        meLanguage = 0;                     // English
        mbKeepMemoryWhenFinished = false;
        mbUsesXMPMusic = false;
        mbStopVideoStraightAway = false;
        macMovieNameBuffer[0] = 0;
        mpcLanguageCode = 0;
        muFirstCollisionBlockAddress = 0;
        muNumCollisionBlocks = 0;
        mapPoolBacking[0] = 0;
        mapPoolBacking[1] = 0;
        mapPoolBacking[2] = 0;
        mbBundleLoaded = false;
        meState = E_MOVIEMANAGERSTATE_CONSTRUCTED;
    }

    // [PC IO] Load VIDEOS\VIDEOLIST.BUNDLE synchronously into mMoviePool -- the same CgsResource::BundleLoader
    // + Pool path LoadAndSetDebugFont uses (the X360 streams it asynchronously via the GameDataModule). Pool
    // sizing mirrors the debug-font pool (over-reserving is safe; the VideoData metadata is small).
    bool MovieManager::LoadVideoListBundle()
    {
        if (mbBundleLoaded)
            return true;

        CgsResource::RegisterAllResourceTypes();

        const u32 KU_POOL_BYTES = 1u * 1024u * 1024u;
        mapPoolBacking[0] = malloc(KU_POOL_BYTES);
        mapPoolBacking[1] = malloc(KU_POOL_BYTES);
        mapPoolBacking[2] = malloc(KU_POOL_BYTES);
        if (mapPoolBacking[0] == 0 || mapPoolBacking[1] == 0 || mapPoolBacking[2] == 0)
        {
            for (u32 li = 0; li < 3; ++li) { free(mapPoolBacking[li]); mapPoolBacking[li] = 0; }
            return false;
        }

        CgsResource::Pool::InitOptions lOptions;
        lOptions.miId = 1;
        lOptions.mpcName = "MovieData";
        for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
        {
            lOptions.maHeapInfo[lt].muMaxNodes       = 64u;
            lOptions.maHeapInfo[lt].muHeapMemorySize = KU_POOL_BYTES - 64u * 1024u;
            lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
            lOptions.mResource.m_baseResources[lt]   = mapPoolBacking[lt];
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = KU_POOL_BYTES;
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
        }
        lOptions.muMaxResources         = 64u;
        lOptions.muMaxImports           = 64u;
        lOptions.miRefCountThreshold    = 0;
        lOptions.miNumDependencies      = 0;
        lOptions.miBankId               = 0;
        lOptions.mbAllowDefragmentation = false;
        mMoviePool.InitPool(&lOptions);

        CgsResource::BundleLoader lLoader;
        const s32 liLoaded = lLoader.LoadBundle("VIDEOS/VIDEOLIST.BUNDLE", &mMoviePool,
                                                CgsResource::ResolveResourceType);
        CgsDev::Log::WriteToLog(liLoaded > 0
            ? "[MovieManager] VIDEOS/VIDEOLIST.BUNDLE loaded.\n"
            : "[MovieManager] VIDEOS/VIDEOLIST.BUNDLE missing/unreadable -> no videos available.\n");
        mbBundleLoaded = (liLoaded > 0);
        return mbBundleLoaded;
    }

    // [PC IO] Resolve the VideoDataResource for luResId out of the loaded bundle (the X360 waits for the
    // async EVENT_ACQUIRERESOURCE; the PC port looks it up in the pool synchronously). Falls back to the
    // first VideoData resource if the id lookup misses (single-video bundle / id-scheme mismatch).
    bool MovieManager::AcquireVideoDataResource(u32 luResId)
    {
        if (!mbBundleLoaded)
            return false;

        s32 liIndex = -1;
        CgsResource::ID lId;
        lId.SetHash(luResId);
        CgsResource::Entry* lpEntry = mMoviePool.FindResource(lId, false, static_cast<u16>(0xFFFF), &liIndex);
        if (lpEntry == 0)
            lpEntry = mMoviePool.FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_VIDEODATA, &liIndex);
        {
            char lac[96];
            std::snprintf(lac, sizeof(lac), "[MovieManager] acquire id=0x%08X entry=%s idx=%d\n",
                          luResId, (lpEntry ? "found" : "NOT FOUND"), liIndex);
            CgsDev::Log::WriteToLog(lac);
        }
        if (lpEntry == 0)
            return false;

        void* lpResource = lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
        if (lpResource == 0)
        {
            CgsDev::Log::WriteToLog("[MovieManager] acquire: entry has no main-memory resource\n");
            return false;
        }
        void* lpVoid = lpResource;
        mpVideoDataResource.SetResource(&lpVoid);
        return true;
    }

    bool MovieManager::Prepare(s32 leLanguage)
    {
        meLanguage = leLanguage;
        if (meState == E_MOVIEMANAGERSTATE_CONSTRUCTED || meState == E_MOVIEMANAGERSTATE_RELEASED)
        {
            // [PC IO] X360 issues an async LoadBundleRequest for VIDEOS\VIDEOLIST.BUNDLE and waits for the
            // GameDataModule's LoadBundleResponse; the PC port loads it synchronously here (same path as the
            // debug font). The manager goes IDLE either way -- if the bundle is missing, no videos resolve.
            LoadVideoListBundle();
            mPlayingMovie.Prepare();
            mQueuedMovie.Prepare();
            meState = E_MOVIEMANAGERSTATE_IDLE;
        }
        return meState == E_MOVIEMANAGERSTATE_IDLE;
    }

    bool MovieManager::Release()
    {
        mMoviePlayer.Release();
        mPlayingMovie.Release();
        mQueuedMovie.Release();
        mReceiverQueue.Release();
        for (u32 li = 0; li < 3; ++li)
        {
            if (mapPoolBacking[li] != 0) { free(mapPoolBacking[li]); mapPoolBacking[li] = 0; }
        }
        mbBundleLoaded = false;
        meState = E_MOVIEMANAGERSTATE_RELEASED;
        return true;
    }

    void MovieManager::Destruct()
    {
        mMoviePlayer.Destruct();
        meState = E_MOVIEMANAGERSTATE_DESTRUCTED;
    }

    // ---- events (ARTIST RecvEvent 0x824F9688: 504 audio / 508 play / 509 stop) -----------------------
    void MovieManager::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventId)
    {
        switch (liEventId)
        {
        case KI_GUIEVENT_AUDIO_READY:   // 504
            if (meState == E_MOVIEMANAGERSTATE_WAITING_FOR_AUDIO)
                meState = E_MOVIEMANAGERSTATE_PLAYING_MOVIE;
            break;
        case KI_GUIEVENT_PLAY_VIDEO:    // 508
            HandlePlayVideoEvent(static_cast<const GuiEventPlayVideo*>(lpEvent));
            mbStopVideoStraightAway = false;
            break;
        case KI_GUIEVENT_STOP_VIDEO:    // 509
            HandleStopVideoEvent(static_cast<const GuiEventStopVideo*>(lpEvent));
            break;
        default:
            break;
        }
    }

    void MovieManager::HandlePlayVideoEvent(const GuiEventPlayVideo* lpEv)
    {
        if (lpEv == 0) return;
        mQueuedMovie.mafRectangle[0] = lpEv->mafRectangle[0];
        mQueuedMovie.mafRectangle[1] = lpEv->mafRectangle[1];
        mQueuedMovie.mafRectangle[2] = lpEv->mafRectangle[2];
        mQueuedMovie.mafRectangle[3] = lpEv->mafRectangle[3];
        mQueuedMovie.mVideoResourceId = lpEv->muVideoResourceId;
        mQueuedMovie.miCrossfadeInFrames = lpEv->miCrossfadeInFrames;
        mQueuedMovie.miCrossfadeOutFrames = lpEv->miCrossfadeOutFrames;
        mQueuedMovie.mbPreload = lpEv->mbPreload;
        mQueuedMovie.mbKeepMemoryWhenFinished = lpEv->mbKeepMemoryWhenFinished;
        mQueuedMovie.mbDisableCustomSoundtracks = lpEv->mbDisableCustomSoundtracks;
    }

    void MovieManager::HandleStopVideoEvent(const GuiEventStopVideo* lpEv)
    {
        if (lpEv == 0) return;
        mbStopVideoStraightAway = lpEv->mbStopStraightAway;
        if (meState == E_MOVIEMANAGERSTATE_PLAYING_MOVIE)
            meState = E_MOVIEMANAGERSTATE_STOP_MOVIE;
    }

    // ---- [STUBBED SUBSYSTEMS] (marked) ---------------------------------------------------------------
    // [stub: collision-world memory reclaim] X360 swaps the collision world out/in to free movie memory;
    // here the state flips immediately so the machine progresses. Real impl needs the collision subsystem.
    void MovieManager::RequestInvalidationOfCollisionWorld()   { meCollisionWorldState = E_COLLISIONWORLDSTATE_INVALID; }
    void MovieManager::RequestValidationOfCollisionWorldState(){ meCollisionWorldState = E_COLLISIONWORLDSTATE_VALID; }
    // [stub: car-pool memory reclaim]
    void MovieManager::RequestInvalidationOfCarPool()          { meCarPoolState = E_CARPOOLSTATE_INVALID; }
    void MovieManager::RequestValidationOfCarPool()            { meCarPoolState = E_CARPOOLSTATE_VALID; }
    // [stub: MovieAllocator] X360 carves a Heap+Linear allocator from the freed memory for the movie.
    bool MovieManager::PrepareMovieAllocator()                 { return true; }
    void MovieManager::DestroyMemoryResourceAndDescriptor()    { }

    u32 MovieManager::PendingVideoDataResourceRequest() const
    {
        // The GuiModule polls this in REQUESTING_MOVIEDATARESOURCE to issue the resource acquire request.
        return (meState == E_MOVIEMANAGERSTATE_REQUESTING_MOVIEDATARESOURCE) ? mQueuedMovie.mVideoResourceId : 0;
    }

    // ---- QueueNextMovie (ARTIST 0x824FF898) ----------------------------------------------------------
    bool MovieManager::QueueNextMovie()
    {
        mPlayingMovie.Copy(&mQueuedMovie);
        mQueuedMovie.Prepare();   // dequeue/reset
        macMovieNameBuffer[0] = 0;

        mMoviePlayer.SetRectangle(mPlayingMovie.mafRectangle[0], mPlayingMovie.mafRectangle[1],
                                  mPlayingMovie.mafRectangle[2], mPlayingMovie.mafRectangle[3]);
        mMoviePlayer.SetCrossfade(mPlayingMovie.miCrossfadeInFrames, mPlayingMovie.miCrossfadeOutFrames);

        // Pick the localized VideoFile from the acquired VideoDataResource. (GetResource avoids the
        // ResourcePtr null-assert.) [stub: language->code map; X360 selects per-language + sound flags.]
        void* lpResource = 0;
        mpVideoDataResource.GetResource(&lpResource);
        if (lpResource == 0)
        {
            CgsDev::Log::WriteToLog("[MovieManager] QueueNextMovie: no VideoDataResource resolved\n");
            return false;
        }
        CgsResource::VideoDataResource* lpVideoData = static_cast<CgsResource::VideoDataResource*>(lpResource);
        CgsResource::VideoDataResource::VideoFile* lpVideoFile =
            lpVideoData->GetVideoFile(static_cast<CgsResource::VideoDataResource::EVideoLanguage>(meLanguage));
        if (lpVideoFile == 0 || lpVideoFile->GetName() == 0)
        {
            CgsDev::Log::WriteToLog("[MovieManager] QueueNextMovie: no VideoFile/name for the language\n");
            return false;
        }
        mpcLanguageCode = "en";

        BuildMoviePath(macMovieNameBuffer, sizeof(macMovieNameBuffer), lpVideoFile->GetName());
        mMoviePlayer.SetMovieFile(macMovieNameBuffer, mPlayingMovie.mbPreload);
        {
            char lac[300];
            std::snprintf(lac, sizeof(lac), "[MovieManager] QueueNextMovie: file '%s'\n", macMovieNameBuffer);
            CgsDev::Log::WriteToLog(lac);
        }
        return true;
    }

    // ---- the state machine (ARTIST Update 0x82507A98), single-step per call --------------------------
    void MovieManager::Update()
    {
        switch (meState)
        {
        case E_MOVIEMANAGERSTATE_CONSTRUCTED:
        case E_MOVIEMANAGERSTATE_PREPARED:
        case E_MOVIEMANAGERSTATE_REQUESTING_AUDIO:   // [stub: audio request issued externally]
        case E_MOVIEMANAGERSTATE_WAITING_FOR_AUDIO:  // advanced by the 504 event
        case E_MOVIEMANAGERSTATE_REPORTING_FINISHED: // terminal; the GUI flow resets us to IDLE
            break;

        case E_MOVIEMANAGERSTATE_STOP_MOVIE:
            mMoviePlayer.Stop();
            meState = E_MOVIEMANAGERSTATE_PLAYING_MOVIE;
            break;

        case E_MOVIEMANAGERSTATE_PLAYING_MOVIE:
            mMoviePlayer.Update();
            if (mMoviePlayer.IsFinished())
            {
                // [stub: XMPRestoreBackgroundMusic]
                meState = E_MOVIEMANAGERSTATE_RELEASING_MOVIE_PLAYER;
            }
            break;

        case E_MOVIEMANAGERSTATE_RELEASING_MOVIE_PLAYER:
            if (!mMoviePlayer.Release())
                break;
            DestroyMemoryResourceAndDescriptor();   // [stub: MovieAllocator Heap+Linear destruct]
            if (mbKeepMemoryWhenFinished)
            {
                meState = E_MOVIEMANAGERSTATE_REPORTING_FINISHED;
            }
            else
            {
                miMoveMemoryReleaseDelay = 10;
                meState = E_MOVIEMANAGERSTATE_DELAYING_RETURN_OF_MEMORY;
            }
            break;

        case E_MOVIEMANAGERSTATE_DELAYING_RETURN_OF_MEMORY:
            if (--miMoveMemoryReleaseDelay > 0)
                break;
            DestroyMemoryResourceAndDescriptor();
            miMoveMemoryReleaseDelay = 10;
            meState = E_MOVIEMANAGERSTATE_DELAYING_VALIDATE;
            break;

        case E_MOVIEMANAGERSTATE_DELAYING_VALIDATE:
            if (--miMoveMemoryReleaseDelay > 0)
                break;
            RequestValidationOfCollisionWorldState();   // [stub] restore collision world
            RequestValidationOfCarPool();               // [stub] restore car pool
            meState = E_MOVIEMANAGERSTATE_RETURNING_MEMORY;
            break;

        case E_MOVIEMANAGERSTATE_RETURNING_MEMORY:
            if (meCollisionWorldState == E_COLLISIONWORLDSTATE_VALID && meCarPoolState == E_CARPOOLSTATE_VALID)
                meState = E_MOVIEMANAGERSTATE_REPORTING_FINISHED;
            break;

        case E_MOVIEMANAGERSTATE_IDLE:
            if (IsMovieQueued())
            {
                if (meCollisionWorldState != E_COLLISIONWORLDSTATE_INVALID ||
                    meCarPoolState != E_CARPOOLSTATE_INVALID)
                {
                    RequestInvalidationOfCollisionWorld();   // [stub] free collision world for the movie
                    RequestInvalidationOfCarPool();          // [stub] free car pool
                }
                meState = E_MOVIEMANAGERSTATE_REQUESTING_MOVIEDATARESOURCE;
            }
            break;

        case E_MOVIEMANAGERSTATE_REQUESTING_MOVIEDATARESOURCE:
            // The GuiModule polls PendingVideoDataResourceRequest() + issues the acquire; advance to wait.
            meState = E_MOVIEMANAGERSTATE_WAITING_FOR_MOVIEDATARESOURCE;
            break;

        case E_MOVIEMANAGERSTATE_WAITING_FOR_MOVIEDATARESOURCE:
            // [PC IO] X360 waits for the async EVENT_ACQUIRERESOURCE (KI_VIDEO_DATA_RESOURCE_EVENT_ID
            // 0x1AFFEED) then mpVideoDataResource.CreateFromHandle(handle). The PC port resolves it
            // synchronously out of the loaded VIDEOLIST.BUNDLE pool (the debug-font pattern).
            if (AcquireVideoDataResource(mQueuedMovie.mVideoResourceId))
                meState = E_MOVIEMANAGERSTATE_WAITING_FOR_MEMORY;
            else
                meState = E_MOVIEMANAGERSTATE_REPORTING_FINISHED;   // not in the bundle -> skip this video
            break;

        case E_MOVIEMANAGERSTATE_WAITING_FOR_MEMORY:
            if (meCollisionWorldState == E_COLLISIONWORLDSTATE_INVALID &&
                meCarPoolState == E_CARPOOLSTATE_INVALID)
            {
                PrepareMovieAllocator();   // [stub]
                QueueNextMovie();
                meState = E_MOVIEMANAGERSTATE_PREPARING_MOVIE_PLAYER;
            }
            break;

        case E_MOVIEMANAGERSTATE_PREPARING_MOVIE_PLAYER:
        {
            const bool lbPrepared = mMoviePlayer.Prepare(mpcLanguageCode);
            char lac[64];
            std::snprintf(lac, sizeof(lac), "[MovieManager] MoviePlayer.Prepare = %d\n", lbPrepared ? 1 : 0);
            CgsDev::Log::WriteToLog(lac);
            if (lbPrepared)
            {
                if (mbStopVideoStraightAway)
                {
                    meState = E_MOVIEMANAGERSTATE_STOP_MOVIE;
                }
                else
                {
                    mMoviePlayer.Play();
                    // X360 goes -> REQUESTING_AUDIO(5) -> [504] WAITING_FOR_AUDIO(6) -> PLAYING(7).
                    // [stub: audio] no sound subsystem yet, so skip straight to PLAYING.
                    meState = E_MOVIEMANAGERSTATE_PLAYING_MOVIE;
                }
            }
            else
            {
                // [diag] the (synchronous) player open failed -> skip rather than spin/retry every frame.
                meState = E_MOVIEMANAGERSTATE_REPORTING_FINISHED;
            }
            break;
        }

        default:
            break;
        }
    }

    void MovieManager::Render(CgsGraphics::Im2dRenderBuffer* lpIm2dRenderBuffer)
    {
        if (meState == E_MOVIEMANAGERSTATE_PLAYING_MOVIE || meState == E_MOVIEMANAGERSTATE_STOP_MOVIE)
            mMoviePlayer.Render(lpIm2dRenderBuffer);
    }
}
