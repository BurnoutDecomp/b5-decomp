from pathlib import Path
import json,os,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
base='6c0bcba0af1e35b05da730b8fd7a24a592e95cf5'
start='3ad4b70bd7d39674e99b32c6f0e4961c7eb0e109'
def git(*args):return subprocess.check_output(['git',*args],cwd=w).decode().rstrip()
assert git('rev-parse','HEAD')==start and not git('status','--porcelain=v1')
git('switch','-c','codex/experimental-x360-r28-movie-player-render',base)
git('cherry-pick','c50043063fbc20a50ff0f4d96df3da587fdf962c',start)
parent=git('rev-parse','HEAD')
src='src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.cpp'
p=w/src
data=p.read_bytes()
eol=b'\r\n' if b'\r\n' in data else b'\n'
old=b'#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"'
assert data.count(old)==1
data=data.replace(old,old+eol+b'#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.h"')
anchor=b'    // ARTIST 0x827EA448'
assert data.count(anchor)==1
body=r'''    // ARTIST 0x827FF110
    void MoviePlayer::Render(Im2dRenderBuffer* lpIm2dRenderBuffer)
    {
        mpIm2dRenderBuffer = lpIm2dRenderBuffer;
        if (mePlayerState != E_RW_MOVIE_PLAYER_PLAYING)
            return;

        CGS_ASSERT(mpIm2dRenderBuffer, "mpIm2dRenderBuffer");
        CgsDev::PerfMonCpu::StartMonitor(miVideoRender);
        if (mbReadyToRender)
        {
            s32 liRow = 0;
            for (s32 liIndex = miNumVideos - 1; liIndex >= 0; --liIndex)
            {
                rw::movie::VideoRenderable lRenderable;
                const YUV_BUFFER_CONFIG lYUVConfig = maYUVConfigs[liIndex];
                const VP6ChunkHeader& lrHeader = maVP6Headers[liIndex];
                const u32 luWidth = lrHeader.mWidth;
                const u32 luHeight = lrHeader.mHeight;

                lRenderable.SetWidth(luWidth);
                lRenderable.SetHeight(luHeight);
                lRenderable.SetFrameNumber(miCurrentFrame);
                lRenderable.SetNumBuffersUsed(3);
                lRenderable.SetFormat(rw::movie::VideoRenderable::VIDEOFORMAT_YV12);
                lRenderable.SetDropFrameFlag(false);
                lRenderable.SetReadyToRender(true);
                lRenderable.SetStride(lYUVConfig.YStride, 0);
                lRenderable.SetStride(lYUVConfig.UVStride, 1);
                lRenderable.SetStride(lYUVConfig.UVStride, 2);
                lRenderable.SetData(reinterpret_cast<u8*>(lYUVConfig.YBuffer), 0);
                lRenderable.SetData(reinterpret_cast<u8*>(lYUVConfig.UBuffer), 1);
                lRenderable.SetData(reinterpret_cast<u8*>(lYUVConfig.VBuffer), 2);
                lRenderable.SetSize(luHeight * lYUVConfig.YStride, 0);
                const u32 luChromaSize = (luHeight * lYUVConfig.UVStride) >> 2;
                lRenderable.SetSize(luChromaSize, 1);
                lRenderable.SetSize(luChromaSize, 2);

                mpRwVideoRenderer->Render(liRow, &lRenderable, liIndex == 0);
                liRow += lrHeader.mHeight;
            }

            if (!mbDroppingFrames)
                WaitForDecode();
        }
        CgsDev::PerfMonCpu::StopMonitor(miVideoRender);
    }

'''
p.write_bytes(data.replace(anchor,body.encode().replace(b'\n',eol)+anchor))
git('diff','--check')
(d/'parent.txt').write_text(parent+'\n')
prior=json.loads((d.parent/'movie-output-texture-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
for path in (src,'src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.cpp',
             'src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer_wBT_01.cpp',
             'src/GameSource/Gui/BrnGuiMovieManager.cpp'):
    args+=['--source',path]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp';tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
