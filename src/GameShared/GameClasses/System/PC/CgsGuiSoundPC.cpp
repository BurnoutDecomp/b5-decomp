#include "GameShared/GameClasses/System/PC/CgsGuiSoundPC.h"

#include "GameShared/GameClasses/System/PC/CgsAudioOutputPC.h"   // overlay fill
#include "GameShared/GameClasses/System/PC/CgsMovieAudioPC.h"    // SnrSampleDecodePC
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"// CgsResource::ID::HashString
#include "GameShared/GameClasses/Development/Log/CgsLog.h"       // CgsDev::Log

#include <zlib.h>    // vendor/zlib (the bnd2 resource payloads are zlib streams)
#include <cstdio>
#include <cstring>
#include <string.h>  // _stricmp (MSVC canonical)
#include <vector>

#define GUISND_LOG(msg) do { char lac_[192]; std::snprintf(lac_, sizeof(lac_), "%s\n", msg); CgsDev::Log::WriteToLog(lac_); } while (0)

namespace CgsSystem
{
namespace
{
    inline u32 Be32(const u8* p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | u32(p[3]); }
    inline u16 Be16(const u8* p) { return u16((u16(p[0]) << 8) | u16(p[1])); }
    inline u64 Be64(const u8* p) { return (u64(Be32(p)) << 32) | u64(Be32(p + 4)); }

    bool ReadFileWhole(const char* lpacPath, std::vector<u8>& lrOut)
    {
        std::FILE* lpF = std::fopen(lpacPath, "rb");
        if (!lpF) return false;
        std::fseek(lpF, 0, SEEK_END);
        long lSize = std::ftell(lpF);
        std::fseek(lpF, 0, SEEK_SET);
        if (lSize <= 0) { std::fclose(lpF); return false; }
        lrOut.resize(size_t(lSize));
        const bool lbOk = std::fread(lrOut.data(), 1, lrOut.size(), lpF) == lrOut.size();
        std::fclose(lpF);
        return lbOk;
    }

    // Decompress-or-copy one bnd2 mem0 resource (the same idiom as StreamHeadersPC:
    // per-resource zlib when the container's compress flag is set).
    bool LoadResource(const std::vector<u8>& d, u32 luEntryBase, std::vector<u8>& lrOut)
    {
        const u32 luData0 = Be32(&d[0x18]);
        const u32 luUnc   = Be32(&d[luEntryBase + 0x10]) & 0x0FFFFFFF;
        const u32 luDisk  = Be32(&d[luEntryBase + 0x1C]) & 0x0FFFFFFF;
        const u32 luOff   = Be32(&d[luEntryBase + 0x28]);
        if (luData0 + luOff + luDisk > d.size() || luUnc == 0)
            return false;
        const u8* lpSrc = &d[luData0 + luOff];
        if (luDisk >= 2 && lpSrc[0] == 0x78)
        {
            lrOut.resize(luUnc);
            uLongf lDst = luUnc;
            if (uncompress(lrOut.data(), &lDst, lpSrc, luDisk) != Z_OK || lDst != luUnc)
                return false;
            return true;
        }
        lrOut.assign(lpSrc, lpSrc + (luDisk < luUnc ? luDisk : luUnc));
        return true;
    }

    // ---- the presentation action list (the Attrib::Gen::presentationactionlist
    //      parallel arrays; one row per trigger mapping) --------------------------
    const s32 KI_NUM_ROWS = 256;   // the generated class's array length
    struct ActionRow
    {
        u64 muScreenId;        // HashString(screen/apt name); 0 == any screen
        u64 muStringId;        // HashString(trigger/action string)
        u32 muContentSpec;     // 0 == the default presentation bank
        u32 muAction;          // the trigger's channel word
        u16 mu16Splice;        // splice index (0xFFFE stream / 0xFFFF wave sentinels)
        u8  mu8Mixer;
        u8  mu8Choke;
        u8  mu8Behaviour;
    };
    std::vector<ActionRow>* g_pRows = nullptr;

    // ---- the presentation Splicer bank ------------------------------------------
    struct SampleRef
    {
        u16 mu16Sample;
        f32 mfVolume;      // ref volume (multiplied with the splice volume)
        f32 mfPitch;       // playback-rate multiplier
        f32 mfDuration;    // seconds; clamps the played extent
        f32 mfFadeOut;     // seconds; linear ramp at the tail of the played extent
    };
    struct Splice
    {
        f32 mfVolume;
        std::vector<SampleRef> mRefs;
    };
    std::vector<Splice>*            g_pSplices    = nullptr;
    std::vector<std::vector<u8>>*   g_pSamples    = nullptr;   // raw SNR sample images
    std::vector<std::vector<s16>>*  g_pSamplePcm  = nullptr;   // decode cache (per sample)
    std::vector<int>*               g_pSampleRate = nullptr;
    std::vector<int>*               g_pSampleCh   = nullptr;

    bool g_bInitTried = false;
    bool g_bReady     = false;

    // ---- the one-shot voice pool (mirrors the effect's small aging pool; sized
    //      for the multi-ref splices -- the accept sound alone layers 3 refs) -----
    const int KI_NUM_VOICES = 8;
    struct OneShot
    {
        volatile bool  mbActive;
        const s16*     mpPcm;       // interleaved stereo
        s64            mnFrames;    // played extent (duration-clamped)
        s64            mnFadeFrom;  // frame where the linear fade-out begins (== mnFrames: none)
        s64            mnCursor;    // fixed-point 16.16 source frame cursor (64-bit:
        s64            mnStep;      //   a 32-bit 16.16 cursor overflows at ~0.74 s)
        f32            mfVolume;
    };
    OneShot g_aVoices[KI_NUM_VOICES] = {};

    // Parse SOUND\BURNOUTGLOBALDATA.BIN -> the presentationactionlist rows.
    bool LoadActionList()
    {
        std::vector<u8> bundle;
        if (!ReadFileWhole("SOUND\\BURNOUTGLOBALDATA.BIN", bundle) || bundle.size() < 0x70 ||
            std::memcmp(bundle.data(), "bnd2", 4) != 0)
        {
            GUISND_LOG("[GuiSound] SOUND\\BURNOUTGLOBALDATA.BIN missing/invalid");
            return false;
        }
        std::vector<u8> vault;
        if (!LoadResource(bundle, Be32(&bundle[0x14]), vault) || vault.size() < 0x20)
        {
            GUISND_LOG("[GuiSound] sound vault payload decompress FAILED");
            return false;
        }

        // Vault image: {u32 vltOff, u32 vltSize, u32 binOff, u32 binSize}; the .bin part
        // opens with the 'StrE' string chunk, then the raw attribute data area.
        const u32 luBinOff  = Be32(&vault[8]);
        const u32 luBinSize = Be32(&vault[12]);
        if (luBinOff + luBinSize > vault.size() || luBinSize < 0x40)
            return false;

        // Locate the presentationactionlist parallel-array block inside the data area
        // by its attested header chain: each array is prefixed by the 8-byte
        // Attrib::Private header {0x01000100, elemSize<<16}, and the class's DWARF
        // layout fixes the sequence s64[256] s64[256] u32[256] u32[256] u16[256]
        // u8[256] u8[256] u8[256] (ScreenIds, CustomStringIds, ContentSpecNames,
        // Actions, SpliceIndicies, MixerOutputs, ChokeGroups, Behaviours).
        static const u32 KAU_ELEM[8] = { 8, 8, 4, 4, 2, 1, 1, 1 };
        const u8* lpBin = &vault[luBinOff];
        u32 luBlock = 0;
        bool lbFound = false;
        for (u32 lu = 0; lu + 8 <= luBinSize && !lbFound; lu += 4)
        {
            u32 luProbe = lu;
            bool lbOk = true;
            for (int i = 0; i < 8 && lbOk; ++i)
            {
                if (luProbe + 8 > luBinSize ||
                    Be32(&lpBin[luProbe]) != 0x01000100u ||
                    Be32(&lpBin[luProbe + 4]) != (KAU_ELEM[i] << 16))
                    lbOk = false;
                luProbe += 8 + KI_NUM_ROWS * KAU_ELEM[i];
            }
            if (lbOk) { luBlock = lu; lbFound = true; }
        }
        if (!lbFound)
        {
            GUISND_LOG("[GuiSound] presentationactionlist array chain not found in the vault");
            return false;
        }

        const u8* lpScreen = lpBin + luBlock + 8;
        const u8* lpString = lpScreen + KI_NUM_ROWS * 8 + 8;
        const u8* lpSpec   = lpString + KI_NUM_ROWS * 8 + 8;
        const u8* lpAction = lpSpec   + KI_NUM_ROWS * 4 + 8;
        const u8* lpSplice = lpAction + KI_NUM_ROWS * 4 + 8;
        const u8* lpMixer  = lpSplice + KI_NUM_ROWS * 2 + 8;
        const u8* lpChoke  = lpMixer  + KI_NUM_ROWS * 1 + 8;
        const u8* lpBehav  = lpChoke  + KI_NUM_ROWS * 1 + 8;

        g_pRows = new std::vector<ActionRow>();
        g_pRows->reserve(KI_NUM_ROWS);
        for (int i = 0; i < KI_NUM_ROWS; ++i)
        {
            ActionRow lRow;
            lRow.muScreenId    = Be64(lpScreen + i * 8);
            lRow.muStringId    = Be64(lpString + i * 8);
            lRow.muContentSpec = Be32(lpSpec + i * 4);
            lRow.muAction      = Be32(lpAction + i * 4);
            lRow.mu16Splice    = Be16(lpSplice + i * 2);
            lRow.mu8Mixer      = lpMixer[i];
            lRow.mu8Choke      = lpChoke[i];
            lRow.mu8Behaviour  = lpBehav[i];
            if (lRow.muStringId != 0)
                g_pRows->push_back(lRow);
        }
        char lac[96];
        std::snprintf(lac, sizeof(lac), "[GuiSound] presentation action list: %d mappings",
                      int(g_pRows->size()));
        GUISND_LOG(lac);
        return !g_pRows->empty();
    }

    // Parse SOUND\SPLICER\PRESENTATIONASSET.BUNDLE -> splice TOC + SNR sample images.
    bool LoadSpliceBank()
    {
        std::vector<u8> bundle;
        if (!ReadFileWhole("SOUND\\SPLICER\\PRESENTATIONASSET.BUNDLE", bundle) || bundle.size() < 0x70 ||
            std::memcmp(bundle.data(), "bnd2", 4) != 0)
        {
            GUISND_LOG("[GuiSound] SOUND\\SPLICER\\PRESENTATIONASSET.BUNDLE missing/invalid");
            return false;
        }
        std::vector<u8> res;
        if (!LoadResource(bundle, Be32(&bundle[0x14]), res) || res.size() < 0x20)
        {
            GUISND_LOG("[GuiSound] splice bank payload decompress FAILED");
            return false;
        }

        // Splicer resource (burnout.wiki Splicer; the volatility tool implements the
        // same layout): BinaryResource {u32 dataSize, u32 dataOffset}; at dataOffset
        // {s32 version==1, s32 sizedata, s32 numSplices}; SpliceHeader[numSplices]
        // (0x18: u32 nameHash, u16 index, s8 type, u8 refCount, f32 vol, f32 rndPitch,
        // f32 rndVol, u32 pad); SampleRef[totalRefs] (0x2C, u16 sampleIndex first);
        // then @dataOffset+0xC+sizedata: {s32 numSamples, s32 ptrs[], blobs}.
        const u32 luDataOff = Be32(&res[4]);
        if (luDataOff + 12 > res.size() || Be32(&res[luDataOff]) != 1)
            return false;
        const u32 luSizeData = Be32(&res[luDataOff + 4]);
        const s32 liSplices  = s32(Be32(&res[luDataOff + 8]));
        if (liSplices <= 0 || liSplices > 4096)
            return false;
        const u32 luHdrs = luDataOff + 12;
        const u32 luRefs = luHdrs + u32(liSplices) * 0x18;
        const u32 luTab  = luDataOff + 12 + luSizeData;
        if (luTab + 4 > res.size())
            return false;

        g_pSplices = new std::vector<Splice>();
        g_pSplices->reserve(liSplices);
        u32 luRefCursor = luRefs;
        union { u32 u; f32 f; } v;
        for (s32 i = 0; i < liSplices; ++i)
        {
            const u8* h = &res[luHdrs + u32(i) * 0x18];
            Splice lS;
            const u8 lu8Refs = h[7];
            v.u = Be32(h + 8);
            lS.mfVolume = v.f;
            for (u8 r = 0; r < lu8Refs && luRefCursor + 0x2C <= res.size(); ++r)
            {
                const u8* rf = &res[luRefCursor];
                SampleRef lRef;
                lRef.mu16Sample = Be16(rf);
                v.u = Be32(rf + 4);  lRef.mfVolume   = v.f;
                v.u = Be32(rf + 8);  lRef.mfPitch    = v.f;
                v.u = Be32(rf + 20); lRef.mfDuration = v.f;
                v.u = Be32(rf + 28); lRef.mfFadeOut  = v.f;
                lS.mRefs.push_back(lRef);
                luRefCursor += 0x2C;
            }
            g_pSplices->push_back(lS);
        }

        const s32 liSamples = s32(Be32(&res[luTab]));
        if (liSamples <= 0 || liSamples > 4096)
            return false;
        const u32 luPtrs = luTab + 4;
        const u32 luData = luPtrs + u32(liSamples) * 4;
        g_pSamples    = new std::vector<std::vector<u8>>(size_t(liSamples));
        g_pSamplePcm  = new std::vector<std::vector<s16>>(size_t(liSamples));
        g_pSampleRate = new std::vector<int>(size_t(liSamples), 0);
        g_pSampleCh   = new std::vector<int>(size_t(liSamples), 0);
        for (s32 i = 0; i < liSamples; ++i)
        {
            const u32 luStart = luData + Be32(&res[luPtrs + u32(i) * 4]);
            const u32 luEnd   = (i + 1 < liSamples) ? luData + Be32(&res[luPtrs + u32(i + 1) * 4])
                                                    : u32(res.size());
            if (luStart >= luEnd || luEnd > res.size())
                continue;
            (*g_pSamples)[size_t(i)].assign(res.begin() + luStart, res.begin() + luEnd);
        }
        char lac[96];
        std::snprintf(lac, sizeof(lac), "[GuiSound] splice bank: %d splices, %d samples",
                      int(liSplices), int(liSamples));
        GUISND_LOG(lac);
        return true;
    }

    // Decode-on-first-use PCM for one bank sample.
    const std::vector<s16>* SamplePcm(u16 lu16Sample, int& lrRate, int& lrChannels)
    {
        if (g_pSamples == nullptr || lu16Sample >= g_pSamples->size())
            return nullptr;
        std::vector<s16>& lrPcm = (*g_pSamplePcm)[lu16Sample];
        if (lrPcm.empty())
        {
            const std::vector<u8>& lrImg = (*g_pSamples)[lu16Sample];
            int liRate = 0, liCh = 0;
            if (lrImg.empty() ||
                !SnrSampleDecodePC(lrImg.data(), lrImg.size(), lrPcm, liRate, liCh) || lrPcm.empty())
            {
                char lac[96];
                std::snprintf(lac, sizeof(lac), "[GuiSound] sample %u decode FAILED", unsigned(lu16Sample));
                GUISND_LOG(lac);
                return nullptr;
            }
            (*g_pSampleRate)[lu16Sample] = liRate;
            (*g_pSampleCh)[lu16Sample]   = liCh;
        }
        lrRate     = (*g_pSampleRate)[lu16Sample];
        lrChannels = (*g_pSampleCh)[lu16Sample];
        return &lrPcm;
    }
}

// The overlay fill: mix every active one-shot (nearest source frame at the
// 16.16 rate step) into the output frames additively; the backend saturates.
void GuiSoundPC::FillStatic(s16* lpOut, int liFrames, void* /*lpUser*/)
{
    std::memset(lpOut, 0, size_t(liFrames) * 2 * sizeof(s16));
    for (int v = 0; v < KI_NUM_VOICES; ++v)
    {
        OneShot& lrV = g_aVoices[v];
        if (!lrV.mbActive || lrV.mpPcm == nullptr)
            continue;
        for (int i = 0; i < liFrames; ++i)
        {
            const s64 liSrc = lrV.mnCursor >> 16;
            if (liSrc >= lrV.mnFrames) { lrV.mbActive = false; break; }
            f32 lfGain = lrV.mfVolume;
            if (liSrc >= lrV.mnFadeFrom && lrV.mnFrames > lrV.mnFadeFrom)
                lfGain *= f32(lrV.mnFrames - liSrc) / f32(lrV.mnFrames - lrV.mnFadeFrom);
            const int liL = int(f32(lrV.mpPcm[liSrc * 2 + 0]) * lfGain);
            const int liR = int(f32(lrV.mpPcm[liSrc * 2 + 1]) * lfGain);
            int liMixL = int(lpOut[i * 2 + 0]) + liL;
            int liMixR = int(lpOut[i * 2 + 1]) + liR;
            if (liMixL >  32767) liMixL =  32767;
            if (liMixL < -32768) liMixL = -32768;
            if (liMixR >  32767) liMixR =  32767;
            if (liMixR < -32768) liMixR = -32768;
            lpOut[i * 2 + 0] = s16(liMixL);
            lpOut[i * 2 + 1] = s16(liMixR);
            lrV.mnCursor += lrV.mnStep;
        }
    }
}

bool GuiSoundPC::Initialise()
{
    if (g_bInitTried)
        return g_bReady;
    g_bInitTried = true;
    if (LoadActionList() && LoadSpliceBank())
    {
        AudioOutputPC::SetOverlayFill(&GuiSoundPC::FillStatic, nullptr);
        g_bReady = true;
    }
    return g_bReady;
}

void GuiSoundPC::OnTrigger(const char* lpacTypeName, const char* lpacActionName,
                           const char* lpacAptName, s32 liChannel)
{
    if (!Initialise())
        return;

    // Console keying (the REAL GuiAudioTriggerEvent::Construct @0x824F6350 record is
    // {component[32] @+0, s32 actionEnum @+32, label[32] @+36, movie[32] @+68}; the
    // trigger-resolve @0x8269E368 hashes the LABEL -- or the COMPONENT when the label
    // is "uninitialised" -- as the string key, the MOVIE name as the screen key, and
    // matches the action ENUM):
    //  * liChannel >= 0 carries the enum directly (the 201 records);
    //  * liChannel < 0 -> parse the AS ACTION STRING against the X360 action-name
    //    table @0x82F2CFC0 {OnEnter..OnRightSweep} (underscore-insensitive: the AS
    //    fires 'ON_FOCUS' for 'OnFocus').
    s32 liActionEnum = liChannel;
    if (liActionEnum < 0 && lpacActionName != nullptr)
    {
        static const char* KAPC_ACTIONS[14] = {
            "OnEnter", "OnExit", "OnFocus", "OnLoseFocus", "OnAccept", "OnCancel",
            "OnTick", "OnChange", "OnUp", "OnDown", "OnLeft", "OnLeftSweep",
            "OnRight", "OnRightSweep"
        };
        for (int a = 0; a < 14 && liActionEnum < 0; ++a)
        {
            const char* lpS = lpacActionName;
            const char* lpT = KAPC_ACTIONS[a];
            while (*lpS == '_') ++lpS;
            bool lbSame = true;
            while (lbSame && (*lpS || *lpT))
            {
                if (*lpS == '_') { ++lpS; continue; }
                char lcS = (*lpS >= 'A' && *lpS <= 'Z') ? char(*lpS + 32) : *lpS;
                char lcT = (*lpT >= 'A' && *lpT <= 'Z') ? char(*lpT + 32) : *lpT;
                if (lcS != lcT) { lbSame = false; break; }
                ++lpS; ++lpT;
            }
            if (lbSame) liActionEnum = a;
        }
    }
    if (liActionEnum < 0)
        return;

    // String key: the LABEL unless "uninitialised"/empty, then the COMPONENT name
    // (the trigger-resolve's fallback); screen key = the movie/screen name (0 == any).
    // The taps pass lpacTypeName = string-key candidate A (label/trigger name) and
    // lpacAptName = candidate B (component name) per that rule.
    const char* lapcKeys[2] = { lpacTypeName, lpacAptName };
    const ActionRow* lpRow = nullptr;
    const char* lpacKey = "";
    for (int k = 0; k < 2 && lpRow == nullptr; ++k)
    {
        const char* lpacCand = lapcKeys[k];
        if (lpacCand == nullptr || lpacCand[0] == 0 || _stricmp(lpacCand, "uninitialised") == 0)
            continue;
        const u64 luString = u64(u32(CgsResource::ID::HashString(reinterpret_cast<const u8*>(lpacCand))));
        const ActionRow* lpAnyScreen = nullptr;
        for (const ActionRow& lrRow : *g_pRows)
        {
            if (lrRow.muStringId != luString || lrRow.muAction != u32(liActionEnum))
                continue;
            if (lrRow.muScreenId == 0) { lpAnyScreen = &lrRow; continue; }
            // Screen-scoped row: match wins over the any-screen fallback.
            lpRow = &lrRow;   // (screen hash comparison joins here when the taps carry it)
            break;
        }
        if (lpRow == nullptr)
            lpRow = lpAnyScreen;
        if (lpRow != nullptr)
            lpacKey = lpacCand;
    }
    if (lpRow == nullptr)
    {
        char lac[224];
        std::snprintf(lac, sizeof(lac),
                      "[GuiSound] no mapping (keyA '%s', action '%s', keyB '%s', enum %d)",
                      lpacTypeName ? lpacTypeName : "", lpacActionName ? lpacActionName : "",
                      lpacAptName ? lpacAptName : "", int(liActionEnum));
        GUISND_LOG(lac);
        return;
    }
    if (lpRow->mu16Splice >= 0xFFFE)
        return;   // stream/wave sentinel paths (the deferred faithful layers)
    if (g_pSplices == nullptr || lpRow->mu16Splice >= g_pSplices->size())
        return;

    const Splice& lrSplice = (*g_pSplices)[lpRow->mu16Splice];
    if (lrSplice.mRefs.empty())
        return;
    const f32 lfSpliceVol = (lrSplice.mfVolume > 0.0f && lrSplice.mfVolume <= 4.0f)
                                ? lrSplice.mfVolume : 1.0f;

    // Start ONE voice per sample ref (the authored composite: e.g. the accept sound
    // layers three refs at different volumes/pitches). Each ref applies its own
    // volume, pitch (playback-rate multiplier), duration clamp and tail fade-out.
    int liStarted = 0;
    for (const SampleRef& lrRef : lrSplice.mRefs)
    {
        if (lrRef.mu16Sample == 0xFFFF)
            continue;
        int liRate = 0, liChannels = 0;
        const std::vector<s16>* lpPcm = SamplePcm(lrRef.mu16Sample, liRate, liChannels);
        if (lpPcm == nullptr || liRate <= 0)
            continue;

        // The device may be closed when no stream owns it; open at the sample's
        // rate so the blip is still audible.
        if (!AudioOutputPC::IsOpen())
            AudioOutputPC::Open(liRate, 2, nullptr, nullptr);
        const int liDevRate = AudioOutputPC::GetOpenSampleRate();
        if (liDevRate <= 0)
            continue;

        int liSlot = 0;
        for (int v = 0; v < KI_NUM_VOICES; ++v)
            if (!g_aVoices[v].mbActive) { liSlot = v; break; }
        OneShot& lrV = g_aVoices[liSlot];
        lrV.mbActive = false;
        lrV.mpPcm    = lpPcm->data();
        s64 liFrames = s64(lpPcm->size() / 2);
        const f32 lfPitch = (lrRef.mfPitch > 0.01f && lrRef.mfPitch < 8.0f) ? lrRef.mfPitch : 1.0f;
        if (lrRef.mfDuration > 0.0f)
        {
            // Duration is authored in OUTPUT seconds; the source extent covers
            // duration * rate * pitch source frames.
            const s64 liCap = s64(f64(lrRef.mfDuration) * f64(liRate) * f64(lfPitch));
            if (liCap > 0 && liCap < liFrames) liFrames = liCap;
        }
        lrV.mnFrames  = liFrames;
        lrV.mnFadeFrom = liFrames;
        if (lrRef.mfFadeOut > 0.0f)
        {
            const s64 liFade = s64(f64(lrRef.mfFadeOut) * f64(liRate) * f64(lfPitch));
            if (liFade > 0 && liFade < liFrames) lrV.mnFadeFrom = liFrames - liFade;
        }
        lrV.mnCursor = 0;
        lrV.mnStep   = s64(f64(s64(liRate) << 16) * f64(lfPitch)) / liDevRate;
        const f32 lfRefVol = (lrRef.mfVolume > 0.0f && lrRef.mfVolume <= 4.0f) ? lrRef.mfVolume : 1.0f;
        lrV.mfVolume = lfSpliceVol * lfRefVol;
        lrV.mbActive = true;
        ++liStarted;
    }

    char lac[176];
    std::snprintf(lac, sizeof(lac), "[GuiSound] '%s' -> splice %u (%d ref voice(s), splice vol %.2f)",
                  lpacKey, unsigned(lpRow->mu16Splice), liStarted, double(lfSpliceVol));
    GUISND_LOG(lac);
}

} // namespace CgsSystem
