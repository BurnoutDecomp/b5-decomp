#include "GameSource/Gui/BrnGuiMovieManager.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h" // collision memory response owner
#include "GameSource/Resource/BrnGameDataModuleIO.h" // original typed Prepare producer
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // LoadBundleRequest
#include "GameShared/GameClasses/Development/CgsStrStream.h" // original state diagnostic
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"           // E_RESOURCETYPE_VIDEODATA
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT (VideoDefinition::Copy)
#include "GameSource/Sound/Global/BrnMusicEffect.h"                                // MusicEffect::IsCustomSoundtrackActive (Update @0x82507A98)

#include <cstring>   // strlen / memset (X360 QueueNextMovie path construction)
#include <xtl.h>
#include <xmp.h>

// Native ARTIST MovieManager: asynchronous bundle/resource replies, original
// same-call state transitions, and dashboard-music ownership.

namespace BrnGui
{
    MovieManager* gpActiveMovieManager = 0;

    namespace
    {
        // X360 QueueNextMovie: StrCpy "VIDEOS\\" then StrCat the VideoFile name. The player opens this path.
        void BuildMoviePath(char* lpacBuffer, u32 luBufferSize, const char* lpcName)
        {
            const char* lpcDir = "VIDEOS\\";
            char* lpcDest = lpacBuffer;
            do
            {
                const char lc = *lpcDir++;
                *lpcDest++ = lc;
            } while (lpcDest[-1] != 0);

            const u32 luSourceLength = static_cast<u32>(std::strlen(lpcName));
            const u32 luDestLength = static_cast<u32>(std::strlen(lpacBuffer));
            const u32 luBytes = luBufferSize;
            if (luSourceLength + luDestLength >= luBytes - 1u)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    "(strlen(lpcSource)+strlen(lpcDest))<luBytes - 1",
                    "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h",
                    75);
                CgsDev::Assert::EndAssert();
            }

            lpcDest = lpacBuffer + luDestLength;
            do
            {
                const char lc = *lpcName++;
                *lpcDest++ = lc;
            } while (lpcDest[-1] != 0);
        }

        // The DEFAULT VIDEO SOUND NAME that VideoDefinition::Prepare seeds +0x18 from.
        // It is mutable data on the console, and its image initialiser is 0. Its ONE
        // writer is the static-initialiser thunk, whose whole body is
        // `gsuDefaultVideoSoundName = CgsSound::Playback::Name::MakeHash("")`; the empty
        // string hashes to 0, so the value is 0 both in the image and at run time. Every
        // other reference to the word is a load. Kept as a named mutable global rather
        // than folded into Prepare because the console's is mutable data.
        u32 gsuDefaultVideoSoundName = 0u;
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
        mv4Rectangle.x = 0.0f; mv4Rectangle.y = 0.0f;
        mv4Rectangle.z = 1.0f; mv4Rectangle.w = 1.0f;
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
        mv4Rectangle.x = lpOther->mv4Rectangle.x;
        mv4Rectangle.y = lpOther->mv4Rectangle.y;
        mv4Rectangle.z = lpOther->mv4Rectangle.z;
        mv4Rectangle.w = lpOther->mv4Rectangle.w;
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
        // ARTIST827DEFF0 initializes only the resource/descriptor owners here;
        // the state machine is initialized by Construct, then Prepare.
        for (u32 li = 0; li < CgsResource::E_MEMTYPE_NUMTYPES; ++li)
            mCarPoolResource.m_baseResources[li] = 0;
    }

    // ---- lifecycle (ARTIST Construct 0x824F9598 / Prepare 0x82514780) --------------------------------
    void MovieManager::Construct()
    {
        // ARTIST824F9598, in original order. Do not seed host-only movie/audio state.
        mResourceAllocatorResource.m_baseResources[0] = 0;
        mMoviePlayer.Construct();
        mPlayingMovie.Prepare();
        mQueuedMovie.Prepare();
        mReceiverQueue.Construct();
        mAllocator.Construct();
        meCollisionWorldState = E_COLLISIONWORLDSTATE_VALID;
        meCarPoolState = E_CARPOOLSTATE_VALID;
        meState = E_MOVIEMANAGERSTATE_CONSTRUCTED;
    }

    // ARTIST82514780: publish one load request, then wait for its receiver reply.
    bool MovieManager::Prepare(BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
        const BrnResource::GameDataIO::OutputBuffer*, CgsLanguage::ELanguage leLanguage)
    {
        switch (meState)
        {
        case E_MOVIEMANAGERSTATE_CONSTRUCTED:
        case E_MOVIEMANAGERSTATE_RELEASED:
        {
            CgsResource::Events::LoadBundleRequest lRequest;
            lRequest.mpUser = &mReceiverQueue;
            lRequest.miEventId = 2;
            lRequest.SetFileName("VIDEOS\\VIDEOLIST.BUNDLE");
            lRequest.mbLiveUpdateReplace = false;
            lRequest.mbAllowFailiure = false;
            lRequest.mbUseHDCache = false;
            lRequest.miPoolId = 11;
            lpGameDataInput->GetRequestInterface()->mRequestQueue.AddEvent(&lRequest, 2);
        }
            // Fall through: the same call may consume an already-delivered reply.
        case E_MOVIEMANAGERSTATE_PREPARING_LOADING_BUNDLE:
            meState = E_MOVIEMANAGERSTATE_PREPARING_LOADING_BUNDLE;
            if (mReceiverQueue.GetLength() < 1)
                return false;
            mReceiverQueue.Clear();
            mPlayingMovie.Prepare();
            mQueuedMovie.Prepare();
            meCollisionWorldState = E_COLLISIONWORLDSTATE_VALID;
            meCarPoolState = E_CARPOOLSTATE_VALID;
            meLanguage = leLanguage;
            meState = E_MOVIEMANAGERSTATE_IDLE;
            return true;
        default:
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStream << "Unhandled state [" << static_cast<s32>(meState)
                    << "] in MovieManager::Prepare";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                "..\\..\\..\\GameSource\\Gui/BrnGuiMovieManager.cpp", 155);
            CgsDev::Assert::EndAssert();
            return false;
        }
        }
    }

    bool MovieManager::Release()
    {
        // Original inline lifecycle: ARTIST GuiModule::Release stage9
        // (824F93xx) advances without a movie call or movie-member store.
        // The movie state machine owns asynchronous player/memory release.
        return true;
    }

    void MovieManager::Destruct()
    {
        // Original inline lifecycle; ARTIST82507690 performs no MovieManager
        // teardown. Its embedded C++ owners retain their normal destructors.
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
        mQueuedMovie.mv4Rectangle.x = lpEv->mafRectangle[0];
        mQueuedMovie.mv4Rectangle.y = lpEv->mafRectangle[1];
        mQueuedMovie.mv4Rectangle.z = lpEv->mafRectangle[2];
        mQueuedMovie.mv4Rectangle.w = lpEv->mafRectangle[3];
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
        // ARTIST 0x824F17C0 does not read the event payload; the state selects stop behavior.
        if (lpEv == 0) return;
        switch (meState)
        {
        case E_MOVIEMANAGERSTATE_STOP_MOVIE:
        case E_MOVIEMANAGERSTATE_RELEASING_MOVIE_PLAYER:
        case E_MOVIEMANAGERSTATE_DELAYING_RETURN_OF_MEMORY:
        case E_MOVIEMANAGERSTATE_DELAYING_VALIDATE:
        case E_MOVIEMANAGERSTATE_RETURNING_MEMORY:
        case E_MOVIEMANAGERSTATE_REPORTING_FINISHED:
        case E_MOVIEMANAGERSTATE_IDLE:
            mQueuedMovie.Prepare();
            break;
        case E_MOVIEMANAGERSTATE_REQUESTING_AUDIO:
        case E_MOVIEMANAGERSTATE_WAITING_FOR_AUDIO:
        case E_MOVIEMANAGERSTATE_PLAYING_MOVIE:
            meState = E_MOVIEMANAGERSTATE_STOP_MOVIE;
            mQueuedMovie.Prepare();
            break;
        case E_MOVIEMANAGERSTATE_REQUESTING_MOVIEDATARESOURCE:
        case E_MOVIEMANAGERSTATE_WAITING_FOR_MOVIEDATARESOURCE:
        case E_MOVIEMANAGERSTATE_WAITING_FOR_MEMORY:
        case E_MOVIEMANAGERSTATE_PREPARING_MOVIE_PLAYER:
            mbStopVideoStraightAway = true;
            break;
        default:
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStream << "Invalid state " << static_cast<s32>(meState)
                    << " when requesting video stop";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                "..\\..\\..\\GameSource\\Gui/BrnGuiMovieManager.cpp", 740);
            CgsDev::Assert::EndAssert();
            break;
        }
        }
    }

    // ---- collision-world / car-pool state requests (ARTIST-faithful) ---------------------------------
    // These four only flip the state to the transitional value (INVALIDATE/VALIDATE); the X360 subsystem
    // (collision-world swap-out / car-pool free) then advances it to INVALID/VALID asynchronously. The PC
    // replies advance the state only after the corresponding resource operation completes.

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

    // Inlined at ARTIST82519894..825198B0; the method records the returned block backing.
    void MovieManager::FreeCollisionPoolMemory(
        const BrnResource::GameDataIO::SwapOutCollisionWorldResponse* lpResponse)
    {
        muFirstCollisionBlockAddress = reinterpret_cast<uintptr_t>(lpResponse->GetResource().m_baseResources[0]);
        muNumCollisionBlocks = lpResponse->GetDescriptor().m_baseResourceDescriptors[0].m_size >> 20;
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

    bool MovieManager::PrepareMovieAllocator()
    {
        // ARTIST82507E38..8C: use the returned car-pool storage. The physical
        // region starts at lane1; the main heap follows its first16MiB.
        if (mResourceAllocatorResource.m_baseResources[0] == 0)
        {
            mResourceAllocatorResource.m_baseResources[0] = mCarPoolResource.m_baseResources[0];
            mResourceAllocatorResource.m_baseResources[1] = 0;
            mResourceAllocatorResource.m_baseResources[2] = mCarPoolResource.m_baseResources[1];
            mResourceAllocatorResource.m_baseResources[3] = 0;
            mResourceAllocatorResource.m_baseResources[4] = 0;
            mDescriptor.m_baseResourceDescriptors[0].m_size = 0x1000000;
            mDescriptor.m_baseResourceDescriptors[0].m_alignment = 128;
            mDescriptor.m_baseResourceDescriptors[2].m_size =
                mCarPoolResourceDescriptor.m_baseResourceDescriptors[1].m_size;
            mDescriptor.m_baseResourceDescriptors[2].m_alignment = 128;
            mResourceAllocatorResource.m_baseResources[0] =
                static_cast<u8*>(mCarPoolResource.m_baseResources[1]) + 0x1000000;
        }
        return mAllocator.Prepare(mResourceAllocatorResource, mDescriptor);
    }

    void MovieManager::DestroyMemoryResourceAndDescriptor()
    {
        CGS_ASSERT(mResourceAllocatorResource.m_baseResources[0] != 0,
                   "mResourceAllocatorResource.GetMemoryResource()");
        XPhysicalFree(mResourceAllocatorResource.m_baseResources[0]);
        mResourceAllocatorResource.m_baseResources[0] = 0;
    }

    // PendingVideoDataResourceRequest (ARTIST @0x824F7808): the GuiModule polls this each PreWorldUpdate.
    // While in REQUESTING_MOVIEDATARESOURCE(14) it advances to WAITING_FOR_MOVIEDATARESOURCE(15), asserts a
    // movie is queued, and returns the queued video's resource ID; otherwise returns a default-name hash.
    CgsResource::ID MovieManager::PendingVideoDataResourceRequest()
    {
        CgsResource::ID lId;
        if (meState == E_MOVIEMANAGERSTATE_REQUESTING_MOVIEDATARESOURCE)
        {
            meState = E_MOVIEMANAGERSTATE_WAITING_FOR_MOVIEDATARESOURCE;
            if (!IsMovieQueued())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("IsMovieQueued()",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiMovieManager.cpp", 641);
                CgsDev::Assert::EndAssert();
            }
            lId.SetHash(mQueuedMovie.mVideoResourceId);
        }
        else
        {
            // ARTIST824F7880..8C calls HashString on the attested empty literal at 0x820046A7.
            lId.SetHash(static_cast<u64>(CgsResource::ID::HashString(
                reinterpret_cast<const u8*>(""))));
        }
        return lId;
    }

    // ---- QueueNextMovie (ARTIST 0x824FF898) ----------------------------------------------------------
    bool MovieManager::QueueNextMovie()
    {
        if (mQueuedMovie.mVideoResourceId == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                " no movie queued, but we are attempting to queue one up?",
                "..\\..\\..\\GameSource\\Gui/BrnGuiMovieManager.cpp",
                759);
            CgsDev::Assert::EndAssert();
        }
        if (!mMoviePlayer.IsFinished())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                " we're attempting to queue a movie when the current one has not finished playing!",
                "..\\..\\..\\GameSource\\Gui/BrnGuiMovieManager.cpp",
                760);
            CgsDev::Assert::EndAssert();
        }

        mPlayingMovie.Copy(&mQueuedMovie);
        mQueuedMovie.Prepare();
        std::memset(macMovieNameBuffer, 0, sizeof(macMovieNameBuffer));

        mMoviePlayer.SetRectangle(mPlayingMovie.mv4Rectangle.x, mPlayingMovie.mv4Rectangle.y,
                                  mPlayingMovie.mv4Rectangle.z, mPlayingMovie.mv4Rectangle.w);
        mMoviePlayer.SetCrossfade(mPlayingMovie.miCrossfadeInFrames, mPlayingMovie.miCrossfadeOutFrames);

        // The original .data pointers resolve to these exact code strings. The language table below
        // controls the video selection; the per-file sound flag controls whether that code replaces EN.
        mpcLanguageCode = "EN";
        const char* lpcLocalizedLanguageCode = mpcLanguageCode;
        CgsResource::VideoDataResource::EVideoLanguage leVideoLanguage =
            CgsResource::VideoDataResource::E_VIDEOLANGUAGE_ENGLISH;
        int liVideoFileAssertLine = 786;
        switch (static_cast<CgsLanguage::ELanguage>(meLanguage))
        {
            case CgsLanguage::E_LANGUAGE_FRENCH:
                leVideoLanguage = CgsResource::VideoDataResource::E_VIDEOLANGUAGE_FRENCH;
                lpcLocalizedLanguageCode = "FR";
                liVideoFileAssertLine = 798;
                break;
            case CgsLanguage::E_LANGUAGE_GERMAN:
                leVideoLanguage = CgsResource::VideoDataResource::E_VIDEOLANGUAGE_GERMAN;
                lpcLocalizedLanguageCode = "GE";
                liVideoFileAssertLine = 810;
                break;
            case CgsLanguage::E_LANGUAGE_ITALIAN:
                leVideoLanguage = CgsResource::VideoDataResource::E_VIDEOLANGUAGE_ITALIAN;
                lpcLocalizedLanguageCode = "IT";
                liVideoFileAssertLine = 822;
                break;
            case CgsLanguage::E_LANGUAGE_JAPANESE:
                leVideoLanguage = CgsResource::VideoDataResource::E_VIDEOLANGUAGE_JAPANESE;
                lpcLocalizedLanguageCode = "JA";
                liVideoFileAssertLine = 834;
                break;
            case CgsLanguage::E_LANGUAGE_SPANISH:
                leVideoLanguage = CgsResource::VideoDataResource::E_VIDEOLANGUAGE_SPANISH;
                lpcLocalizedLanguageCode = "SP";
                liVideoFileAssertLine = 846;
                break;
        }

        CgsResource::VideoDataResource::VideoFile* lpVideoFile =
            mpVideoDataResource->GetVideoFile(leVideoLanguage);
        if (lpVideoFile == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpVideoFile",
                "..\\..\\..\\GameSource\\Gui/BrnGuiMovieManager.cpp",
                liVideoFileAssertLine);
            CgsDev::Assert::EndAssert();
        }

        if (lpVideoFile->IsLocalisedSoundAvailable(leVideoLanguage))
            mpcLanguageCode = lpcLocalizedLanguageCode;

        if (mpcLanguageCode == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "mpcLanguageCode",
                "..\\..\\..\\GameSource\\Gui/BrnGuiMovieManager.cpp",
                856);
            CgsDev::Assert::EndAssert();
        }

        BuildMoviePath(macMovieNameBuffer, sizeof(macMovieNameBuffer), lpVideoFile->GetName());
        return true;
    }

    // ---- the state machine (ARTIST Update 0x82507A98), single-step per call --------------------------
    void MovieManager::Update()
    {
        // ARTIST82507A98. Requests are completed only by their native receiver;
        // several transitions deliberately continue in this same update.
        switch (meState)
        {
        case E_MOVIEMANAGERSTATE_CONSTRUCTED:
        case E_MOVIEMANAGERSTATE_REQUESTING_AUDIO:
        case E_MOVIEMANAGERSTATE_WAITING_FOR_AUDIO:
            return;
        case E_MOVIEMANAGERSTATE_STOP_MOVIE:
            mMoviePlayer.Stop();
            meState = E_MOVIEMANAGERSTATE_PLAYING_MOVIE;
            // Fall through.
        case E_MOVIEMANAGERSTATE_PLAYING_MOVIE:
            if (meState == E_MOVIEMANAGERSTATE_PLAYING_MOVIE)
                mMoviePlayer.Update();
            if (!mMoviePlayer.IsFinished())
                return;
            if (mPlayingMovie.mbDisableCustomSoundtracks)
                CGS_ASSERT(XMPRestoreBackgroundMusic() == ERROR_SUCCESS,
                           "XMPRestoreBackgroundMusic() == ERROR_SUCCESS");
            meState = E_MOVIEMANAGERSTATE_RELEASING_MOVIE_PLAYER;
            // Fall through.
        case E_MOVIEMANAGERSTATE_RELEASING_MOVIE_PLAYER:
            if (!mMoviePlayer.Release())
                return;
            mAllocator.Release();
            if (mPlayingMovie.mbKeepMemoryWhenFinished)
                meState = E_MOVIEMANAGERSTATE_REPORTING_FINISHED;
            else
            {
                miMoveMemoryReleaseDelay = 10;
                meState = E_MOVIEMANAGERSTATE_DELAYING_RETURN_OF_MEMORY;
            }
            return;
        case E_MOVIEMANAGERSTATE_DELAYING_RETURN_OF_MEMORY:
            if (--miMoveMemoryReleaseDelay > 0)
                return;
            DestroyMemoryResourceAndDescriptor();
            miMoveMemoryReleaseDelay = 10;
            meState = E_MOVIEMANAGERSTATE_DELAYING_VALIDATE;
            // Fall through: the first validation delay tick occurs now.
        case E_MOVIEMANAGERSTATE_DELAYING_VALIDATE:
            if (--miMoveMemoryReleaseDelay > 0)
                return;
            RequestValidationOfCollisionWorldState();
            RequestValidationOfCarPool();
            // Fall through.
        case E_MOVIEMANAGERSTATE_RETURNING_MEMORY:
            meState = E_MOVIEMANAGERSTATE_RETURNING_MEMORY;
            if (meCollisionWorldState != E_COLLISIONWORLDSTATE_VALID ||
                meCarPoolState != E_CARPOOLSTATE_VALID)
                return;
            // Fall through.
        case E_MOVIEMANAGERSTATE_REPORTING_FINISHED:
            meState = E_MOVIEMANAGERSTATE_REPORTING_FINISHED;
            return;
        case E_MOVIEMANAGERSTATE_IDLE:
            meState = E_MOVIEMANAGERSTATE_IDLE;
            if (!IsMovieQueued())
                return;
            if (meCollisionWorldState != E_COLLISIONWORLDSTATE_INVALID ||
                meCarPoolState != E_CARPOOLSTATE_INVALID)
            {
                RequestInvalidationOfCollisionWorld();
                RequestInvalidationOfCarPool();
            }
            // Fall through. GuiModule::PreWorldUpdate owns publication of the request.
        case E_MOVIEMANAGERSTATE_REQUESTING_MOVIEDATARESOURCE:
            meState = E_MOVIEMANAGERSTATE_REQUESTING_MOVIEDATARESOURCE;
            return;
        case E_MOVIEMANAGERSTATE_WAITING_FOR_MOVIEDATARESOURCE:
        {
            meState = E_MOVIEMANAGERSTATE_WAITING_FOR_MOVIEDATARESOURCE;
            CGS_ASSERT(IsMovieQueued(), "IsMovieQueued()");
            if (mReceiverQueue.GetLength() == 0)
                return;
            CGS_ASSERT(mReceiverQueue.GetLength() == 1, "mReceiverQueue.GetLength() == 1");
            const CgsModule::Event* lpEvent = 0;
            s32 liEventSize;
            const s32 liEventId = mReceiverQueue.GetFirstEvent(&lpEvent, &liEventSize);
            CGS_ASSERT(lpEvent != 0, "lpEvent");
            CGS_ASSERT(liEventId == 4, "liEventId == CgsResource::ResourceIO::EVENT_ACQUIRERESOURCE");
            const CgsResource::Events::AcquireResourceResponse* lpAcquire =
                reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);
            CGS_ASSERT(lpAcquire->miEventId == KI_VIDEO_DATA_RESOURCE_EVENT_ID,
                       "KI_VIDEO_DATA_RESOURCE_EVENT_ID == lpAcquire->GetEventId()");
            CgsResource::ResourceHandle lHandle;
            lHandle.mpResourceMemory = lpAcquire->mpResourceMemory;
            lHandle.mpSourceEntry = lpAcquire->mpSourceEntry;
            mpVideoDataResource = lHandle;
            mReceiverQueue.Clear();
        }
            // Fall through.
        case E_MOVIEMANAGERSTATE_WAITING_FOR_MEMORY:
            CGS_ASSERT(IsMovieQueued(), "IsMovieQueued()");
            meState = E_MOVIEMANAGERSTATE_WAITING_FOR_MEMORY;
            if (meCollisionWorldState != E_COLLISIONWORLDSTATE_INVALID ||
                meCarPoolState != E_CARPOOLSTATE_INVALID)
                return;
            CGS_ASSERT(PrepareMovieAllocator(), "mAllocator.Prepare(mResourceAllocatorResource, mDescriptor)");
            QueueNextMovie();
            meState = E_MOVIEMANAGERSTATE_PREPARING_MOVIE_PLAYER;
            // Fall through.
        case E_MOVIEMANAGERSTATE_PREPARING_MOVIE_PLAYER:
            if (!mMoviePlayer.Prepare(&mAllocator, macMovieNameBuffer, mpcLanguageCode))
                return;
            if (mbStopVideoStraightAway)
                meState = E_MOVIEMANAGERSTATE_STOP_MOVIE;
            else
            {
                mMoviePlayer.Play();
                meState = E_MOVIEMANAGERSTATE_REQUESTING_AUDIO;
            }
            if (mPlayingMovie.mbDisableCustomSoundtracks)
                CGS_ASSERT(XMPOverrideBackgroundMusic() == ERROR_SUCCESS,
                           "XMPOverrideBackgroundMusic() == ERROR_SUCCESS");
            else if (BrnSound::Logic::MusicEffect::IsCustomSoundtrackActive())
                meState = E_MOVIEMANAGERSTATE_PLAYING_MOVIE;
            return;
        default:
            CGS_ASSERT(false, "Unhandled state");
            return;
        }
    }

    void MovieManager::Render(CgsGraphics::Im2dRenderBuffer* lpIm2dRenderBuffer)
    {
        // ARTIST 0x82511240 inlines this call after Update without a manager-state gate.
        // MoviePlayer owns frame readiness and whether anything can be recorded.
        mMoviePlayer.Render(lpIm2dRenderBuffer);
    }
}
