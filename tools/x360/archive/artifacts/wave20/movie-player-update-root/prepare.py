from pathlib import Path
import json,os,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
base='617bf88ee7731401f240148a6feff05c4e61e625'
def git(*args):return subprocess.check_output(['git',*args],cwd=w).decode().rstrip()
assert git('rev-parse','HEAD')==base and not git('status','--porcelain=v1')
src='src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.cpp'
p=w/src
data=p.read_bytes()
eol=b'\r\n' if b'\r\n' in data else b'\n'
def edit(before,after):
    global data
    a=before.encode().replace(b'\n',eol)
    b=after.encode().replace(b'\n',eol)
    assert data.count(a)==1
    data=data.replace(a,b)
edit('#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"',
'''#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h"
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"''')
edit('    // ARTIST 0x827FF110',r'''    // ARTIST 0x827EEFB0
    void MoviePlayer::Update()
    {
        if (miCurrentFrame >= maVP6Headers[0].mTotalFrames)
        {
            miCurrentFrame = maVP6Headers[0].mTotalFrames;
            mePlayerState = E_RW_MOVIE_PLAYER_STOPPED;
            return;
        }
        if (mePlayerState != E_RW_MOVIE_PLAYER_PLAYING)
            return;

        const u64 luNow = CgsSystem::GetSystemTimerBaseTime();
        s32 liNumFramesToDecode = 1;
        if (!mbReadyToRender)
        {
            muStartTimeInCycles = luNow;
        }
        else
        {
            const u64 luNumerator = static_cast<u64>(maVP6Headers[0].mFpsNumerator);
            const s32 liDenominator = maVP6Headers[0].mFpsDenominator;
            const u64 luDenominator = static_cast<u64>(liDenominator);
            const u64 luScaledCycles = ((luNow - muStartTimeInCycles) * luNumerator) / luDenominator;
            const u64 luElapsedFrames = luScaledCycles / CgsSystem::GetSystemTimerFrequency();
            const u32 luFramesDue = static_cast<u32>(luElapsedFrames) - static_cast<u32>(miCurrentFrame) + 1;
            liNumFramesToDecode = CgsNumeric::Max(static_cast<s32>(luFramesDue), 0);

            if (liNumFramesToDecode > 4)
            {
                // ARTIST multiplies the negative offset in u64 and divides it unsigned.
                const u64 luFrequency = CgsSystem::GetSystemTimerFrequency();
                const u64 luCycleOffset = luFrequency * (miCurrentFrame - 1) * (-liDenominator);
                muStartTimeInCycles = luNow + luCycleOffset / luNumerator;

                const u64 luAdjustedCycles = ((luNow - muStartTimeInCycles) * luNumerator) / luDenominator;
                const u64 luAdjustedFrames = luAdjustedCycles / CgsSystem::GetSystemTimerFrequency();
                liNumFramesToDecode = static_cast<s32>(static_cast<u32>(luAdjustedFrames) -
                                                       static_cast<u32>(miCurrentFrame) + 1);
                CGS_ASSERT(liNumFramesToDecode == 0, "liNumFramesToDecode == 0");
            }
        }

        const s32 liTotalFrames = maVP6Headers[0].mTotalFrames;
        if (miCurrentFrame + liNumFramesToDecode > liTotalFrames)
            liNumFramesToDecode = liTotalFrames - miCurrentFrame;

        mbDroppingFrames = true;
        while (liNumFramesToDecode > 0)
        {
            if (!ReadChunkSet(miNumAudios + miNumVideos))
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "Stalling to wait for stream\n";
                break;
            }

            StartDecodes();
            if (liNumFramesToDecode > 1)
                WaitForDecode();
            --liNumFramesToDecode;
            mbReadyToRender = true;
            mbDroppingFrames = false;
        }
    }

    // ARTIST 0x827FF110''')
p.write_bytes(data)
git('diff','--check')
prior=json.loads((d.parent/'movie-player-render-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
args+=prior[prior.index('--source'):]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp';tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
