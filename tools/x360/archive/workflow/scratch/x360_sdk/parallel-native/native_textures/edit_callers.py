from pathlib import Path
root=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_textures')
p=root/'src/GameSource/Game/BrnLoadingScreenRenderer.cpp'
s=p.read_text(encoding='utf-8')
s=s.replace('#include <cstdio>\n#include <cstdlib>\n', '#include "GameSource/Resource/BrnResourceAllocator.h"\n#include "GameShared/GameClasses/Core/CgsAssert.h"\n#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"\n')
a=s.index('// Reconstructed from BURNOUT_X360_ARTIST.XEX');b=s.index('namespace\n{',a)
s=s[:a]+'''// ARTIST owns the native loading path and layout. The four embedded tiled
// DXT5 arrays are linked from the original data; their symbols retain the
// DecFIGS names. The car staging region becomes the reusable scratch allocator.
namespace BrnResource { LinearResourceAllocator* GetEmbeddedTextureAllocator(); }
namespace BrnGame
{
    extern u8 gpLoadingScreenTextureArrow[0x4000];
    extern u8 gpLoadingScreenTextureCar[0x200000];
    extern u8 gpLoadingScreenTextureText[0x40000];
    extern u8 gpLoadingScreenTextureBox[0x4000];
}

'''+s[b:]
s=s.replace('(53.5f / 64.0f) * (64.0f / 128.0f)', '(53.5f / 64.0f)').replace('(51.0f / 64.0f) * (64.0f / 128.0f)', '(51.0f / 64.0f)')
s=s.replace('29.0f / 512.0f','28.0f / 512.0f').replace('429.0f / 512.0f','428.0f / 512.0f').replace('329.0f / 512.0f','328.0f / 512.0f').replace('229.0f / 512.0f','228.0f / 512.0f')
s=s.replace('            laVerts[i].mv2Pos = { laPos[i].x, laPos[i].y };','            laVerts[i].mv2Pos = laPos[i];').replace('            laVerts[i].mv2Tex0UV = { laUV[i][0], laUV[i][1] };','            laVerts[i].mv2Tex0UV.x = laUV[i][0];\n            laVerts[i].mv2Tex0UV.y = laUV[i][1];')
s=s.replace('namespace\n{\n', '''namespace
{
    CgsGraphics::Vector2 Point(f32 x, f32 y)
    {
        CgsGraphics::Vector2 point = { x, y };
        return point;
    }

    void SetScreenTransform(CgsGraphics::Im2d* renderer)
    {
        CgsGraphics::Im2dTransform transform;
        const rw::math::vpu::Vector4 origin = { -1.0f, 1.0f, 0.0f, 0.0f };
        const rw::math::vpu::Vector4 rightUp = { 0.0015625f, 0.0f, 0.0f, -0.0027777778f };
        const rw::math::vpu::Vector4 shift = { 0.0f, 0.0f, 0.0f, 0.0f };
        const rw::math::vpu::Vector4 scale = { 1.0f, 1.0f, 1.0f, 1.0f };
        transform.mOriginXYZ = origin;
        transform.mRightUp = rightUp;
        transform.mColourShift = shift;
        transform.mColourScale = scale;
        transform.TransformByAspectRatio();
        renderer->SetTransform(transform);
    }
''',1)
a=s.index('        renderengine::Texture2D::Parameters lParams = {};');b=s.index('    // @ 0x823AAD48',a)
s=s[:a]+'''        renderengine::Texture2D::Parameters lParams;
        lParams.muFlags = 0;
        lParams.muWidth = static_cast<u32>(lfWidth + 0.5f);
        lParams.muHeight = static_cast<u32>(lfHeight + 0.5f);
        lParams.muNumLevels = 1;
        lParams.meFormat = renderengine::PIXELFORMAT_DXT5;

        renderengine::Texture2D::ResourceDescriptor lDesc;
        renderengine::Texture2D::GetResourceDescriptor(&lDesc, &lParams);
        rw::ResourceDescriptor lAllocationDesc;
        for (u32 i = 0; i != 5; ++i)
        {
            lAllocationDesc.m_baseResourceDescriptors[i].m_size = lDesc.maEntries[i].muSize;
            lAllocationDesc.m_baseResourceDescriptors[i].m_alignment = lDesc.maEntries[i].muAlignment;
        }
        const rw::Resource lResource = BrnResource::GetEmbeddedTextureAllocator()->DoAllocate(lAllocationDesc, 0);
        renderengine::Texture2D* lpTexture = renderengine::Texture2D::Initialize(&lResource, &lParams);
        renderengine::Texture::Locked lLocked;
        renderengine::Texture::Lock(lpTexture, 0, 0, 0, &lLocked);
        XMemCpy(lLocked.mpPixelData, lpcData, static_cast<size_t>(liDataSize));
        renderengine::Texture::Unlock(lpTexture, &lLocked);
        return lpTexture;
    }

    // ARTIST 823CE208. Preserve the exact initialization stores: this object is
    // statically zeroed before Construct; the function does not reset every field.
    void LoadingScreenRenderer::Construct()
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Sizes: Arrow=" << 0x4000
                << ", Car=" << 0x200000 << ", Text=" << 0x40000 << ", Box=" << 0x4000 << "\\n";
        mpArrowTexture = SetupLoadingScreenTexture(64.0f, 64.0f, gpLoadingScreenTextureArrow, 0x4000);
        mpCarTexture = SetupLoadingScreenTexture(2048.0f, 1024.0f, gpLoadingScreenTextureCar, 0x200000);
        mpTextTexture = SetupLoadingScreenTexture(512.0f, 512.0f, gpLoadingScreenTextureText, 0x40000);
        mpBoxTexture = SetupLoadingScreenTexture(64.0f, 64.0f, gpLoadingScreenTextureBox, 0x4000);
        mReusableDataBuffer.Construct();
        mReusableDataBuffer.Create(gpLoadingScreenTextureCar, 0x200000);
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Created re-usable data buffer of " << 0x200000 << " bytes\\n";
        mbVisible = false;
        switch (CgsSystem::HardwareSku::FindLanguage())
        {
        case 7: case 8: meLanguage = E_LOADINGLANGUAGE_ENGLISH; break;
        case 10: meLanguage = E_LOADINGLANGUAGE_FRENCH; break;
        case 11: meLanguage = E_LOADINGLANGUAGE_GERMAN; break;
        case 15: meLanguage = E_LOADINGLANGUAGE_ITALIAN; break;
        case 16: meLanguage = E_LOADINGLANGUAGE_JAPANESE; break;
        case 22: meLanguage = E_LOADINGLANGUAGE_SPANISH; break;
        default:
            CgsDev::Assert::BeginAssert();
            {
                CgsDev::StrStream message(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
                message << "Language does not match SKU\\n";
                CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                    "d:\\\\p4\\\\b5_main\\\\burnout\\\\main\\\\code\\\\gamesource\\\\unity\\\\../Game/BrnLoadingScreenRenderer.cpp", 245);
            }
            CgsDev::Assert::EndAssert();
            break;
        }
        mbRenderInBackground = false;
        mbBlackOverlayHiding = false;
        mbBlackOverlayVisible = false;
        mbKillBlackOverlayWhenDone = false;
        mfBlackOverlayFade = 0.0f;
        mfArrowTranslation = 0.0f;
        mfRotateSpeedInterp = 0.0f;
        mfArrowDirection = 1.0f;
    }

'''+s[b:]
a=s.index('        // PC bring-up trace:');b=s.index('        switch (leCommand)',a);s=s[:a]+s[b:]
a=s.index('            // [PC timing seam]');b=s.index('            mfBlackOverlayFade = 1.2f;',a);s=s[:a]+s[b:]
s=s.replace('        Im2dTransform lScreenXForm;\n        lScreenXForm.TransformByAspectRatio();\n        lpIm2d->SetTransform(lScreenXForm);','        SetScreenTransform(lpIm2d);')
s=s.replace('{0.0f,0.0f}, {KF_SCREEN_W,0.0f}, {0.0f,KF_SCREEN_H}, {KF_SCREEN_W,KF_SCREEN_H}', 'Point(0.0f,0.0f), Point(KF_SCREEN_W,0.0f), Point(0.0f,KF_SCREEN_H), Point(KF_SCREEN_W,KF_SCREEN_H)')
a=s.index('        const u32 luFirstTime =');b=s.index('        if (!mbRenderInBackground',a)
s=s[:a]+'''        const u64 luFirstTime = CgsSystem::GetSystemTimerBaseTime64();
        const u64 luSecondTime = CgsSystem::GetSystemTimerBaseTime64();
        const u64 luDelta = luSecondTime - muLastTime;
        muLastTime = luFirstTime;
        const u64 luFrequency = CgsSystem::GetSystemTimerFrequency64();
        f32 lfStep = static_cast<f32>((luDelta * 10000) / luFrequency) * 0.0001f;
        if (lfStep > 0.1f) lfStep = 0.1f;
        mfTimeStep = lfStep;

'''+s[b:]
a=s.index('    // @ 0x823AAD48 (RenderDiskErrorMessage)');s=s[:a]+'}\n'
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
p=root/'src/GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.cpp'
s=p.read_text(encoding='utf-8')
s=s.replace('#include <cstdio>    // [diag] snprintf ([netimg] slot-ring probe)\n#include <cstdlib>   // [diag] getenv   (BRN_NETIMG gates it)\n#include "GameShared/GameClasses/Development/Log/CgsLog.h"                     // [diag] CgsDev::Log::WriteToLog\n', '#include "rw/rwcore_structs.h"\n')
a=s.index('        // ================= [netimg]');b=s.index('        // The id the default texture',a)
s=s[:a]+'''        renderengine::Texture2D* AllocateTexture(rw::IResourceAllocator* allocator,
            const renderengine::Texture2D::ResourceDescriptor& descriptor,
            const renderengine::Texture2D::Parameters& parameters)
        {
            rw::ResourceDescriptor allocation;
            for (u32 i = 0; i != 5; ++i)
            {
                allocation.m_baseResourceDescriptors[i].m_size = descriptor.maEntries[i].muSize;
                allocation.m_baseResourceDescriptors[i].m_alignment = descriptor.maEntries[i].muAlignment;
            }
            const rw::Resource resource = allocator->DoAllocate(allocation, 0);
            return renderengine::Texture2D::Initialize(&resource, &parameters);
        }

'''+s[b:]
s=s.replace('            lNormalParams.muDepth     = 1u;\n','').replace('.muFormat', '.meFormat').replace('lNormalParams.muUsage', 'lNormalParams.muFlags')
s=s.replace('            lNormalParams.mauReserved[0] = 0u;\n            lNormalParams.mauReserved[1] = 0u;\n','')
for typ in ['Normal','YUY2','Compressed']:
    s=s.replace(f'renderengine::Texture2D::Initialize(\n                        &l{typ}Desc, &l{typ}Params)', f'AllocateTexture(\n                        mpTextureAllocator, l{typ}Desc, l{typ}Params)')
    s=s.replace(f'renderengine::Texture2D::Initialize(\n                    &l{typ}Desc, &l{typ}Params)', f'AllocateTexture(\n                    mpTextureAllocator, l{typ}Desc, l{typ}Params)')
import re
s=re.sub(r'        // \[netimg\][\s\S]*?\n(?=        (?:if \(miClear|if \(mbUse|CGS_ASSERT\(mpFlapt))', '',s)
s=re.sub(r'            // \[netimg\][\s\S]*?\n(?=            if \(mbRenderTexture)', '',s)
s='\n'.join(line for line in s.split('\n') if 'guNetImg' not in line)
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
