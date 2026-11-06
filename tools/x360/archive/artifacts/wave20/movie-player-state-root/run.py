import hashlib,json,os,pathlib,subprocess,sys
D=pathlib.Path(__file__).resolve().parent
W=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
HEAD='a888255de9c1263a043d9fa6364d513d66b2d30a'
P='src/GameShared/GameClasses/Graphics/MoviePlayer/'
C=P+'CgsMoviePlayer.cpp'
CH=P+'CgsMoviePlayerChunkBuffer.h'
CC=P+'CgsMoviePlayerChunkBuffer.cpp'
GUI='src/GameSource/Gui/BrnGuiMovieManager.cpp'
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import u32
def git(*a):return subprocess.check_output(['git',*a],cwd=W).decode().strip()
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
assert git('rev-parse','HEAD')==HEAD and not git('status','--porcelain=v1')
assert u32(0x82815570)==0x7c8a2378 and u32(0x82F264A8)==4
assert u32(0x82004fd8)==0x41c80000
assert u32(0x82507f44)&0x03fffffc==(0x827ea470-0x82507f44)&0x03fffffc
file=W/CH
b=file.read_bytes()
old=b'        MoviePlayerCoreAllocator();'
assert b.count(old)==1
file.write_bytes(b.replace(old,b'        MoviePlayerCoreAllocator() {}'))
with (W/C).open('x',encoding='utf-8',newline='\n') as f:
    f.write("""#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

namespace CgsGraphics
{
    // ARTIST 0x827DD390. The stream, allocator and resource records initialize themselves.
    MoviePlayer::MoviePlayer()
    {
    }

    // ARTIST 0x827EA178
    void MoviePlayer::Construct()
    {
        mfRectLeft = 0.0f;
        mfRectTop = 0.0f;
        mpRwAllocator = 0;
        mpIm2dRenderBuffer = 0;
        mpRwVideoRenderer = 0;
        mfRectRight = 1.0f;
        mpRwVideoRenderableOperations = 0;
        mfRectBottom = 1.0f;
        mePlayerState = E_RW_MOVIE_PLAYER_NULL;
        macMovieFileName[0] = 0;
        mbIsLooped = false;
        miCrossfadeInFrames = 0;
        miCrossfadeOutFrames = 0;
        mbDecodingFrame = false;
        miVideoRender = CgsDev::PerfMonCpu::AddMonitor("Video", CgsDev::E_PMP_22, false, 25.0f, true);
    }

    // ARTIST 0x827EA448
    void MoviePlayer::SetRectangle(f32 lfLeft, f32 lfTop, f32 lfRight, f32 lfBottom)
    {
        mfRectLeft = lfLeft;
        mfRectTop = lfTop;
        mfRectRight = lfRight;
        mfRectBottom = lfBottom;
    }

    // ARTIST 0x827EA460
    void MoviePlayer::SetCrossfade(s32 liCrossfadeInFrames, s32 liCrossfadeOutFrames)
    {
        miCrossfadeInFrames = liCrossfadeInFrames;
        miCrossfadeOutFrames = liCrossfadeOutFrames;
    }

    // ARTIST 0x827EA470
    void MoviePlayer::Play()
    {
        if (mePlayerState == E_RW_MOVIE_PLAYER_PREPARED)
            mePlayerState = E_RW_MOVIE_PLAYER_PLAYING;
    }

    // ARTIST 0x827EA488
    void MoviePlayer::Stop()
    {
        const s32 liTotalFrames = maVP6Headers[0].mTotalFrames;
        mePlayerState = E_RW_MOVIE_PLAYER_STOPPED;
        miCurrentFrame = liTotalFrames;
    }
}
""")
git('diff','--check','--',CH)
args=json.loads((D.parent/'r26-root-providers/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(D/'native')
for p in (C,CC,GUI):args+=['--source',p]
temp=D/'temp'
temp.mkdir()
(D/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=W,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report=D/'native/compile-results.json'
r=json.loads(report.read_text())
assert r['passed']==r['requested']==3 and r['failed']==0
rows={row['source']:row for row in r['results']}
assert all(row['machine']==498 for row in r['results'])
src=pathlib.Path(rows[C]['assembly']).read_text(errors='replace')
caller=pathlib.Path(rows[GUI]['assembly']).read_text(errors='replace')
symbols=('??0MoviePlayer@CgsGraphics@@QAA@XZ','?Construct@MoviePlayer@CgsGraphics@@QAAXXZ',
         '?SetRectangle@MoviePlayer@CgsGraphics@@QAAXMMMM@Z','?SetCrossfade@MoviePlayer@CgsGraphics@@QAAXHH@Z',
         '?Play@MoviePlayer@CgsGraphics@@QAAXXZ','?Stop@MoviePlayer@CgsGraphics@@QAAXXZ')
for sym in symbols:
    assert 'Begin code for function: '+sym in src and sym in caller,sym
ctor=src.split('Begin code for function: '+symbols[0],1)[1].split('End code for function:',1)[0]
assert 'Futex' in ctor and ('884h(' in ctor or '2180(' in ctor)
msg=D/'message.txt'
msg.write_text('Restore native movie-player construction and playback state\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',C,CH)
assert set(git('diff','--cached','--name-only').splitlines())=={C,CH}
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
result={'base':HEAD,'commit':git('rev-parse','HEAD'),'paths':{p:sha(W/p) for p in (C,CH)},
        'artist':{'constructor':'0x827DD390','Construct':'0x827EA178','SetRectangle':'0x827EA448',
                  'SetCrossfade':'0x827EA460','Play':'0x827EA470','Stop':'0x827EA488'},
        'verified_provider_symbols':symbols,'report':str(report),'report_sha256':sha(report)}
(D/'DELIVERY.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['commit'])
