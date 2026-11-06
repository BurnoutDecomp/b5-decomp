from pathlib import Path
import json,os,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
base='6d9418d40a9f00d92e3cc804b7d939008faa8d77'
def git(*args):return subprocess.check_output(['git',*args],cwd=w).decode().rstrip()
assert git('rev-parse','HEAD')=='49ae375da93dafe66b3d165eeb614cecaaa3fa50'
assert not git('status','--porcelain=v1')
git('switch','-c','codex/experimental-x360-r29-movie-prepare',base)
folder='src/GameShared/GameClasses/Graphics/MoviePlayer/'
movie=folder+'CgsMoviePlayer.cpp'
allocator=folder+'CgsMoviePlayerChunkBuffer.cpp'
def edit(path,changes):
    p=w/path
    data=p.read_bytes()
    eol=b'\r\n' if b'\r\n' in data else b'\n'
    for before,after in changes:
        a=before.encode().replace(b'\n',eol)
        b=after.encode().replace(b'\n',eol)
        assert data.count(a)==1,(p,before)
        data=data.replace(a,b)
    p.write_bytes(data)

edit(allocator,[
    ("    // MoviePlayerCoreAllocator::'vector deleting destructor'", """    void MoviePlayerCoreAllocator::Construct(rw::IResourceAllocator* lpAllocator)
    {
        mpAllocator = lpAllocator;
        miAllocations = 0;
    }

    // MoviePlayerCoreAllocator::'vector deleting destructor'""")
])
edit(movie,[
    ('#include "GameShared/GameClasses/Core/CgsAssert.h"', '''#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"'''),
    ('#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"', '''#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "GameShared/GameClasses/System/FileSystem/CgsPatchManager.h"'''),
    ('#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"', '''#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"
#include <cctype>
#include <cstring>
#include <new>

namespace CgsFileSystem
{
    extern PatchManager* gpPatchManager;
}'''),
    ('namespace CgsGraphics\n{',r'''namespace CgsGraphics
{
    namespace
    {
        const u32 KU_MOVIE_BUFFER_SIZE = 0x100000;

        void LogMovieMemoryStatus(const char* lpcFreeMemoryLabel)
        {
            MEMORYSTATUS lStatus;
            GlobalMemoryStatus(&lStatus);
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "XBox 360 memory status: \n";
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lStatus.dwTotalPhys >> 20)
                                          << " total MB of physical memory.\n";
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "    " << static_cast<u32>(lStatus.dwAvailPhys >> 20)
                                          << lpcFreeMemoryLabel;
        }
    }'''),
    ('    // ARTIST 0x827EF200',r'''    // ARTIST 0x827FAE08
    void MoviePlayer::PrepareResources(const char*)
    {
        CGS_ASSERT(mePlayerState == E_RW_MOVIE_PLAYER_NULL,
                   "mePlayerState == E_RW_MOVIE_PLAYER_NULL");

        mAllocator.Construct(mpRwAllocator);
        vp6::SetAllocator(&mAllocator);

        const rw::ResourceDescriptor lRendererDescriptor = MovieVideoRenderer::GetResourceDescriptor();
        mRwVideoRendererResource =
            mpRwAllocator->DoAllocate(lRendererDescriptor, "CgsGraphics::MovieVideoDecoder object");
        void* lpRendererMemory = mRwVideoRendererResource.m_baseResources[0];
        mpRwVideoRenderer = lpRendererMemory ? new (lpRendererMemory) MovieVideoRenderer() : 0;
        mpRwVideoRenderer->SetParentMoviePlayer(this);
        mpRwVideoRenderer->Init(&mAllocator, 1280, 720);

        mMovieStream.Construct(0, 0);
        mpMoviePlayerBuffer = mpRwAllocator->AllocateMemoryResource(KU_MOVIE_BUFFER_SIZE, 128, 0);
        char* lpChunkMemory = static_cast<char*>(
            mpRwAllocator->AllocateMemoryResource(KU_MOVIE_BUFFER_SIZE, 128, 0));
        mChunkBuffer.miBufferSize = KU_MOVIE_BUFFER_SIZE;
        mChunkBuffer.mpcBuffer = lpChunkMemory;
        VP6_VPInitLibrary();
    }

    void MoviePlayer::ResetChunkSet()
    {
        mChunkBuffer.muCurrentChunk = 0;
        mChunkBuffer.muCurrentChunkRead = 0;
        mChunkBuffer.muChunkBufferPos = 0;
        std::memset(mChunkBuffer.mpcBuffer, 0, mChunkBuffer.miBufferSize);
    }

    // ARTIST 0x827FE928
    bool MoviePlayer::Prepare(rw::IResourceAllocator* lpAllocator, const char* lpMovieFileName,
                              const char* lpcLanguageCode)
    {
        switch (mePlayerState)
        {
        case E_RW_MOVIE_PLAYER_NULL:
        {
            CGS_ASSERT(lpAllocator, "lpResourceAllocator");
            mpRwAllocator = lpAllocator;
            PrepareResources(lpcLanguageCode);

            macMovieFileName[0] = 0;
            CGS_ASSERT(lpMovieFileName, "lpMovieFileName");
            CgsCore::StrCpy(macMovieFileName, sizeof(macMovieFileName), lpMovieFileName);
            for (u32 i = 0; i < std::strlen(macMovieFileName); ++i)
                macMovieFileName[i] = static_cast<char>(std::toupper(macMovieFileName[i]));

            char lacPatchedName[256];
            CgsFileSystem::gpPatchManager->RemapFilename(macMovieFileName, lacPatchedName,
                                                        sizeof(lacPatchedName));
            mMovieStream.Open(lacPatchedName, mpMoviePlayerBuffer, KU_MOVIE_BUFFER_SIZE, 16,
                              0, 0, false, -1);
            ResetChunkSet();
        }
        // Fall through to poll the stream's opening state.
        case E_RW_MOVIE_PLAYER_OPENING_FILE:
            mePlayerState = E_RW_MOVIE_PLAYER_OPENING_FILE;
            if (mMovieStream.GetStatus() != CgsFileSystem::StreamDeviceDiskRead::E_STATUS_OPEN)
                return false;

            miNumVideos = 0;
            miNumAudios = 0;
            // Fall through to read the stream headers.
        case E_RW_MOVIE_PLAYER_LOADING_VIDEO_STREAMS:
            mePlayerState = E_RW_MOVIE_PLAYER_LOADING_VIDEO_STREAMS;
            if (!ReadChunkSet(1))
                return false;
            if (std::strncmp(mChunkBuffer.GetChunkId(0), "MVhd", 4) == 0)
            {
                AddVideoStream(0);
                ResetChunkSet();
                return false;
            }
            if (std::strncmp(mChunkBuffer.GetChunkId(0), "S", 1) == 0)
            {
                ++miNumAudios;
                ResetChunkSet();
                return false;
            }
            // Fall through to read the first complete set.
        case E_RW_MOVIE_PLAYER_LOADING_FIRST_BUFFER:
            if (!ReadChunkSet(miNumAudios + miNumVideos))
                return false;

            // Fall through to start the decoders.
        case E_RW_MOVIE_PLAYER_INIT_DECODERS:
        {
            mePlayerState = E_RW_MOVIE_PLAYER_INIT_DECODERS;
            LogMovieMemoryStatus(" free MB of physical memory.\n");

            s32 liTotalHeight = 0;
            for (s32 liVideoIndex = 0; liVideoIndex < miNumVideos; ++liVideoIndex)
            {
                const VP6ChunkHeader& lrHeader = maVP6Headers[liVideoIndex];
                CGS_ASSERT(lrHeader.mWidth == maVP6Headers[0].mWidth,
                           "maVP6Headers[liVideoIndex].mWidth == maVP6Headers[0].mWidth");
                CGS_ASSERT(lrHeader.mHeight % 16 == 0,
                           "(maVP6Headers[liVideoIndex].mHeight % 16) == 0");
                CGS_ASSERT(lrHeader.mTotalFrames == maVP6Headers[0].mTotalFrames,
                           "maVP6Headers[liVideoIndex].mTotalFrames == maVP6Headers[0].mTotalFrames");
                VP6_StartDecoder(&maVP6Decoders[liVideoIndex], lrHeader.mWidth, lrHeader.mHeight,
                                 true, CgsSystem::JobManager(), 63);
                VP6_SetPbParam(maVP6Decoders[liVideoIndex], PBC_SET_POSTPROC, 0);
                liTotalHeight += lrHeader.mHeight;
            }
            CGS_ASSERT(liTotalHeight == 720, "liTotalHeight == 720");

            miCurrentFrame = 0;
            muStartTimeInCycles = 0;
            mbReadyToRender = false;
            StartDecodes();
            WaitForDecode();
            ResetChunkSet();
            LogMovieMemoryStatus("  free MB of physical memory.\n");
        }
        // Fall through to publish the prepared state.
        case E_RW_MOVIE_PLAYER_PREPARED:
            mePlayerState = E_RW_MOVIE_PLAYER_PREPARED;
            return true;

        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer,
                                      CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lMessage << "Unhandled state " << static_cast<s32>(mePlayerState) << "\n";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return false;
        }
    }

    // ARTIST 0x827EF200''')
])
git('diff','--check')
prior=json.loads((d.parent/'movie-player-render-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
args+=prior[prior.index('--source'):]+['--source',allocator]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp';tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
