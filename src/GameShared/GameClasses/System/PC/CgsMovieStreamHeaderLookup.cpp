#include "GameShared/GameClasses/System/PC/CgsMovieStreamHeaderLookup.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cctype>
#include <algorithm>
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
    // ---- container readers (platform-2 X360 and native platform-4) ----------
    inline u32 Be32(const u8* p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | u32(p[3]); }
    inline u16 Be16(const u8* p) { return u16((u16(p[0]) << 8) | u16(p[1])); }
    inline u16 Le16(const u8* p) { return u16(u16(p[0]) | (u16(p[1]) << 8)); }
    inline u32 Le32(const u8* p) { return u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24); }
    inline u64 Le64(const u8* p) { return u64(Le32(p)) | (u64(Le32(p + 4)) << 32); }

    struct WaveSpec
    {
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

    // The engine-facing StreamsRegistry is a platform-4 native-x64 bundle after
    // Phase F.  StreamHeaders remains the original platform-2 SNR container, so
    // keep its BE reader above and use this LE counterpart only for Registry.
    bool LoadResourceLE(const std::vector<u8>& d, u32 luEntryBase, std::vector<u8>& lrOut)
    {
        if (d.size() < 0x30 || luEntryBase + 0x40 > d.size())
            return false;
        const u32 luData0 = Le32(&d[0x18]);
        const u32 luUnc   = Le32(&d[luEntryBase + 0x10]) & 0x0FFFFFFF;
        const u32 luDisk  = Le32(&d[luEntryBase + 0x1C]) & 0x0FFFFFFF;
        const u32 luOff   = Le32(&d[luEntryBase + 0x28]);
        if (luData0 + luOff + luDisk > d.size() || luUnc == 0)
            return false;
        const u8* lpSrc = &d[luData0 + luOff];
        if ((Le32(&d[0x24]) & 1) != 0)
        {
            lrOut.resize(luUnc);
            uLongf lDst = luUnc;
            if (uncompress(lrOut.data(), &lDst, lpSrc, luDisk) != Z_OK || lDst != luUnc)
                return false;
            return true;
        }
        if (luDisk != luUnc)
            return false;
        lrOut.assign(lpSrc, lpSrc + luDisk);
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
        const bool lbNative = Le32(&hdr[8]) == 4;
        const bool lbX360 = Be32(&hdr[8]) == 2;
        if (!lbNative && !lbX360)
        {
            AUDIO_LOG << "[StreamHeaders] STREAMHEADERS.bundle has unsupported platform -- header lookups disabled\n";
            return;
        }
        const u32 luHN = lbNative ? Le32(&hdr[0x10]) : Be32(&hdr[0x10]);
        const u32 luHE = lbNative ? Le32(&hdr[0x14]) : Be32(&hdr[0x14]);
        g_pHeaders->reserve(luHN);
        for (u32 e = 0; e < luHN; ++e)
        {
            const u32 b = luHE + 0x40 * e;
            if (b + 0x40 > hdr.size()) break;
            HeaderRes lRes;
            lRes.muId = lbNative ? Le32(&hdr[b]) : Be32(&hdr[b + 4]);
            const bool lbLoaded = lbNative ? LoadResourceLE(hdr, b, lRes.mData)
                                           : LoadResource(hdr, b, lRes.mData);
            if (lbLoaded && lRes.mData.size() >= 0x18)
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
        if (Le32(&regBundle[8]) != 4 ||
            !LoadResourceLE(regBundle, Le32(&regBundle[0x14]), reg))
        {
            AUDIO_LOG << "[StreamHeaders] native-x64 registry payload load FAILED\n";
            return;
        }

        // Walk the native-x64 Registry image produced by engine_transcode.py:
        // 48-byte header, cap u64 entity offsets, then 8-aligned ContentSpecs.
        // A ContentSpec is {Name, type Name, ContentType* tag, u16 pathLength,
        // u8 loadMethod, u8 loadTime, char path[]}.  This is the same graph the
        // live RegistryResourceType fixes and RootSoundModule merges.
        if (reg.size() < 48)
            return;
        const u32 luCount = Le32(&reg[0]);
        const u32 luCapacity = Le32(&reg[4]);
        const u64 luDataEnd = Le64(&reg[16]);
        const size_t luSlotEnd = 48u + size_t(luCapacity) * 8u;
        if (luCount > luCapacity || luSlotEnd > reg.size() || luDataEnd > reg.size())
            return;
        std::vector<size_t> lRecords;
        lRecords.reserve(luCount);
        for (u32 i = 0; i < luCapacity; ++i)
        {
            const u64 luOffset = Le64(&reg[48u + size_t(i) * 8u]);
            if (luOffset != 0 && luOffset >= luSlotEnd && luOffset < luDataEnd)
                lRecords.push_back(size_t(luOffset));
        }
        std::sort(lRecords.begin(), lRecords.end());
        lRecords.erase(std::unique(lRecords.begin(), lRecords.end()), lRecords.end());
        if (lRecords.size() != luCount)
            return;
        for (size_t pos : lRecords)
        {
            if (pos + 20 > luDataEnd)
                return;
            const u32 luType = Le32(&reg[pos + 4]);
            const u16 luPathLength = Le16(&reg[pos + 16]);
            if (pos + 20u + luPathLength >= luDataEnd || reg[pos + 20u + luPathLength] != 0)
                return;
            const char* lpacPath = reinterpret_cast<const char*>(&reg[pos + 20]);
            if (luType == 0x511A448Bu)
            {
                const char* lpacBar = std::strchr(lpacPath, '|');
                if (std::strncmp(lpacPath, "gamedb://", 9) == 0 && lpacBar != nullptr &&
                    lpacBar < lpacPath + luPathLength)
                {
                    WaveSpec lSpec;
                    // headerId = crc32 of the LOWERCASED url (CgsResource::ID::HashString)
                    std::string lUrl(lpacPath, lpacBar);
                    for (char& c : lUrl) c = char(std::tolower(u8(c)));
                    lSpec.muHeaderId = u32(crc32(0, reinterpret_cast<const Bytef*>(lUrl.data()),
                                                 uInt(lUrl.size())));
                    lSpec.mSnsFile = lpacBar + 1;
                    g_pSpecs->push_back(std::move(lSpec));
                }
            }
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

bool MovieStreamHeaderLookup::ResolveBySnsName(const char* lpacSnsFile,
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
