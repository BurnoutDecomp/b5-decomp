#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <malloc.h>
#include "GameSource/Gui/BrnGuiMovieManager.h"
#include "GameSource/Gui/BrnGuiVideoEvents.h"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

static int checks, failures, assertions, retired, arenasFreed, decodes;
static u32 tick;
static void Check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL: %s\n", label); }
}

// Only the hardware/transport boundaries are observed. Production wrapper and
// state placement, heap allocation, ring selection and command writers execute.
struct NativeImage { std::vector<u8> pixels; u32 pitch; };
namespace renderengine
{
    void Texture::Create(Texture* texture, const Parameters* params, const void*)
    {
        auto* image = new NativeImage;
        image->pitch = params->muWidth * 4 + 4;
        image->pixels.resize(image->pitch * params->muHeight, 0xCD);
        texture->mpD3DTexture = reinterpret_cast<IDirect3DBaseTexture9*>(image);
        texture->muWidth = static_cast<u16>(params->muWidth);
        texture->muHeight = static_cast<u16>(params->muHeight);
    }
    void Texture::Destroy(Texture* texture)
    {
        if (!texture->mpD3DTexture) return;
        ++retired;
        delete reinterpret_cast<NativeImage*>(texture->mpD3DTexture);
        texture->mpD3DTexture = nullptr;
    }
    void Texture::Lock(Texture* texture, s32, s32, s32, LockInfo* lock)
    {
        auto* image = reinterpret_cast<NativeImage*>(texture->mpD3DTexture);
        lock->mpBits = image->pixels.data(); lock->muPitch = image->pitch;
    }
    void Texture::Unlock(Texture*, LockInfo*) {}
}
namespace CgsDev { namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char* expression, const char*, int)
    { ++assertions; std::printf("ASSERT: %s\n", expression); return 0; }
    void* EndAssert() { return nullptr; }
} namespace Log { void WriteToLog(const char*) {} } }
namespace CgsSystem
{
    u32 GetSystemTimerBaseTime() { return tick; }
    u32 GetSystemTimerFrequency() { return 1000; }
}
namespace EA { namespace Jobs
{
    // The converted CPU codec does not submit the console encoding job.
    Job::Job(const char*) {}
    Job::~Job() {}
    void Job::Clear() {}
} }
extern "C"
{
    void* XPhysicalAlloc(unsigned long size, unsigned long, unsigned long alignment, unsigned long)
    { return _aligned_malloc(size, alignment); }
    void XPhysicalFree(void* address) { ++arenasFreed; _aligned_free(address); }
    void avcodec_free_context(AVCodecContext** context) { delete *context; *context = nullptr; }
    void avformat_close_input(AVFormatContext**) { Check(false, "unexpected format close"); }
    void av_frame_free(AVFrame**) { Check(false, "unexpected frame close"); }
    void av_packet_free(AVPacket**) { Check(false, "unexpected packet close"); }
    SwsContext* sws_getContext(int, int, AVPixelFormat, int, int, AVPixelFormat,
                              int, SwsFilter*, SwsFilter*, const double*)
    { return reinterpret_cast<SwsContext*>(new int); }
    void sws_freeContext(SwsContext* context) { delete reinterpret_cast<int*>(context); }
    int sws_scale(SwsContext*, const u8* const[], const int[], int, int, u8* const[], const int[])
    { Check(false, "test supplies completed BGRA, not YUV conversion"); return 0; }
}
namespace CgsGui
{
    const renderengine::BlendState* gpGuiBlendStateStandard = nullptr;
    const renderengine::RasterizerState* gpGuiRasterizerStateCullNone = nullptr;
    const CgsGraphics::Im2dTransform gBillboardScreenTransform = [] {
        CgsGraphics::Im2dTransform t = {};
        t.mRightUp.x = 1280; t.mRightUp.w = 720;
        t.mColourScale.x = t.mColourScale.y = t.mColourScale.z = t.mColourScale.w = 255;
        return t;
    }();
}
namespace CgsGraphics
{
    bool MoviePlayer::DecodeFrame() { ++decodes; mbFinished = true; return false; }
    bool MoviePlayer::PrepareResources() { Check(false, "unexpected transport prepare"); return false; }
    bool MoviePlayer::Prepare(rw::IResourceAllocator*, const char*, const char*)
    { Check(false, "unrelated manager acquire arm"); return false; }
    void MoviePlayer::Play() { Check(false, "unrelated manager play arm"); }
    void MoviePlayer::UploadFrame() { Check(false, "EOF has no new frame to upload"); }
    void Im2dRenderBuffer::Dispatch(Im2d*) const { Check(false, "no GPU consumer in CPU regression"); }
}
namespace BrnSound { namespace Logic
{
    struct MusicEffect { static bool IsCustomSoundtrackActive() { return false; } };
} }
namespace BrnGui
{
    struct ObservedAudio
    { void Stop() {} void Start() {} bool IsLoaded() { return false; } } g_movieAudio;
    bool MovieAudioPcLeafEnabled() { return false; }

    // The real manager's unrelated bundle/audio/reclaim dependencies are outside
    // this CPU check. Its unchanged production Update and owner methods below
    // execute against the actual player, allocator and VideoDefinition types.
    struct MovieManagerFixture
    {
#include "movie_manager_enums.inc"
        CgsGraphics::MoviePlayer mMoviePlayer;
        MovieAllocator mAllocator;
        rw::Resource mResourceAllocatorResource;
        rw::ResourceDescriptor mDescriptor;
        MovieManager::VideoDefinition mPlayingMovie = {};
        MovieManager::VideoDefinition mQueuedMovie = {};
        EMovieManagerState meState = E_MOVIEMANAGERSTATE_PLAYING_MOVIE;
        ECollisionWorldState meCollisionWorldState = E_COLLISIONWORLDSTATE_VALID;
        ECarPoolState meCarPoolState = E_CARPOOLSTATE_VALID;
        s32 miMoveMemoryReleaseDelay = 0;
        bool mbUsesXMPMusic = false, mbStopVideoStraightAway = false;
        char macMovieNameBuffer[256] = {};
        const char* mpcLanguageCode = "en";
        MovieManagerFixture() { mAllocator.Construct(); }
        bool PrepareMovieAllocator();
        void DestroyMemoryResourceAndDescriptor();
        void Update();
        void HandlePlayVideoEvent(const GuiEventPlayVideo*);
        bool IsMovieQueued() { return false; }
        void PendingVideoDataResourceRequest() { Check(false, "unrelated acquire arm"); }
        bool AcquireVideoDataResource(u64) { Check(false, "unrelated acquire arm"); return false; }
        bool QueueNextMovie() { Check(false, "unrelated acquire arm"); return false; }
        void RequestInvalidationOfCollisionWorld() { meCollisionWorldState = E_COLLISIONWORLDSTATE_INVALIDATE; }
        void RequestInvalidationOfCarPool() { meCarPoolState = E_CARPOOLSTATE_INVALIDATE; }
        void RequestValidationOfCollisionWorldState() { meCollisionWorldState = E_COLLISIONWORLDSTATE_VALIDATE; }
        void RequestValidationOfCarPool() { meCarPoolState = E_CARPOOLSTATE_VALIDATE; }
    };
}

#include "movie_texture_ownership.inc"

struct BufferAllocator : rw::IResourceAllocator
{
    std::vector<void*> blocks;
    rw::Resource DoAllocate(const rw::ResourceDescriptor& descriptor, const char*) override
    {
        rw::Resource resource;
        for (u32 i = 0; i < rw::KU_RESOURCE_LANE_COUNT; ++i)
            if (descriptor.m_baseResourceDescriptors[i].m_size)
            {
                resource.m_baseResources[i] = _aligned_malloc(descriptor.m_baseResourceDescriptors[i].m_size, 128);
                blocks.push_back(resource.m_baseResources[i]);
            }
        return resource;
    }
    ~BufferAllocator() { for (void* block : blocks) _aligned_free(block); }
};

static const renderengine::TextureState* QueuedState(const CgsGraphics::Im2dRenderBuffer& buffer)
{
    for (auto* command = buffer.GetFirstCommand(); command; command = buffer.GetNextCommand(command))
        if (command->muType == CgsGraphics::IM_CMD_SET_STATE_TEXTURE)
            return static_cast<const CgsGraphics::ImCommandSetStateTexture*>(command)->mpTextureState;
    return nullptr;
}

static void Scenario(bool keep, bool stop)
{
    using namespace CgsGraphics;
    BrnGui::MovieManagerFixture manager;
    Check(manager.PrepareMovieAllocator(), "actual descriptor/allocator prepare");
    auto& player = manager.mMoviePlayer;
    Check(player.GetPlayerState() == MoviePlayer::E_RW_MOVIE_PLAYER_NULL,
          "original Construct begins in NULL before preparation or playback");
    BrnGui::GuiEventPlayVideo playEvent;
    playEvent.mbKeepMemoryWhenFinished = keep;
    manager.HandlePlayVideoEvent(&playEvent);
    manager.mPlayingMovie.Copy(&manager.mQueuedMovie);
    player.SetRectangle(manager.mPlayingMovie.mafRectangle[0], manager.mPlayingMovie.mafRectangle[1],
                        manager.mPlayingMovie.mafRectangle[2], manager.mPlayingMovie.mafRectangle[3]);
    player.mpAllocator = &manager.mAllocator;
    player.mpVideoCtx = new AVCodecContext();
    player.mpVideoCtx->width = player.mpVideoCtx->height = 2;
    player.mpVideoCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    Check(player.EnsureTexture(2, 2), "four resource-backed native textures prepared");
    player.mpStripStaging = new u8[16];
    for (int i = 0; i < 16; ++i) player.mpStripStaging[i] = static_cast<u8>(i);
    player.muStripStagingBytes = 16;
    player.mbReadyToRender = true;
    player.mePlayerState = MoviePlayer::E_PLAYING;
    player.mfFrameRate = 30; player.mfDurationSec = 11.166667;
    manager.mPlayingMovie.mbKeepMemoryWhenFinished = keep;

    BufferAllocator allocator;
    Im2dRenderBuffer buffer;
    buffer.Construct();
    Check(buffer.Prepare(4096, 4096, &allocator, false), "real separate renderer banks prepare");
    auto* renderer = player.mpRwVideoRenderer;
    Check(renderer->GetNumberOfVideoBuffers(rw::movie::VideoRenderable::VIDEOFORMAT_YV12) == 3,
          "three decode buffers remain distinct from four texture slots");
    for (u32 frame = 0; frame < 8; ++frame)
    {
        buffer.Clear();
        const u32 slot = frame % MovieVideoRenderer::KU_NUM_TEXTURES;
        player.Render(&buffer);
        Check(renderer->muTextureIndex == (frame + 1) % 4, "original modulo-four cursor");
        buffer.Swap();
        const auto* state = QueuedState(buffer);
        if (frame == 0)
        {
            const ImCommandRenderPrimitives<Basic2dColouredTexturedVertex>* draw = nullptr;
            Im2dTransform transform = {};
            s32 program = -1, drawProgram = -1;
            std::vector<u32> commandTypes;
            for (const ImCommand* command = buffer.GetFirstCommand(); command;
                 command = buffer.GetNextCommand(command))
            {
                commandTypes.push_back(command->muType);
                if (command->muType == IM_CMD_SET_TRANSFORM)
                    transform = static_cast<const ImCommandSetTransform*>(command)->mTransform;
                if (command->muType == IM_CMD_SET_SHADER_PROGRAM)
                    program = static_cast<const ImCommandSetShaderProgram*>(command)->mi8ShaderProgram;
                if (command->muType == IM_CMD_RENDER_PRIMITIVES)
                {
                    draw = static_cast<const ImCommandRenderPrimitives<Basic2dColouredTexturedVertex>*>(command);
                    drawProgram = program;
                }
            }
            const std::vector<u32> originalOrder = {IM_CMD_BEGIN_RENDERING, IM_CMD_SET_STATE_BLEND,
                IM_CMD_SET_STATE_RASTERIZER, IM_CMD_SET_STATE_TEXTURE, IM_CMD_SET_TRANSFORM,
                IM_CMD_SET_SHADER_PROGRAM, IM_CMD_RENDER_PRIMITIVES,
                IM_CMD_SET_SHADER_PROGRAM, IM_CMD_END_RENDERING};
            Check(commandTypes == originalOrder, "event-driven movie preserves original command order");
            Check(draw && draw->muNumVertices == 4 && static_cast<u32>(draw->mePrimitiveType) == 6,
                  "event-driven movie records original four-vertex strip");
            Check(drawProgram == 2 && program == 0, "movie program 2 is selected and program 0 restored");
            if (draw && draw->muNumVertices == 4)
            {
                f32 minX = draw->mpVertices[0].mv2Pos.x, maxX = minX;
                f32 minY = draw->mpVertices[0].mv2Pos.y, maxY = minY;
                for (u32 vertex = 1; vertex < 4; ++vertex)
                {
                    minX = std::min(minX, draw->mpVertices[vertex].mv2Pos.x);
                    maxX = std::max(maxX, draw->mpVertices[vertex].mv2Pos.x);
                    minY = std::min(minY, draw->mpVertices[vertex].mv2Pos.y);
                    maxY = std::max(maxY, draw->mpVertices[vertex].mv2Pos.y);
                }
                Check(minX == 0 && minY == 0 && maxX == 1 && maxY == 1,
                      "actual event defaults reach unit-screen banked movie vertices");
                Check(transform.mOriginXYZ.x == 0 && transform.mOriginXYZ.y == 0
                      && transform.mRightUp.y == 0 && transform.mRightUp.z == 0
                      && maxX * transform.mRightUp.x == 1280 && maxY * transform.mRightUp.w == 720,
                      "event-to-command rectangle covers the logical screen exactly once");
                Check(transform.mColourScale.x == 255 && transform.mColourScale.y == 255
                      && transform.mColourScale.z == 255 && transform.mColourScale.w == 255,
                      "recorded native transform preserves colour identity");
            }
            auto* image = reinterpret_cast<NativeImage*>(renderer->maTextureInfoTypes[slot].mpTexture->mpD3DTexture);
            bool copied = true, padding = true;
            for (u32 row = 0; row < 2; ++row)
            {
                copied &= std::memcmp(image->pixels.data() + row * image->pitch,
                                      player.mpStripStaging + row * 8, 8) == 0;
                for (u32 byte = 8; byte < image->pitch; ++byte)
                    padding &= image->pixels[row * image->pitch + byte] == 0xCD;
            }
            Check(copied && padding, "BGRA payload uses native row pitch without altering bytes or padding");
        }
        Check(state == renderer->maTextureInfoTypes[slot].mpTextureState, "queued selected slot's real state");
        const uintptr_t arena = reinterpret_cast<uintptr_t>(manager.mResourceAllocatorResource.m_baseResources[0]);
        Check(reinterpret_cast<uintptr_t>(state) >= arena && reinterpret_cast<uintptr_t>(state) + sizeof(*state)
            <= arena + manager.mDescriptor.m_baseResourceDescriptors[0].m_size, "state resides in actual manager arena");
        Check(state && state->mpRaster == renderer->maTextureInfoTypes[slot].mpTexture, "state retains its typed raster");
    }
    const auto* queued = QueuedState(buffer);
    auto* texture = queued->mpRaster;
    auto* native = texture->mpD3DTexture;
    Check(native != nullptr, "published frame has native texture");
    const int retireBefore = retired, freeBefore = arenasFreed;
    if (stop)
    {
        manager.meState = BrnGui::MovieManagerFixture::E_MOVIEMANAGERSTATE_STOP_MOVIE;
        manager.Update();
        const u32 before = buffer.mpWriteBuffer->muCommandBufferWritePos;
        player.Render(&buffer);
        Check(buffer.mpWriteBuffer->muCommandBufferWritePos == before, "stopped player emits no new draw");
        manager.Update();
    }
    else
    {
        const int decodeBefore = decodes;
        manager.Update();
        Check(decodes == decodeBefore + 1 && player.mbFinished, "actual Update observes transport EOF");
    }
    Check(manager.meState == BrnGui::MovieManagerFixture::E_MOVIEMANAGERSTATE_RELEASING_MOVIE_PLAYER,
          "finished manager enters original release state");
    buffer.Clear(); // other producer bank; the previous queued state remains live.
    manager.Update(); // production ReleaseResources frees metadata, not queued texture owners.
    Check(player.mpRwVideoRenderer == nullptr && player.mpAllocator == nullptr
          && player.mePlayerState == MoviePlayer::E_RW_MOVIE_PLAYER_NULL,
          "decoder and renderer metadata release completes");
    Check(queued->mpRaster == texture && texture->mpD3DTexture == native,
          "prior bank can consume same valid state/native texture after release");
    Check(retired == retireBefore && arenasFreed == freeBefore, "release does not retire arena/device owner");
    if (keep)
    {
        Check(manager.meState == BrnGui::MovieManagerFixture::E_MOVIEMANAGERSTATE_REPORTING_FINISHED,
              "actual playing definition's keep byte selects retained arena");
        for (int i = 0; i < 12; ++i) manager.Update();
        Check(arenasFreed == freeBefore && texture->mpD3DTexture == native,
              "keep-memory survives report updates without timer expiry");
        Check(manager.PrepareMovieAllocator(), "retained arena reconstructs for next movie");
        Check(retired == retireBefore + 4 && arenasFreed == freeBefore, "reconstruction retires four native images, retains backing");
        manager.mAllocator.Release();
        manager.DestroyMemoryResourceAndDescriptor();
    }
    else
    {
        Check(manager.miMoveMemoryReleaseDelay == 10, "original delay is ten manager updates");
        for (int i = 0; i < 9; ++i) manager.Update();
        Check(arenasFreed == freeBefore && texture->mpD3DTexture == native, "arena alive through ninth delay update");
        manager.Update();
        Check(arenasFreed == freeBefore + 1 && retired == retireBefore + 4,
              "tenth update retires four native images and actual arena");
    }
    Check(manager.mResourceAllocatorResource.m_baseResources[0] == nullptr, "destroy clears actual arena resource");
}
int main()
{
    using Payload = rw::movie::VideoRenderable;
    alignas(Payload) u8 payloadStorage[sizeof(Payload)];
    std::memset(payloadStorage, 0xA5, sizeof(payloadStorage));
    Payload* payload = new (payloadStorage) Payload;
    Check(payload->CheckValidity() && payload->GetFormat() == Payload::VIDEOFORMAT_MAXNUM,
          "real payload magic/invalid-format constructor");
    Check(!payload->IsReadyToRender() && payload->GetData(0) == nullptr, "payload begins unpublished");
    Check(payload->mStride[0] == 0xA5A5A5A5u && payload->mStride[2] == 0xA5A5A5A5u,
          "constructor preserves ARTIST's untouched stride words");
    u32 sizes[3] = {};
    Payload::GetDataBufSizes(2, 3, Payload::VIDEOFORMAT_ARGB32, 1, sizes);
    Check(sizes[0] == 24 && sizes[1] == 0 && sizes[2] == 0, "static packed payload size ABI");
    payload->~Payload();
    u32 magic;
    std::memcpy(&magic, payloadStorage, sizeof(magic));
    Check(magic == 0, "payload destructor clears magic");
    Scenario(false, false); Scenario(true, false);
    Scenario(false, true); Scenario(true, true);
    Check(assertions == 0, "production assertions remained satisfied");
    std::printf("MovieTextureOwnership: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
