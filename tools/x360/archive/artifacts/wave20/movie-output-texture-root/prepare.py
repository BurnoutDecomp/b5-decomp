from pathlib import Path
import json,os,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
base='3792b7d0f752405cbf160c530832492df09da639'
branch='codex/experimental-x360-r28-movie-texture'
def git(*args):return subprocess.check_output(['git',*args],cwd=w).decode().rstrip()
assert not git('status','--porcelain=v1')
git('switch','-c',branch,base)
src='src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.cpp'
p=w/src
data=p.read_bytes()
eol=b'\r\n' if b'\r\n' in data else b'\n'
anchor=b'    unsigned int MovieVideoRenderer::GetNumberOfVideoBuffers'
assert data.count(anchor)==1
body=r'''    // ARTIST 0x827F8120
    void MovieVideoRenderer::CreateOutputTexture(TextureInfoType* lpTextureTypeInfo,
                                                 u32 luWidth, u32 luHeight)
    {
        const char* const lpcFile =
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\graphics\\MoviePlayer/CgsMovieVideoRenderer.cpp";
        renderengine::Texture2D::Parameters lTextureParameters;
        lTextureParameters.muFlags = 0;
        lTextureParameters.muWidth = luWidth;
        lTextureParameters.muHeight = luHeight;
        lTextureParameters.muNumLevels = 1;
        lTextureParameters.meFormat = renderengine::PIXELFORMAT_A8R8G8B8;

        const rw::ResourceDescriptor lTextureDescriptor =
            renderengine::Texture2D::GetResourceDescriptor(lTextureParameters);
        lpTextureTypeInfo->mTextureResource =
            mpParentMoviePlayer->mpRwAllocator->DoAllocate(lTextureDescriptor, 0);
        if (!lpTextureTypeInfo->mTextureResource.m_baseResources[2])
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpTextureTypeInfo->mTextureResource.GetPhysicalMemoryResource()",
                                       lpcFile, 566);
            CgsDev::Assert::EndAssert();
        }

        lpTextureTypeInfo->mpTexture =
            renderengine::Texture2D::Initialize(lpTextureTypeInfo->mTextureResource, lTextureParameters);
        renderengine::Texture::Locked lLockedTexture;
        if (!lpTextureTypeInfo->mpTexture->Lock(renderengine::Texture::LOCK_WRITE, 0, 0, lLockedTexture))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpTextureTypeInfo->mpTexture->Lock(rw::graphics::core::Texture2D::LOCK_WRITE, 0, lLockedTexture)",
                lpcFile, 579);
            CgsDev::Assert::EndAssert();
        }
        lpTextureTypeInfo->mpPixelData = static_cast<u32*>(lLockedTexture.mpPixelData);

        renderengine::TextureState::Parameters lStateParameters;
        lStateParameters.muAddressU = D3DTADDRESS_CLAMP;
        lStateParameters.muAddressV = D3DTADDRESS_CLAMP;
        lStateParameters.muMagFilter = D3DTEXF_LINEAR;
        lStateParameters.muMinFilter = D3DTEXF_LINEAR;
        lStateParameters.mpTexture = lpTextureTypeInfo->mpTexture;
        const rw::ResourceDescriptor lStateDescriptor =
            renderengine::TextureState::GetResourceDescriptor(lStateParameters);
        lpTextureTypeInfo->mTextureStateResource =
            mpParentMoviePlayer->mpRwAllocator->DoAllocate(lStateDescriptor, 0);
        lpTextureTypeInfo->mpTextureState =
            renderengine::TextureState::Initialize(lpTextureTypeInfo->mTextureStateResource, lStateParameters);
    }

'''
p.write_bytes(data.replace(anchor,body.encode().replace(b'\n',eol)+anchor))
git('diff','--check')
prior=json.loads((d.parent/'r26-stream-lock-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
for path in (src,'src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.cpp','src/GameSource/Gui/BrnGuiMovieManager.cpp'):
    args+=['--source',path]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp'
tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
