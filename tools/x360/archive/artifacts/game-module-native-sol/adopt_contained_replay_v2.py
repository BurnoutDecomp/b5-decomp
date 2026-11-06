from pathlib import Path
import re,json
ROOT=Path(r'D:/codex-native/game-module-sol');OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol')
files=['src/GameSource/Director/BrnDirectorModule.h','src/GameSource/Director/BrnDirectorModule.cpp','src/GameSource/Director/DirectorModule/BrnDirectorModule.cpp']
for rel in files:
 p=ROOT/rel;(OUT/(p.parent.name+'-'+p.name+'.before')).write_bytes(p.read_bytes())
p=ROOT/files[0];s=p.read_text(encoding='utf-8')
s=s.replace('#include "GameSource/Director/Camera/Camera.h"', '#include "GameSource/Director/BrnReplayDirector.h"\n#include "GameSource/Replays/Serialisers/BrnReplayDirectorSerialiser.h"\n#include "GameSource/Director/Camera/Camera.h"',1)
a=s.index('    u8 mPad_0x35F50[');b=s.index('    // this+0x38990',a)
s=s[:a]+'''    // ARTIST827E2020 builds this genuine contained object. Its current/last
    // camera at+430 and replay-selector fields at+29D0/+29D4 belong inside it.
    ReplayDirector mReplayDirector;                          // native35F50

    BrnReplays::DirectorSerialiser mDirectorSerialiser;       // native38930

'''+s[b:]
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
p=ROOT/files[2];s=p.read_text(encoding='utf-8')
for before,after in [('mCamera','mReplayDirector.mLastCamera'),('mbReplayStartedThisFrame','mReplayDirector.mbStartedThisFrame'),('miPostGuiCarIndexLatch','mReplayDirector.miCameraType')]:s=re.sub(r'\b'+before+r'\b',after,s)
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
p=ROOT/files[1];s='''#include "GameSource/Director/BrnDirectorModule.h"

namespace BrnDirector
{
// ARTIST827E5210: true base/member construction installs the module/debug
// vptrs and builds the resource manager, world-map heads, six post offices,
// MainDirector and the complete ReplayDirector through their genuine types.
DirectorModule::DirectorModule()
{
}
}
''';p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
# Update only the successor probe; the initial layout source/report stay immutable.
s=(ROOT/'.native-probes/game-module-layout.cpp').read_text()
s=s.replace('offsetof(BrnDirector::DirectorModule, mCamera)', 'offsetof(BrnDirector::DirectorModule, mReplayDirector)+offsetof(BrnDirector::ReplayDirector, mLastCamera)')
s=s.replace('offsetof(BrnDirector::DirectorModule, miPostGuiCarIndexLatch)', 'offsetof(BrnDirector::DirectorModule, mReplayDirector)+offsetof(BrnDirector::ReplayDirector, miCameraType)')
s=s.replace('offsetof(BrnDirector::DirectorModule, mbReplayStartedThisFrame)', 'offsetof(BrnDirector::DirectorModule, mReplayDirector)+offsetof(BrnDirector::ReplayDirector, mbStartedThisFrame)')
(ROOT/'.native-probes/game-module-replay-layout.cpp').write_text(s)
