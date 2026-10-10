#pragma once

#include <d3d9.h>
#include <array>
#include <cstdlib>
#include <mutex>

// FLAG PC-platform leaf: reuse retired D3D9 SYSTEMMEM upload storage. GPU
// texture identity and lifetime are unchanged. A live Shadow owns its staging
// texture exclusively; only its final release can return storage to this cache.
namespace renderengine::TextureUploadPC
{
    struct StagingKey
    {
        IDirect3DDevice9* mpDevice = nullptr; // each cached texture retains its device
        D3DRESOURCETYPE meType = D3DRTYPE_TEXTURE;
        D3DFORMAT meFormat = D3DFMT_UNKNOWN;
        UINT muWidth = 0, muHeight = 0, muDepth = 1, muLevels = 0;
        bool operator==(const StagingKey& lrOther) const
        {
            return mpDevice == lrOther.mpDevice && meType == lrOther.meType
                && meFormat == lrOther.meFormat && muWidth == lrOther.muWidth
                && muHeight == lrOther.muHeight && muDepth == lrOther.muDepth
                && muLevels == lrOther.muLevels;
        }
    };

    class StagingCache
    {
        struct Entry { StagingKey mKey; IDirect3DBaseTexture9* mpTexture; size_t muBytes; };
        static constexpr size_t KU_CAPACITY = 128, KU_BYTE_LIMIT = 16 * 1024 * 1024;
        std::array<Entry, KU_CAPACITY> maEntries{};
        size_t muCount = 0, muBytes = 0;
        std::mutex mMutex;
    public:
        static bool Enabled()
        {
            static const bool sbEnabled = [] {
                const char* lpcValue = std::getenv("BRN_TEXTURE_STAGING_CACHE");
                return !lpcValue || lpcValue[0] != '0';
            }();
            return sbEnabled;
        }
        // Bound estimated padded pixel storage as well as object count. Driver
        // metadata is implementation-defined and is not claimed by this budget.
        static size_t StorageBytes(const StagingKey& lrKey)
        {
            unsigned luUnitBytes = 0, luBlock = 1;
            switch (lrKey.meFormat)
            {
            case D3DFMT_DXT1: luUnitBytes = 8; luBlock = 4; break;
            case D3DFMT_DXT2: case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5:
                luUnitBytes = 16; luBlock = 4; break;
            case D3DFMT_A8: case D3DFMT_L8: case D3DFMT_A4L4: luUnitBytes = 1; break;
            case D3DFMT_R5G6B5: case D3DFMT_A1R5G5B5: case D3DFMT_X1R5G5B5:
            case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4: case D3DFMT_A8L8:
            case D3DFMT_V8U8: case D3DFMT_L16: case D3DFMT_R16F: luUnitBytes = 2; break;
            case D3DFMT_R8G8B8: luUnitBytes = 3; break;
            case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A8B8G8R8:
            case D3DFMT_X8B8G8R8: case D3DFMT_A2R10G10B10: case D3DFMT_A2B10G10R10:
            case D3DFMT_G16R16: case D3DFMT_R32F: case D3DFMT_G16R16F: luUnitBytes = 4; break;
            case D3DFMT_A16B16G16R16: case D3DFMT_A16B16G16R16F: case D3DFMT_G32R32F:
                luUnitBytes = 8; break;
            case D3DFMT_A32B32G32R32F: luUnitBytes = 16; break;
            default: return 0; // unknown storage size: release normally
            }
            if (!lrKey.muWidth || !lrKey.muHeight || !lrKey.muDepth
                || !lrKey.muLevels || lrKey.muLevels > 32) return 0;
            unsigned long long luTotal = 0, luW = lrKey.muWidth, luH = lrKey.muHeight;
            unsigned long long luD = lrKey.muDepth;
            for (UINT lu = 0; lu < lrKey.muLevels; ++lu)
            {
                const auto luPitch = ((((luW + luBlock - 1) / luBlock) * luUnitBytes) + 255) & ~255ull;
                const auto luRows = (luH + luBlock - 1) / luBlock;
                if (luPitch > KU_BYTE_LIMIT || luRows > KU_BYTE_LIMIT / luPitch) return 0;
                const auto luSlice = luPitch * luRows;
                if (luD > KU_BYTE_LIMIT / luSlice) return 0;
                luTotal += luSlice * luD;
                if (luTotal > KU_BYTE_LIMIT) return 0;
                if (luW > 1) luW >>= 1;
                if (luH > 1) luH >>= 1;
                if (lrKey.meType == D3DRTYPE_VOLUMETEXTURE && luD > 1) luD >>= 1;
            }
            return static_cast<size_t>(luTotal);
        }
        IDirect3DBaseTexture9* Take(const StagingKey& lrKey)
        {
            if (!Enabled()) return nullptr;
            std::lock_guard<std::mutex> lGuard(mMutex);
            for (size_t lu = muCount; lu != 0; --lu)
            {
                if (!(maEntries[lu - 1].mKey == lrKey)) continue;
                const Entry lEntry = maEntries[lu - 1];
                muBytes -= lEntry.muBytes;
                for (size_t li = lu; li < muCount; ++li) maEntries[li - 1] = maEntries[li];
                --muCount;
                return lEntry.mpTexture;
            }
            return nullptr;
        }
        // Consumes the caller's reference. Never call D3D while holding the
        // cache mutex: a final COM release can run inside the device's lock.
        void Retire(IDirect3DBaseTexture9* lpTexture, const StagingKey& lrKey)
        {
            const size_t luBytes = Enabled() ? StorageBytes(lrKey) : 0;
            if (!luBytes) { lpTexture->Release(); return; }
            std::array<IDirect3DBaseTexture9*, KU_CAPACITY> laReleased{};
            size_t luReleased = 0;
            {
                std::lock_guard<std::mutex> lGuard(mMutex);
                while (muCount && (muCount == KU_CAPACITY || muBytes + luBytes > KU_BYTE_LIMIT))
                {
                    laReleased[luReleased++] = maEntries[0].mpTexture;
                    muBytes -= maEntries[0].muBytes;
                    for (size_t lu = 1; lu < muCount; ++lu) maEntries[lu - 1] = maEntries[lu];
                    --muCount;
                }
                maEntries[muCount++] = Entry{lrKey, lpTexture, luBytes};
                muBytes += luBytes;
            }
            for (size_t lu = 0; lu < luReleased; ++lu) laReleased[lu]->Release();
        }
        void Clear()
        {
            std::array<IDirect3DBaseTexture9*, KU_CAPACITY> laReleased{};
            size_t luReleased = 0;
            {
                std::lock_guard<std::mutex> lGuard(mMutex);
                for (size_t lu = 0; lu < muCount; ++lu) laReleased[luReleased++] = maEntries[lu].mpTexture;
                muCount = muBytes = 0;
            }
            for (size_t lu = 0; lu < luReleased; ++lu) laReleased[lu]->Release();
        }
        ~StagingCache() { Clear(); }
    };
    inline StagingCache gStagingCache;
}
