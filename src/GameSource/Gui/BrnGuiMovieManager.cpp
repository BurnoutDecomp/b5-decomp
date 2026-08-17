#include "GameSource/Gui/BrnGuiMovieManager.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"      // BundleLoader (PC sync loader)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h"  // RegisterAllResourceTypes
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"           // E_RESOURCETYPE_VIDEODATA
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT (VideoDefinition::Copy)
#include "GameShared/GameClasses/System/PC/CgsMovieAudioPC.h"                     // PC EA-XMA movie/stream audio

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

        // Derive the movie's audio stream path "SOUND\STREAMS\<base>.SNS" from the video file name
        // (e.g. "CRITERION.VP6" -> "SOUND\STREAMS\CRITERION.SNS"). The X360 looks the sound stream up via
        // the GenericRwacWaveContent (SNR) header keyed by a hashed name; the PC path loads the named
        // .SNS directly (staged like VIDEOS\*.VP6). See the movie-audio dossier.
        void BuildSoundStreamPath(char* lpacBuffer, u32 luBufferSize, const char* lpcVideoName)
        {
            const char* lpcDir = "SOUND\\STREAMS\\";
            u32 lu = 0;
            for (; lpcDir[lu] != 0 && lu < luBufferSize - 1u; ++lu) lpacBuffer[lu] = lpcDir[lu];
            // copy the base name, stopping at the extension dot
            u32 liDot = 0;
            for (u32 li = 0; lpcVideoName[li] != 0; ++li) if (lpcVideoName[li] == '.') liDot = li;
            const u32 luNameLen = liDot ? liDot : 0xFFFFFFFFu;
            for (u32 li = 0; lpcVideoName[li] != 0 && li < luNameLen && lu < luBufferSize - 5u; ++li, ++lu)
                lpacBuffer[lu] = lpcVideoName[li];
            const char* lpcExt = ".SNS";
            for (u32 li = 0; lpcExt[li] != 0 && lu < luBufferSize - 1u; ++li, ++lu) lpacBuffer[lu] = lpcExt[li];
            lpacBuffer[lu] = 0;
        }

        // The PC movie/stream audio player (single movie at a time -> file-static, no header churn).
        CgsSystem::MovieAudioPC g_movieAudio;
        char                    g_acSoundStreamPath[260] = { 0 };

        // dword_830082A8 -- the .data DEFAULT VIDEO SOUND NAME that VideoDefinition::Prepare
        // seeds +0x18 from (`lwz r10, dword_830082A8; stw r10, 0x18(r31)` @0x824729C8/D8).
        // Its image initialiser is 0x0233B6D5, read out of the decrypted XEX. Kept as a
        // named mutable global rather than folded into Prepare because the console's is
        // mutable .data -- if a writer turns up, this is its home.
        u32 gsuDefaultVideoSoundName = 0x0233B6D5u;
    }

    // ---- VideoDefinition -----------------------------------------------------------------------------
    void MovieManager::VideoDefinition::Construct() { Prepare(); }

    // VideoDefinition::Prepare -- X360 ARTIST @0x82472990 (boot-trace executed).
    //   stvx128 v0 -> +0x00 : rectangle = {0.0, 0.0, 1.0, 1.0} (flt_82001CC0=0.0f, flt_82001C98=1.0f)
    //   std r3   -> +0x10 : CgsResource::ID::HashString(&unk_820046A7) -- a default video-name hash.
    //                       The literal string at 0x820046A7 is not in the export (address only), so the
    //                       hash argument is an honest placeholder; the call structure is faithful.
    //   stw      -> +0x18 : mSoundStreamName = dword_830082A8 (the .data default sound name)
    //   stw 0    -> +0x1C, +0x20 ; stb 0 -> +0x24, +0x25, +0x26
    //
    // ⭐ FIELD ATTRIBUTION CORRECTED 2026-08-16 (boot audit F-P8b-6). +0x10 is ONE 8-byte id
    // (`std r3`), not an id plus a sound name; the dword_830082A8 seed at +0x18 IS the sound
    // name; and the two crossfades are +0x1C/+0x20. See the header for the full store list.
    void MovieManager::VideoDefinition::Prepare()
    {
        mafRectangle[0] = 0.0f; mafRectangle[1] = 0.0f;
        mafRectangle[2] = 1.0f; mafRectangle[3] = 1.0f;
        // zero-extended, matching the console's `clrldi r3,r11,32` tail (see BrnBootVideos).
        mVideoResourceId = static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(
            reinterpret_cast<const u8*>(""))));   // the "" literal @0x820046A7
        mSoundStreamName = gsuDefaultVideoSoundName;
        miCrossfadeInFrames = 0;
        miCrossfadeOutFrames = 0;
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
        mbPreload = lpOther->mbPreload;
        mbKeepMemoryWhenFinished = lpOther->mbKeepMemoryWhenFinished;
        mbDisableCustomSoundtracks = lpOther->mbDisableCustomSoundtracks;
    }

    // ---- constructor (ARTIST MovieManager::MovieManager @0x827DEFF0) ----------------------------------
    // The X360 ctor does NOT write meState/meCollisionWorldState/meCarPoolState/meLanguage
    // (+0x4/+0x8/+0xC) -- those are seeded by Construct(). Its (r31)-relative stores target only the
    // 0x8D0-0x908 block (the owned MovieAllocator/GeneralAllocator free-list + its three =1 flag fields
    // @+0x8E4/0x8EC/0x8F4, plus the GeneralAllocator(...,1,...) ctor + vtable off_820CEA44) and the
    // 0xDB0-0xDD0 car-pool region (the three SmallResource base pointers = 0 and the three descriptor
    // {size=0, alignment=1} pairs). The allocator internals are the [omitted, stubbed] subsystem (header);
    // they are intentionally not fabricated here -- only the deterministic car-pool seeding is reproduced.
    MovieManager::MovieManager()
    {
        miMoveMemoryReleaseDelay  = 0;
        muFirstCollisionBlockAddress = 0;
        muNumCollisionBlocks      = 0;
        mbKeepMemoryWhenFinished  = false;
        mbUsesXMPMusic            = false;
        mbStopVideoStraightAway   = false;
        macMovieNameBuffer[0]     = 0;
        mpcLanguageCode           = 0;
        mapPoolBacking[0]         = 0;
        mapPoolBacking[1]         = 0;
        mapPoolBacking[2]         = 0;
        mbBundleLoaded            = false;

        // Car-pool memory descriptor: asm seeds all three pools to {base=0, size=0, alignment=1}
        // (+0xDB0..+0xDD0). SmallResource/ResourceDescriptor have no zero-init, so do it explicitly.
        for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
        {
            mCarPoolResource.m_baseResources[lt] = 0;
            mCarPoolResourceDescriptor.m_baseResourceDescriptors[lt].m_size      = 0;
            mCarPoolResourceDescriptor.m_baseResourceDescriptors[lt].m_alignment = 1;
        }
    }

    // ---- lifecycle (ARTIST Construct 0x824F9598 / Prepare 0x82514780) --------------------------------
    void MovieManager::Construct()
    {
        miMoveMemoryReleaseDelay = 0;
        mMoviePlayer.Construct();
        g_movieAudio.Construct();           // [PC] EA-XMA movie/stream audio player
        g_acSoundStreamPath[0] = 0;
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
    // async EVENT_ACQUIRERESOURCE; the PC port looks it up in the pool synchronously).
    //
    // ⭐ THE TYPE-SCAN FALLBACK IS GONE, 2026-08-16 (boot audit F-P8b-3). This used to fall
    // back to `FindFirstResourceOfType(E_RESOURCETYPE_VIDEODATA)` when the id lookup missed.
    // There is no console analogue -- the acquire is strictly BY HANDLE: the queued 64-bit
    // hash comes back from PendingVideoDataResourceRequest @0x824F7808 (which asserts
    // IsMovieQueued) and goes straight into CreateFromHandle @0x82507DD4; no type scan
    // exists anywhere in the image. The fallback did not make a miss survivable, it made it
    // INVISIBLE: an id-scheme mismatch silently bound and played the WRONG video, which is
    // exactly the class of bug the boot-video work kept chasing. A miss now fails, loudly.
    bool MovieManager::AcquireVideoDataResource(u64 luResId)
    {
        if (!mbBundleLoaded)
            return false;

        s32 liIndex = -1;
        CgsResource::ID lId;
        lId.SetHash(luResId);
        CgsResource::Entry* lpEntry = mMoviePool.FindResource(lId, false, static_cast<u16>(0xFFFF), &liIndex);
        {
            char lac[96];
            std::snprintf(lac, sizeof(lac), "[MovieManager] acquire id=0x%016llX entry=%s idx=%d\n",
                          static_cast<unsigned long long>(luResId),
                          (lpEntry ? "found" : "NOT FOUND"), liIndex);
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
        g_movieAudio.Release();            // [PC] free the decoded movie-audio buffer
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
        // ⭐ the sound name rides across with the id (boot audit F-P8b-5/F-P8b-6): the
        // producer fills it, so the queued definition has to carry it or the whole point of
        // filling it is lost at the first copy.
        mQueuedMovie.mSoundStreamName = lpEv->muSoundStreamName;
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

    // ---- collision-world / car-pool state requests (ARTIST-faithful) ---------------------------------
    // These four only flip the state to the transitional value (INVALIDATE/VALIDATE); the X360 subsystem
    // (collision-world swap-out / car-pool free) then advances it to INVALID/VALID asynchronously. The PC
    // build has no live collision/car-pool subsystem yet, so Update() completes those transitions itself
    // (marked [stub-complete]) -- but the request functions themselves are reconstructed exactly per the asm.

    // ARTIST @0x824EADF8: asserts the collision world is currently VALID(0), then requests INVALIDATE(1).
    void MovieManager::RequestInvalidationOfCollisionWorld()
    {
        CGS_ASSERT(meCollisionWorldState == E_COLLISIONWORLDSTATE_VALID,
                   "Invalidation of collision world requested when collision world wasn't valid");
        if (meCollisionWorldState == E_COLLISIONWORLDSTATE_VALID)
            meCollisionWorldState = E_COLLISIONWORLDSTATE_INVALIDATE;
    }

    // ARTIST @0x824EAEB8: asserts the collision world is INVALID(3), then requests VALIDATE(4).
    void MovieManager::RequestValidationOfCollisionWorldState()
    {
        if (meCollisionWorldState == E_COLLISIONWORLDSTATE_INVALID)
        {
            meCollisionWorldState = E_COLLISIONWORLDSTATE_VALIDATE;
        }
        else
        {
            CGS_ASSERT(false, "Validation of collision world requested when collision world wasn't invalid");
        }
    }

    // ARTIST @0x824EAE58: asserts the car pool is currently VALID(0), then requests INVALIDATE(1).
    void MovieManager::RequestInvalidationOfCarPool()
    {
        CGS_ASSERT(meCarPoolState == E_CARPOOLSTATE_VALID,
                   "Invalidation of car pool requested when collision world wasn't valid");
        if (meCarPoolState == E_CARPOOLSTATE_VALID)
            meCarPoolState = E_CARPOOLSTATE_INVALIDATE;
    }

    // ARTIST @0x824EAF18: asserts the car pool is INVALID(3), then requests VALIDATE(4).
    void MovieManager::RequestValidationOfCarPool()
    {
        if (meCarPoolState == E_CARPOOLSTATE_INVALID)
        {
            meCarPoolState = E_CARPOOLSTATE_VALIDATE;
        }
        else
        {
            CGS_ASSERT(false, "Validation of car pool requested when car pool wasn't invalid");
        }
    }

    // SetCarPoolValid (ARTIST @0x824EAF78): the GUI car-pool-validation flow hands back the re-validated
    // car-pool resource + descriptor and sets meCarPoolState to VALID(0) when lbValid, else INVALID(3).
    // asm: stw (0 if a2 else 3) -> +0xC ; copy 3 dwords from a3 -> +0xDB0 ; copy 6 dwords from a4 -> +0xDBC.
    void MovieManager::SetCarPoolValid(bool lbValid, const CgsResource::SmallResource& lrResource,
                                       const CgsResource::Entry::ResourceDescriptor& lrDescriptor)
    {
        meCarPoolState = lbValid ? E_CARPOOLSTATE_VALID : E_CARPOOLSTATE_INVALID;

        for (u32 li = 0; li < CgsResource::E_MEMTYPE_NUMTYPES; ++li)
            mCarPoolResource.m_baseResources[li] = lrResource.m_baseResources[li];
        for (u32 li = 0; li < CgsResource::E_MEMTYPE_NUMTYPES; ++li)
        {
            mCarPoolResourceDescriptor.m_baseResourceDescriptors[li].m_size =
                lrDescriptor.m_baseResourceDescriptors[li].m_size;
            mCarPoolResourceDescriptor.m_baseResourceDescriptors[li].m_alignment =
                lrDescriptor.m_baseResourceDescriptors[li].m_alignment;
        }
    }

    // [stub: MovieAllocator] X360 carves a Heap+Linear allocator from the freed memory for the movie.
    bool MovieManager::PrepareMovieAllocator()                 { return true; }
    void MovieManager::DestroyMemoryResourceAndDescriptor()    { }

    // PendingVideoDataResourceRequest (ARTIST @0x824F7808): the GuiModule polls this each PreWorldUpdate.
    // While in REQUESTING_MOVIEDATARESOURCE(14) it advances to WAITING_FOR_MOVIEDATARESOURCE(15), asserts a
    // movie is queued, and returns the queued video's resource ID; otherwise returns a default-name hash.
    CgsResource::ID MovieManager::PendingVideoDataResourceRequest()
    {
        CgsResource::ID lId;
        if (meState == E_MOVIEMANAGERSTATE_REQUESTING_MOVIEDATARESOURCE)
        {
            meState = E_MOVIEMANAGERSTATE_WAITING_FOR_MOVIEDATARESOURCE;
            CGS_ASSERT(IsMovieQueued(), "IsMovieQueued()");
            lId.SetHash(mQueuedMovie.mVideoResourceId);
        }
        else
        {
            // asm: CgsResource::ID::HashString(&unk_820046A7) -- default video-name hash. The literal at
            // 0x820046A7 is address-only in the export, so the argument is an honest placeholder (the empty
            // string), matching the existing VideoDefinition::Prepare reconstruction.
            lId.SetHash(static_cast<u64>(CgsResource::ID::HashString(
                reinterpret_cast<const u8*>(""))));   // [unrecoverable: literal @0x820046A7]
        }
        return lId;
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
        // [PC] derive the matching EA-XMA sound stream path (SOUND\STREAMS\<base>.SNS) and decode it
        // NOW -- ahead of MoviePlayer::Play(). The player is wall-clock driven (Play() stamps
        // muStartTick), so decoding between Play() and the audio Start() would let the video clock run
        // on during the (heavy, synchronous) decode and leave the audio lagging the video by the decode
        // time. Decoding here keeps the later Play()+Start() instant and frame-aligned.
        BuildSoundStreamPath(g_acSoundStreamPath, sizeof(g_acSoundStreamPath), lpVideoFile->GetName());
        g_movieAudio.Load(g_acSoundStreamPath);
        {
            char lac[300];
            std::snprintf(lac, sizeof(lac), "[MovieManager] QueueNextMovie: file '%s' (audio '%s')\n",
                          macMovieNameBuffer, g_acSoundStreamPath);
            CgsDev::Log::WriteToLog(lac);
        }
        return true;
    }

    // ---- the state machine (ARTIST Update 0x82507A98), single-step per call --------------------------
    void MovieManager::Update()
    {
        // [stub-complete] On the X360 the collision-world swap-out / car-pool free subsystem advances the
        // transitional INVALIDATE(1)->INVALID(3) and VALIDATE(4)->VALID(0) states asynchronously. The PC
        // build has no live collision/car-pool subsystem yet, so the request is treated as completing
        // immediately here -- the Request* helpers themselves are reconstructed exactly per the ARTIST asm
        // (they set only the transitional state); this block stands in for the missing subsystem's reply.
        if (meCollisionWorldState == E_COLLISIONWORLDSTATE_INVALIDATE)
            meCollisionWorldState = E_COLLISIONWORLDSTATE_INVALID;
        else if (meCollisionWorldState == E_COLLISIONWORLDSTATE_VALIDATE)
            meCollisionWorldState = E_COLLISIONWORLDSTATE_VALID;
        if (meCarPoolState == E_CARPOOLSTATE_INVALIDATE)
            meCarPoolState = E_CARPOOLSTATE_INVALID;
        else if (meCarPoolState == E_CARPOOLSTATE_VALIDATE)
            meCarPoolState = E_CARPOOLSTATE_VALID;

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
            g_movieAudio.Stop();           // [PC] silence the movie sound stream
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
            g_movieAudio.Stop();           // [PC] the movie finished -> stop its sound stream
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
            // On the X360 the GuiModule's PreWorldUpdate polls PendingVideoDataResourceRequest() to obtain
            // the resource ID + advance to WAITING_FOR_MOVIEDATARESOURCE, then issues the acquire. The PC
            // build drives that poll here (the GuiModule bridge isn't wired yet); the call advances state.
            PendingVideoDataResourceRequest();
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
                    // X360 goes -> REQUESTING_AUDIO(5) -> [504] WAITING_FOR_AUDIO(6) -> PLAYING(7):
                    // it requests the movie's EA sound stream, waits for it, then plays both together.
                    // [PC] the stream was already decoded in QueueNextMovie; start it now in lock-step
                    // with the video clock (Play() above). (The faithful SndStream/SndPlayer1 streaming
                    // path + the SNR-by-id lookup are the deferred layers; see the movie-audio dossier.)
                    if (g_movieAudio.IsLoaded())
                        g_movieAudio.Start();
                    meState = E_MOVIEMANAGERSTATE_PLAYING_MOVIE;
                }
            }
            else
            {
                // [diag] the (synchronous) player open failed -> skip rather than spin/retry every frame.
                // [PC] QueueNextMovie already decoded this movie's sound stream and claimed the
                // (silent) output voice for it; release it on the skip exactly as the finished
                // path does, or the never-started stream pins the voice (g_finished stays false)
                // and the menu-music stream can never reclaim the output afterwards -- the
                // "menu music stops for good after the first missing-attract skip" fault.
                g_movieAudio.Stop();
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
