#include "GameSource/Resource/BrnResourceDebugComponent.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameSource/Resource/BrnGameDataModule.h"
#include "rw/rwcore_general_alloc.h"

#include <cstring>

int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour,
                  bool lbCheckScreen);

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::DebugComponent::Update           @ 0x82666F38
//   BrnResource::DebugComponent::RenderHUD        @ 0x82662008
//   BrnResource::DebugComponent::OnActivate       @ 0x826626A8
//   BrnResource::DebugComponent::InterpolateColours @ 0x82661410
//
// Per-channel linear interpolation between two packed 32-bit colours in 0xAARRGGBB byte
// order, used by the GameData debug component's RenderHUD (caller @0x82661410's xref) to
// fade the resource-usage bar colours by a [0,1] factor.
//
// The X360 body unpacks each colour into its four 8-bit channels (alpha = byte>>24,
// red = (byte>>16)&0xFF, green = (byte>>8)&0xFF, blue = byte&0xFF), promotes each to
// double, computes `chT * factor + ch0 * (1 - factor)` per channel, rounds to nearest by
// adding 0.5 and truncating toward zero (fctidz), then repacks the four rounded channel
// bytes back into 0xAARRGGBB. The two shared rodata floats it uses are flt_82001C98 == 1.0f
// (the `1 - factor` complement) and flt_82001DA0 == 0.5f (the round-to-nearest bias).
//
// Endpoint mapping (from the asm: the `* f1` term is the factor-weighted operand and the
// `fmadds ..., f0, ...` term is the (1 - factor)-weighted operand): the FIRST colour arg
// (r5) is weighted by `factor`, the SECOND (r6) by `1 - factor`. So factor == 1 yields
// luColourT, factor == 0 yields luColour0.

namespace BrnResource
{
    namespace
    {
        constexpr s32 KI_ALLOCATOR_STATS_BUFFER_SIZE = 0x2AEC;
        constexpr s32 KI_ALLOCATOR_STATS_ROW_COUNT = 67;
        constexpr s32 KI_RESOURCE_STAT_ROWS_LINEAR = 7;
        constexpr s32 KI_RESOURCE_STAT_ROWS_HEAP = 8;
        constexpr s32 KI_RESOURCE_STAT_ROWS_RWLINEAR = 4;
        constexpr s32 KI_RESOURCE_STAT_ROWS_RWGENERAL = 4;

        const char* const KAPC_ALLOCATOR_TYPE_NAMES[5] = {
            "RAW", "LINEAR", "HEAP", "RWLINEAR", "RWHEAP"
        };

        // ARTIST 0x82662008 initializes these .bss values once through its flag word.
        // The six globals have no other X360 readers or writers.
        u32 guResourceHudColourInitFlags;
        CgsDev::RGBA guResourceHudTextColour;
        CgsDev::RGBA guResourceHudMainBackColour;
        CgsDev::RGBA guResourceHudMainForeColour;
        CgsDev::RGBA guResourceHudVideoBackColour;
        CgsDev::RGBA guResourceHudVideoForeColour;

        // The X360 copy path asserts on a name that reaches 128 bytes, then continues
        // with the original NUL-terminated byte copy after the non-fatal assert.
        void CopyAllocatorName(char (&lacDestination)[128], const char* lpcName)
        {
            u32 luLength = 0;
            while (lpcName[luLength] != '\0')
                ++luLength;

            if (luLength >= 128u)
            {
                CgsDev::Assert::BeginAssert();
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(
                    lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "String " << (lpcName ? lpcName : "<NULLSTRING>")
                           << " is too long. Buffer size = ";
                lStrStream.AppendFormat("0x%X", 128u);
                lStrStream << ", string length = ";
                lStrStream.AppendFormat("%u", luLength);
                lStrStream << "\n";
                CgsDev::Assert::FireAssert(
                    lacMessageBuffer,
                    "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h",
                    65);
                CgsDev::Assert::EndAssert();
            }

            u32 luIndex = 0;
            do
            {
                lacDestination[luIndex] = lpcName[luIndex];
            }
            while (lpcName[luIndex++] != '\0');
        }

        void FadeAllocatorInfo(DebugComponent::MemTypeInfo& lrInfo)
        {
            lrInfo.mfColInterp -= 0.1f;
            if (lrInfo.mfColInterp < 0.0f)
                lrInfo.mfColInterp = 0.0f;
            lrInfo.UpdateMax();
        }
    }

    // ARTIST inlines this DecFIGS helper into Update: fade the current-usage bar,
    // then raise the stored maximum and restart its colour transition on a new peak.
    void DebugComponent::MemTypeInfo::UpdateMax()
    {
        if (miCurrUsed > miMaxUsed)
        {
            mfColInterp = 1.0f;
            miMaxUsed = miCurrUsed;
        }
    }

    // X360 0x826627A8. GenAllocUsage is {allocated, free}; bit 4 adds to element 1
    // (free), while bit 2 adds to element 0 (allocated). The bit-4 test takes
    // precedence when both are set, and ReportHeap continues after every block.
    bool DebugComponent::GeneralAllocatorReportCB(
        const EA::Allocator::GeneralAllocator::BlockInfo* lpBlockInfo, void* lpContext)
    {
        GenAllocUsage* lpUsage = static_cast<GenAllocUsage*>(lpContext);
        if ((lpBlockInfo->mBlockType & 4) != 0)
        {
            lpUsage->muFree += lpBlockInfo->mnDataSize;
            return true;
        }

        if ((lpBlockInfo->mBlockType & 2) != 0)
            lpUsage->muAllocated += lpBlockInfo->mnDataSize;

        return true;
    }

    // X360 0x82666F38; DecFIGS declares a virtual void Update(). The base call is
    // the same empty body that ARTIST folds with BaseCollisionGenerator::Destruct.
    void DebugComponent::Update()
    {
        CgsDev::DebugComponent::Update();

        if (!mbShowHud)
        {
            miSlotsUsed = static_cast<s32>(mpModule->mGameDataEventSlotPool.GetNumUsed());
            miSlotsFree = static_cast<s32>(mpModule->mGameDataEventSlotPool.GetNumFree());
            return;
        }

        if (mpAllocatorStats == 0)
        {
            mpAllocatorStats = static_cast<AllocatorStatsRow*>(
                mpAllocator->AllocateMemoryResource(KI_ALLOCATOR_STATS_BUFFER_SIZE, 16u, 0));
            if (mpAllocatorStats != 0)
                std::memset(mpAllocatorStats, 0, KI_ALLOCATOR_STATS_BUFFER_SIZE);
        }

        if (mpAllocatorStats != 0)
        {
            const CgsMemory::MemoryMap* lpMemoryMap = mpModule->mpMemoryMap;
            GameDataIO::AllocatorList& lrAllocatorList = mpModule->mAllocatorList;

            // Row 0 describes the one raw resource. Its two visible sizes and current
            // usages are the lane-zero and lane-two descriptor sizes from the map.
            if (lrAllocatorList.mapRawResources[0] != 0)
            {
                const CgsMemory::MemoryIO::CreateResourceRequest* lpRequest =
                    lpMemoryMap->GetRawResource(0)->GetRequest();
                AllocatorStatsRow& lrRow = mpAllocatorStats[0];
                CopyAllocatorName(lrRow.macName, lpRequest->GetBankName());
                lrRow.meAllocatorType = CgsMemory::MemoryMap::E_ALLOCATORTYPE_RAW;

                const rw::ResourceDescriptor& lrDescriptor = *lpRequest->GetDescriptor();
                const s32 liMainSize = static_cast<s32>(
                    lrDescriptor.m_baseResourceDescriptors[0].m_size);
                const s32 liVideoSize = static_cast<s32>(
                    lrDescriptor.m_baseResourceDescriptors[2].m_size);
                lrRow.mMainMemInfo.miSize = liMainSize;
                lrRow.mMainMemInfo.miCurrUsed = liMainSize;
                lrRow.mVideoMemInfo.miSize = liVideoSize;
                lrRow.mVideoMemInfo.miCurrUsed = liVideoSize;
            }

            // Rows 1-7: the seven CgsMemory linear allocators.
            for (s32 liIndex = 0; liIndex < KI_RESOURCE_STAT_ROWS_LINEAR; ++liIndex)
            {
                CgsMemory::LinearMalloc* lpLinearAllocator =
                    lrAllocatorList.mapLinearAllocators[liIndex];
                if (lpLinearAllocator == 0)
                    continue;

                const CgsMemory::MemoryIO::CreateAllocatorRequest* lpRequest =
                    lpMemoryMap->GetLinearAllocator(liIndex)->GetRequest();
                AllocatorStatsRow& lrRow = mpAllocatorStats[1 + liIndex];
                CopyAllocatorName(lrRow.macName, lpRequest->GetBankName());
                lrRow.meAllocatorType = CgsMemory::MemoryMap::E_ALLOCATORTYPE_LINEAR;
                lrRow.mMainMemInfo.miCurrUsed = static_cast<s32>(lpLinearAllocator->GetUsage());
                lrRow.mMainMemInfo.miSize = lpRequest->GetSize();
                lrRow.mVideoMemInfo.miSize = 0;
                lrRow.mVideoMemInfo.miCurrUsed = 0;
            }

            // Rows 8-15: eight CgsMemory heap allocators, with PPMalloc's allocated
            // total read through the original callback and its two-word context.
            for (s32 liIndex = 0; liIndex < KI_RESOURCE_STAT_ROWS_HEAP; ++liIndex)
            {
                CgsMemory::HeapMalloc* lpHeapAllocator =
                    lrAllocatorList.mapHeapAllocators[liIndex];
                if (lpHeapAllocator == 0)
                    continue;

                const CgsMemory::MemoryIO::CreateAllocatorRequest* lpRequest =
                    lpMemoryMap->GetHeapAllocator(liIndex)->GetRequest();
                AllocatorStatsRow& lrRow = mpAllocatorStats[8 + liIndex];
                CopyAllocatorName(lrRow.macName, lpRequest->GetBankName());
                lrRow.meAllocatorType = CgsMemory::MemoryMap::E_ALLOCATORTYPE_HEAP;

                GenAllocUsage lUsage = {};
                lpHeapAllocator->GetAllocator()->ReportHeap(
                    &DebugComponent::GeneralAllocatorReportCB, &lUsage, 31, false, 0, 0);
                lrRow.mMainMemInfo.miCurrUsed = static_cast<s32>(lUsage.muAllocated);
                lrRow.mMainMemInfo.miSize = lpRequest->GetSize();
                lrRow.mVideoMemInfo.miSize = 0;
                lrRow.mVideoMemInfo.miCurrUsed = 0;
            }

            // Rows 16-19: four RW linear allocators. Capacity and current usage are
            // descriptors; only the original main/video lanes are displayed.
            for (s32 liIndex = 0; liIndex < KI_RESOURCE_STAT_ROWS_RWLINEAR; ++liIndex)
            {
                rw::LinearResourceAllocator* lpLinearAllocator =
                    lrAllocatorList.mapRWLinearAllocators[liIndex];
                if (lpLinearAllocator == 0)
                    continue;

                const CgsMemory::MemoryIO::CreateLinearAllocatorRequest* lpRequest =
                    lpMemoryMap->GetRWLinearAllocator(liIndex)->GetRequest();
                AllocatorStatsRow& lrRow = mpAllocatorStats[16 + liIndex];
                CopyAllocatorName(lrRow.macName, lpRequest->GetBankName());
                lrRow.meAllocatorType = CgsMemory::MemoryMap::E_ALLOCATORTYPE_RWLINEAR;

                const rw::ResourceDescriptor lCapacity = lpLinearAllocator->GetCapacity();
                const rw::ResourceDescriptor lCurrentUsage = lpLinearAllocator->GetCurrentUsage();
                lrRow.mMainMemInfo.miSize = static_cast<s32>(
                    lCapacity.m_baseResourceDescriptors[0].m_size);
                lrRow.mMainMemInfo.miCurrUsed = static_cast<s32>(
                    lCurrentUsage.m_baseResourceDescriptors[0].m_size);
                lrRow.mVideoMemInfo.miSize = static_cast<s32>(
                    lCapacity.m_baseResourceDescriptors[2].m_size);
                lrRow.mVideoMemInfo.miCurrUsed = static_cast<s32>(
                    lCurrentUsage.m_baseResourceDescriptors[2].m_size);
            }

            // Rows 20-23: four RW general allocators. The X360 skips the main and
            // physical reports for the two original bank ids 24 and 31.
            for (s32 liIndex = 0; liIndex < KI_RESOURCE_STAT_ROWS_RWGENERAL; ++liIndex)
            {
                rw::core::GeneralResourceAllocator* lpGeneralAllocator =
                    lrAllocatorList.mapRWGeneralAllocators[liIndex];
                if (lpGeneralAllocator == 0)
                    continue;

                const CgsMemory::MemoryIO::CreateGeneralAllocatorRequest* lpRequest =
                    lpMemoryMap->GetRWGeneralAllocator(liIndex)->GetRequest();
                AllocatorStatsRow& lrRow = mpAllocatorStats[20 + liIndex];
                CopyAllocatorName(lrRow.macName, lpRequest->GetBankName());
                lrRow.meAllocatorType = CgsMemory::MemoryMap::E_ALLOCATORTYPE_RWHEAP;

                if (lpRequest->GetBankId() != 24 && lpRequest->GetBankId() != 31)
                {
                    const rw::ResourceDescriptor& lrDescriptor = *lpRequest->GetDescriptor();
                    GenAllocUsage lMainUsage = {};
                    GenAllocUsage lVideoUsage = {};

                    // ARTIST GetLinearHeapBase @0x82BBC488 returns this+4. For this
                    // GeneralResourceAllocator, that is its canonical m_mainAllocator.
                    lpGeneralAllocator->m_mainAllocator.ReportHeap(
                        &DebugComponent::GeneralAllocatorReportCB, &lMainUsage,
                        31, false, 0, 0);
                    lpGeneralAllocator->GetPhysicalGeneralAllocator().ReportHeap(
                        &DebugComponent::GeneralAllocatorReportCB, &lVideoUsage,
                        31, false, 0, 0);

                    lrRow.mMainMemInfo.miSize = static_cast<s32>(
                        lrDescriptor.m_baseResourceDescriptors[0].m_size);
                    lrRow.mMainMemInfo.miCurrUsed = static_cast<s32>(lMainUsage.muAllocated);
                    lrRow.mVideoMemInfo.miSize = static_cast<s32>(
                        lrDescriptor.m_baseResourceDescriptors[2].m_size);
                    lrRow.mVideoMemInfo.miCurrUsed = static_cast<s32>(lVideoUsage.muAllocated);
                }
            }

            // The X360 eases and raises both peaks for exactly rows 0-23, even when
            // an allocator slot is currently null.
            for (s32 liIndex = 0; liIndex < 24; ++liIndex)
            {
                FadeAllocatorInfo(mpAllocatorStats[liIndex].mMainMemInfo);
                FadeAllocatorInfo(mpAllocatorStats[liIndex].mVideoMemInfo);
            }
        }

        miSlotsUsed = static_cast<s32>(mpModule->mGameDataEventSlotPool.GetNumUsed());
        miSlotsFree = static_cast<s32>(mpModule->mGameDataEventSlotPool.GetNumFree());
    }

    // X360 0x82662008; DecFIGS declares a virtual void RenderHUD(Debug2DImmediateRender*).
    void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        CgsDev::DebugComponent::RenderHUD(lpRender);
        if (!mbShowHud || mpAllocatorStats == 0)
            return;

        // These values mirror ARTIST's lazy initialization of dword_82FFACF0..F4.
        if ((guResourceHudColourInitFlags & 1u) == 0)
        {
            guResourceHudColourInitFlags |= 1u;
            guResourceHudTextColour = 0xFFFFFFFFu;
        }
        if ((guResourceHudColourInitFlags & 2u) == 0)
        {
            guResourceHudColourInitFlags |= 2u;
            guResourceHudMainBackColour = 0x4B64C864u;
        }
        if ((guResourceHudColourInitFlags & 4u) == 0)
        {
            guResourceHudColourInitFlags |= 4u;
            guResourceHudMainForeColour = 0x4BC8C8C8u;
        }
        if ((guResourceHudColourInitFlags & 8u) == 0)
        {
            guResourceHudColourInitFlags |= 8u;
            guResourceHudVideoBackColour = 0x963232C8u;
        }
        if ((guResourceHudColourInitFlags & 0x10u) == 0)
        {
            guResourceHudColourInitFlags |= 0x10u;
            guResourceHudVideoForeColour = 0x4B6464C8u;
        }

        constexpr f32 lfNameX = 75.0f;
        constexpr f32 lfTypeX = 250.0f;
        constexpr f32 lfMainSizeX = 350.0f;
        constexpr f32 lfMainCurrentX = 450.0f;
        constexpr f32 lfMainPeakX = 550.0f;
        constexpr f32 lfVideoSizeX = 650.0f;
        constexpr f32 lfVideoCurrentX = 750.0f;
        constexpr f32 lfVideoPeakX = 850.0f;
        constexpr f32 lfHeaderY = 40.0f;
        constexpr f32 lfTextYStep = 12.0f;
        constexpr f32 lfTextScale = 12.0f;
        constexpr f32 lfColumnGap = 2.0f;
        constexpr f32 lfHeaderScale = lfTextYStep + lfColumnGap;

        MaybeDrawText(lpRender, "Name", lfNameX, lfHeaderY, lfHeaderScale,
                      guResourceHudTextColour, false);
        MaybeDrawText(lpRender, "Type", lfTypeX, lfHeaderY, lfHeaderScale,
                      guResourceHudTextColour, false);
        MaybeDrawText(lpRender, "MM Size", lfMainSizeX, lfHeaderY, lfHeaderScale,
                      guResourceHudTextColour, false);
        MaybeDrawText(lpRender, "MM Curr Used", lfMainCurrentX, lfHeaderY, lfHeaderScale,
                      guResourceHudTextColour, false);
        MaybeDrawText(lpRender, "MM Max Used", lfMainPeakX, lfHeaderY, lfHeaderScale,
                      guResourceHudTextColour, false);
        MaybeDrawText(lpRender, "VM Size", lfVideoSizeX, lfHeaderY, lfHeaderScale,
                      guResourceHudTextColour, false);
        MaybeDrawText(lpRender, "VM Curr Used", lfVideoCurrentX, lfHeaderY, lfHeaderScale,
                      guResourceHudTextColour, false);
        MaybeDrawText(lpRender, "VM Max Used", lfVideoPeakX, lfHeaderY, lfHeaderScale,
                      guResourceHudTextColour, false);

        f32 lfRowY = lfHeaderY + lfTextYStep + 5.0f;
        for (s32 liIndex = 0; liIndex < KI_ALLOCATOR_STATS_ROW_COUNT; ++liIndex)
        {
            AllocatorStatsRow& lrRow = mpAllocatorStats[liIndex];
            if (lrRow.macName[0] == '\0')
                continue;

            MaybeDrawText(lpRender, lrRow.macName, lfNameX, lfRowY, lfTextScale,
                          guResourceHudTextColour, false);
            MaybeDrawText(lpRender, KAPC_ALLOCATOR_TYPE_NAMES[lrRow.meAllocatorType],
                          lfTypeX, lfRowY, lfTextScale, guResourceHudTextColour, false);
            lpRender->DrawDataSize(lrRow.mMainMemInfo.miSize, lfMainSizeX, lfRowY,
                                   lfTextScale, guResourceHudTextColour);
            lpRender->DrawDataSize(lrRow.mMainMemInfo.miCurrUsed, lfMainCurrentX, lfRowY,
                                   lfTextScale, guResourceHudTextColour);
            lpRender->DrawDataSize(lrRow.mMainMemInfo.miMaxUsed, lfMainPeakX, lfRowY,
                                   lfTextScale, guResourceHudTextColour);
            lpRender->DrawDataSize(lrRow.mVideoMemInfo.miSize, lfVideoSizeX, lfRowY,
                                   lfTextScale, guResourceHudTextColour);
            lpRender->DrawDataSize(lrRow.mVideoMemInfo.miCurrUsed, lfVideoCurrentX, lfRowY,
                                   lfTextScale, guResourceHudTextColour);
            lpRender->DrawDataSize(lrRow.mVideoMemInfo.miMaxUsed, lfVideoPeakX, lfRowY,
                                   lfTextScale, guResourceHudTextColour);

            const f32 lfBarBottomY = lfRowY + lfTextYStep + lfColumnGap;
            const f32 lfBarTopY = lfBarBottomY + lfTextYStep;
            if (lrRow.mMainMemInfo.miSize > 0)
            {
                lpRender->DrawHorizontalBar(
                    Vector2{lfMainPeakX - 10.0f, lfBarTopY, 0.0f, 0.0f},
                    Vector2{lfMainCurrentX, lfBarBottomY, 0.0f, 0.0f},
                    static_cast<f32>(lrRow.mMainMemInfo.miCurrUsed),
                    static_cast<f32>(lrRow.mMainMemInfo.miSize),
                    guResourceHudMainForeColour, guResourceHudMainBackColour);

                const f32 lfFactor = 1.0f - lrRow.mMainMemInfo.mfColInterp;
                const CgsDev::RGBA luBackColour = InterpolateColours(
                    guResourceHudMainBackColour, guResourceHudVideoBackColour, lfFactor);
                const CgsDev::RGBA luForeColour = InterpolateColours(
                    guResourceHudMainForeColour, guResourceHudVideoForeColour, lfFactor);
                lpRender->DrawHorizontalBar(
                    Vector2{lfVideoSizeX - 10.0f, lfBarTopY, 0.0f, 0.0f},
                    Vector2{lfMainPeakX, lfBarBottomY, 0.0f, 0.0f},
                    static_cast<f32>(lrRow.mMainMemInfo.miMaxUsed),
                    static_cast<f32>(lrRow.mMainMemInfo.miSize),
                    luForeColour, luBackColour);
            }

            if (lrRow.mVideoMemInfo.miSize > 0)
            {
                lpRender->DrawHorizontalBar(
                    Vector2{lfVideoPeakX - 10.0f, lfBarTopY, 0.0f, 0.0f},
                    Vector2{lfVideoCurrentX, lfBarBottomY, 0.0f, 0.0f},
                    static_cast<f32>(lrRow.mVideoMemInfo.miCurrUsed),
                    static_cast<f32>(lrRow.mVideoMemInfo.miSize),
                    guResourceHudMainForeColour, guResourceHudMainBackColour);

                const f32 lfFactor = 1.0f - lrRow.mVideoMemInfo.mfColInterp;
                const CgsDev::RGBA luBackColour = InterpolateColours(
                    guResourceHudMainBackColour, guResourceHudVideoBackColour, lfFactor);
                const CgsDev::RGBA luForeColour = InterpolateColours(
                    guResourceHudMainForeColour, guResourceHudVideoForeColour, lfFactor);
                lpRender->DrawHorizontalBar(
                    Vector2{950.0f - 10.0f, lfBarTopY, 0.0f, 0.0f},
                    Vector2{lfVideoPeakX, lfBarBottomY, 0.0f, 0.0f},
                    static_cast<f32>(lrRow.mVideoMemInfo.miMaxUsed),
                    static_cast<f32>(lrRow.mVideoMemInfo.miSize),
                    luForeColour, luBackColour);
            }

            lfRowY += (lfTextYStep + lfColumnGap) * 2.0f;
        }
    }

    // X360 GameDataModule::Construct @0x82671E48-0x82671E74; DecFIGS
    // BrnResource::DebugComponent::Construct @0x7CC378. The X360 inlines
    // the base Construct and these member stores at the embedded member.
    void DebugComponent::Construct(GameDataModule* lpModule, rw::IResourceAllocator* lpAllocator)
    {

        mpModule = lpModule;
        mbShowHud = true;
        mpAllocatorStats = 0;
        miSlotsUsed = 0;
        miSlotsFree = 0;
        mpAllocator = lpAllocator;
    }

    // X360 0x826626A8; DecFIGS 0x7D94F8. ARTIST calls the bool, int,
    // read-only operations in this exact order and continues after the
    // non-fatal module assertion.
    void DebugComponent::OnActivate()
    {
        if (mpModule == 0)
        {
            CgsDev::Assert::BeginAssert();
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "No module\n";
            CgsDev::Assert::FireAssert(
                lacMessageBuffer,
                "..\\..\\..\\GameSource\\Resource/BrnResourceDebugComponent.cpp",
                313);
            CgsDev::Assert::EndAssert();
        }

        RegisterVariable(&mbShowHud, "Show HUD");
        RegisterVariable(&miSlotsUsed, "Slots Used");
        SetReadOnly(&miSlotsUsed, true);
        RegisterVariable(&miSlotsFree, "Slots Free");
        SetReadOnly(&miSlotsFree, true);
    }

    namespace
    {
        // Round a non-negative interpolated channel to the nearest integer the way the X360
        // does: add 0.5 then truncate toward zero (fctidz), and keep it in [0,255].
        u8 RoundChannel(f64 lfChannel)
        {
            const s32 liRounded = static_cast<s32>(lfChannel + 0.5);
            return static_cast<u8>(liRounded);
        }

        // Linearly interpolate one 8-bit channel: chT * factor + ch0 * (1 - factor).
        u8 LerpChannel(u8 lu8ChannelT, u8 lu8Channel0, f32 lfFactor)
        {
            const f64 lfFactorT = static_cast<f64>(lfFactor);
            const f64 lfFactor0 = 1.0 - lfFactorT;
            const f64 lfResult =
                static_cast<f64>(lu8ChannelT) * lfFactorT +
                static_cast<f64>(lu8Channel0) * lfFactor0;
            return RoundChannel(lfResult);
        }
    }

    u32 DebugComponent::InterpolateColours(u32 luColourT, u32 luColour0, f32 lfFactor)
    {
        const u8 lu8AlphaT = static_cast<u8>(luColourT >> 24);
        const u8 lu8RedT   = static_cast<u8>(luColourT >> 16);
        const u8 lu8GreenT = static_cast<u8>(luColourT >> 8);
        const u8 lu8BlueT  = static_cast<u8>(luColourT);

        const u8 lu8Alpha0 = static_cast<u8>(luColour0 >> 24);
        const u8 lu8Red0   = static_cast<u8>(luColour0 >> 16);
        const u8 lu8Green0 = static_cast<u8>(luColour0 >> 8);
        const u8 lu8Blue0  = static_cast<u8>(luColour0);

        const u8 lu8Alpha = LerpChannel(lu8AlphaT, lu8Alpha0, lfFactor);
        const u8 lu8Red   = LerpChannel(lu8RedT,   lu8Red0,   lfFactor);
        const u8 lu8Green = LerpChannel(lu8GreenT, lu8Green0, lfFactor);
        const u8 lu8Blue  = LerpChannel(lu8BlueT,  lu8Blue0,  lfFactor);

        return (static_cast<uint32_t>(lu8Alpha) << 24) |
               (static_cast<uint32_t>(lu8Red)   << 16) |
               (static_cast<uint32_t>(lu8Green) << 8)  |
                static_cast<uint32_t>(lu8Blue);
    }
}
