#include "GameShared/GameClasses/System/PC/CgsStreamHeadersPC.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"   // CgsSound::Playback::Name::MakeHash

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <zlib.h>   // vendor/zlib (the console exe embeds zlib for exactly these bundles)

#define AUDIO_LOG if (CgsDev::Log::gpDebugPrint) (*CgsDev::Log::gpDebugPrint)

namespace CgsSystem
{
namespace
{
    // ---- big-endian readers (the staged bundles are the X360 originals) -----
    inline u32 Be32(const u8* p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | u32(p[3]); }
    inline u16 Be16(const u8* p) { return u16((u16(p[0]) << 8) | u16(p[1])); }

    struct WaveSpec
    {
        u32         muNameHash;    // interned ContentSpec name (Playback::Name::MakeHash)
        u32         muHeaderId;    // crc32(lowercase(gamedb url)) == the StreamHeaders id
        std::string mSnsFile;      // path zone 1 (the .SNS under SOUND\STREAMS\)
    };

    struct HeaderRes
    {
        u32             muId;
        std::vector<u8> mData;     // decompressed GenericRwacWaveContent (SNR) bytes
    };

    bool                    g_bInitTried = false;
    bool                    g_bReady     = false;
    std::vector<WaveSpec>*  g_pSpecs     = nullptr;
    std::vector<HeaderRes>* g_pHeaders   = nullptr;

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

    // Decompress-or-copy one bnd2 mem0 resource (per-resource zlib when the
    // container's compress flag is set -- the X360 sound bundles are).
    bool LoadResource(const std::vector<u8>& d, u32 luEntryBase, std::vector<u8>& lrOut)
    {
        const u32 luData0 = Be32(&d[0x18]);
        const u32 luUnc   = Be32(&d[luEntryBase + 0x10]) & 0x0FFFFFFF;
        const u32 luDisk  = Be32(&d[luEntryBase + 0x1C]) & 0x0FFFFFFF;
        const u32 luOff   = Be32(&d[luEntryBase + 0x28]);
        if (luData0 + luOff + luDisk > d.size() || luUnc == 0)
            return false;
        const u8* lpSrc = &d[luData0 + luOff];
        if (luDisk >= 2 && lpSrc[0] == 0x78)   // zlib stream
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

    void Init()
    {
        if (g_bInitTried) return;
        g_bInitTried = true;
        g_pSpecs   = new std::vector<WaveSpec>();
        g_pHeaders = new std::vector<HeaderRes>();

        // ---- StreamHeaders.bundle: every entry is one SNR resource keyed by id ----
        std::vector<u8> hdr;
        if (!ReadFileWhole("SOUND\\STREAMS\\STREAMHEADERS.bundle", hdr) || hdr.size() < 0x40 ||
            std::memcmp(hdr.data(), "bnd2", 4) != 0)
        {
            AUDIO_LOG << "[StreamHeaders] STREAMHEADERS.bundle missing/invalid -- header lookups disabled\n";
            return;
        }
        const u32 luHN = Be32(&hdr[0x10]);
        const u32 luHE = Be32(&hdr[0x14]);
        g_pHeaders->reserve(luHN);
        for (u32 e = 0; e < luHN; ++e)
        {
            const u32 b = luHE + 0x40 * e;
            if (b + 0x40 > hdr.size()) break;
            HeaderRes lRes;
            lRes.muId = Be32(&hdr[b + 4]);       // low u32 of the big-endian u64 id
            if (LoadResource(hdr, b, lRes.mData) && lRes.mData.size() >= 0x18)
                g_pHeaders->push_back(std::move(lRes));
        }

        // ---- StreamsRegistry.bundle: the ContentSpec table (one zlib'd payload) ----
        std::vector<u8> regBundle;
        if (!ReadFileWhole("SOUND\\STREAMS\\STREAMSREGISTRY.BUNDLE", regBundle) ||
            regBundle.size() < 0x70 || std::memcmp(regBundle.data(), "bnd2", 4) != 0)
        {
            AUDIO_LOG << "[StreamHeaders] STREAMSREGISTRY.BUNDLE missing/invalid -- name lookups disabled\n";
            return;
        }
        std::vector<u8> reg;
        if (!LoadResource(regBundle, Be32(&regBundle[0x14]), reg))
        {
            AUDIO_LOG << "[StreamHeaders] registry payload decompress FAILED\n";
            return;
        }

        // Scan the wave ContentSpec records: {u32 nameHash, u32 typeHash(0x511A448B
        // == wave), u32 classRef, u32 misc, char path[] NUL} 4-aligned, path =
        // "<gamedb url>|<SNS file>". (FLAG: pragmatic scan; the full serialised
        // Registry parse is the CgsSound::Playback::Registry recon follow-on.)
        size_t pos = 0;
        // find the first record: 12 bytes before the first "gamedb"
        for (size_t i = 0; i + 6 < reg.size(); ++i)
            if (std::memcmp(&reg[i], "gamedb", 6) == 0) { pos = (i >= 16) ? i - 16 : 0; break; }
        while (pos + 17 <= reg.size())
        {
            const u32 luType = Be32(&reg[pos + 4]);
            size_t z = pos + 16;
            while (z < reg.size() && reg[z] != 0) ++z;
            if (z >= reg.size()) break;
            const char* lpacPath = reinterpret_cast<const char*>(&reg[pos + 16]);
            if (std::strncmp(lpacPath, "gamedb://", 9) != 0)
                break;   // end of the spec run
            if (luType == 0x511A448Bu)
            {
                const char* lpacBar = std::strchr(lpacPath, '|');
                if (lpacBar != nullptr)
                {
                    WaveSpec lSpec;
                    lSpec.muNameHash = Be32(&reg[pos]);
                    // headerId = crc32 of the LOWERCASED url (CgsResource::ID::HashString)
                    std::string lUrl(lpacPath, lpacBar);
                    for (char& c : lUrl) c = char(std::tolower(u8(c)));
                    lSpec.muHeaderId = u32(crc32(0, reinterpret_cast<const Bytef*>(lUrl.data()),
                                                 uInt(lUrl.size())));
                    lSpec.mSnsFile = lpacBar + 1;
                    g_pSpecs->push_back(std::move(lSpec));
                }
            }
            pos = (z + 1 + 3) & ~size_t(3);
        }

        g_bReady = !g_pSpecs->empty() && !g_pHeaders->empty();
        char lac[160];
        std::snprintf(lac, sizeof(lac),
                      "[StreamHeaders] registry: %u wave specs, headers: %u SNR resources (%s)\n",
                      unsigned(g_pSpecs->size()), unsigned(g_pHeaders->size()),
                      g_bReady ? "READY" : "DISABLED");
        AUDIO_LOG << lac;
    }

    const HeaderRes* FindHeader(u32 luId)
    {
        for (const HeaderRes& r : *g_pHeaders)
            if (r.muId == luId)
                return &r;
        return nullptr;
    }

    bool FillFromSpec(const WaveSpec& lrSpec, const u8** lppSnr, u32* lpuSnrLen,
                      char* lpacSnsFile, u32 luSnsCap)
    {
        const HeaderRes* lpRes = FindHeader(lrSpec.muHeaderId);
        if (lpRes == nullptr)
            return false;
        if (lppSnr)    *lppSnr    = lpRes->mData.data();
        if (lpuSnrLen) *lpuSnrLen = u32(lpRes->mData.size());
        if (lpacSnsFile && luSnsCap)
        {
            std::strncpy(lpacSnsFile, lrSpec.mSnsFile.c_str(), luSnsCap - 1);
            lpacSnsFile[luSnsCap - 1] = 0;
        }
        return true;
    }
}   // anonymous namespace

// ⭐ 2026-08-16 (boot audit F-P5-11/F7). See the header: this is the same one-shot Init the
// resolvers run lazily, exposed so the boot can run it AT THE CONSOLE'S POINT --
// RootSoundModule::Prepare's REGISTRY_LOAD stage, loading-screen stage 4. On the console
// that stage is RegistryLoad @0x826EBA08 streaming the CSIS/AEMS registries through the
// playback module, and StreamingStateManager::Prepare @0x826EE680 loading
// "sound\streams\StreamHeaders.bundle" beside it; both are blocked here on the rw::audio
// engine. What is NOT blocked is the TIMING of reading the same two files, so that is what
// this restores. Idempotent.
void StreamHeadersPC::Preload()
{
    Init();
}

bool StreamHeadersPC::ResolveBySpecName(const char* lpacSpecName,
                                        const u8** lppSnr, u32* lpuSnrLen,
                                        char* lpacSnsFile, u32 luSnsCap)
{
    Init();
    if (!g_bReady || lpacSpecName == nullptr)
        return false;
    const u32 luName = u32(CgsSound::Playback::Name::MakeHash(lpacSpecName));
    for (const WaveSpec& s : *g_pSpecs)
        if (s.muNameHash == luName)
            return FillFromSpec(s, lppSnr, lpuSnrLen, lpacSnsFile, luSnsCap);
    return false;
}

bool StreamHeadersPC::ResolveBySnsName(const char* lpacSnsFile,
                                       const u8** lppSnr, u32* lpuSnrLen)
{
    Init();
    if (!g_bReady || lpacSnsFile == nullptr)
        return false;
    for (const WaveSpec& s : *g_pSpecs)
        if (_stricmp(s.mSnsFile.c_str(), lpacSnsFile) == 0)
            return FillFromSpec(s, lppSnr, lpuSnrLen, nullptr, 0);
    return false;
}

}   // namespace CgsSystem
