from pathlib import Path
import sys


here = Path(__file__).resolve().parent
root = Path(sys.argv[1]) / "src/GameShared/GameClasses/Graphics/MoviePlayer"
prefix = '#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'


def write(name, edits):
    body = (root / name).read_text(encoding="utf-8")
    for old, new in edits:
        assert body.count(old) == 1, old
        body = body.replace(old, new)
    with (here / name).open("x", encoding="utf-8") as output:
        output.write(prefix + body)


prepare = """    bool MoviePlayer::Prepare(rw::IResourceAllocator* lpAllocator, const char* lpMovieFileName,
                              const char* lpcLanguageCode)
    {"""
prepared = "            mePlayerState = E_RW_MOVIE_PLAYER_PREPARED;\n            return true;"
update = "    void MoviePlayer::Update()\n    {"
render = "                mpRwVideoRenderer->Render(liRow, &lRenderable, liIndex == 0);"
play = "    void MoviePlayer::Play()\n    {"
start_decoder = """                VP6_StartDecoder(&maVP6Decoders[liVideoIndex], lrHeader.mWidth, lrHeader.mHeight,
                                 true, CgsSystem::JobManager(), 63);"""
write("CgsMoviePlayer.cpp", (
    (prepare, prepare + """
        static s32 siLastPrepareState = -1;
        static unsigned int suPrepareCount = 0;
        if (siLastPrepareState != mePlayerState || (++suPrepareCount & 255u) == 0)
        {
            siLastPrepareState = mePlayerState;
            char lacProbe[144];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R101_VP6_PREPARE player=%08X state=%d stream=%d file=%.64s\\n",
                reinterpret_cast<u32>(this), siLastPrepareState,
                static_cast<s32>(mMovieStream.GetStatus()), lpMovieFileName ? lpMovieFileName : "");
            OutputDebugStringA(lacProbe);
        }"""),
    (prepared, """            mePlayerState = E_RW_MOVIE_PLAYER_PREPARED;
            OutputDebugStringA("R101_VP6_PREPARED\\n");
            return true;"""),
    (start_decoder, """                OutputDebugStringA("R101_VP6_START_DECODER_BEGIN\\n");
""" + start_decoder + """
                OutputDebugStringA("R101_VP6_START_DECODER_END\\n");"""),
    (update, update + """
        static unsigned int suUpdateCount = 0;
        if (mePlayerState == E_RW_MOVIE_PLAYER_PLAYING &&
            (++suUpdateCount <= 12 || (suUpdateCount & 127u) == 0))
        {
            char lacProbe[130];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R101_VP6_UPDATE frame=%d total=%d ready=%d decoding=%d\\n",
                miCurrentFrame, maVP6Headers[0].mTotalFrames, mbReadyToRender, mbDecodingFrame);
            OutputDebugStringA(lacProbe);
        }"""),
    (render, """                static unsigned int suRenderCount = 0;
                if (++suRenderCount <= 8 || (suRenderCount & 127u) == 0)
                {
                    char lacProbe[124];
                    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                        "R101_VP6_RENDER frame=%d slice=%d y=%08X\\n",
                        miCurrentFrame, liIndex, reinterpret_cast<u32>(lYUVConfig.YBuffer));
                    OutputDebugStringA(lacProbe);
                }
""" + render),
    (play, play + """
        static unsigned int suPlayCount = 0;
        if (++suPlayCount <= 8)
        {
            char lacProbe[108];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R101_VP6_PLAY state=%d frame=%d\\n",
                static_cast<s32>(mePlayerState), miCurrentFrame);
            OutputDebugStringA(lacProbe);
        }"""),
))

chunk = "            const u32 luChunkSize = ByteSwap(lBuffer.GetChunkDef(lBuffer.muCurrentChunk)->muSize);"
wait = "    void MoviePlayer::WaitForDecode()\n    {\n        if (mbDecodingFrame)\n        {"
write("CgsMoviePlayer_wBT_01.cpp", (
    ("    return sDecodeSemaphore.Post(1);", """    static volatile LONG slCallbacks = 0;
    if (InterlockedIncrement(&slCallbacks) <= 12) OutputDebugStringA("R101_VP6_DECODE_CALLBACK\\n");
    return sDecodeSemaphore.Post(1);"""),
    (chunk, chunk + """
            static unsigned int suChunkCount = 0;
            if (++suChunkCount <= 16)
            {
                char lacProbe[150];
                CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                    "R101_VP6_CHUNK id=%.4s size=%u read=%u index=%u target=%u\\n",
                    lBuffer.GetChunkId(lBuffer.muCurrentChunk), luChunkSize,
                    lBuffer.muCurrentChunkRead, lBuffer.muCurrentChunk, luNumChunks);
                OutputDebugStringA(lacProbe);
            }"""),
    (wait, wait + """
            static unsigned int suWaitCount = 0;
            const bool lbProbe = ++suWaitCount <= 8 || (suWaitCount & 127u) == 0;
            if (lbProbe) OutputDebugStringA("R101_VP6_DECODE_WAIT_BEGIN\\n");"""),
    ("            ++miCurrentFrame;", """            ++miCurrentFrame;
            if (lbProbe) OutputDebugStringA("R101_VP6_DECODE_WAIT_END\\n");"""),
))
