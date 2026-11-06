from pathlib import Path
ROOT=Path(r'D:/codex-native/game-module-sol');OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol')
for rel in ['src/GameSource/Director/Utils/BrnDirectorVehicleTracker.h','src/GameSource/Director/Utils/BrnDirectorVehicleTracker.cpp','src/GameSource/Director/Camera/BrnCollisionPolicy.h']:
 p=ROOT/rel;(OUT/(p.name+'.before')).write_bytes(p.read_bytes())
p=ROOT/'src/GameSource/Director/Utils/BrnDirectorVehicleTracker.h';s=p.read_text(encoding='utf-8')
s=s.replace('#include "types.hpp"','#include "types.hpp"\n#include "GameSource/GameState/BrnGameStateSharedIO.h"',1)
a=s.index('// FLAG: minimal slice of the per-car score block');a=s.rfind('// ----------------------------------------------------------------------------',0,a)
b=s.index('// FLAG: minimal slices of the two director-IO inputs',a);b=s.rfind('// ----------------------------------------------------------------------------',a,b)
s=s[:a]+'''// The tracker owns the actual score record. Its native C++ constructor is
// visible inside ReplayDirector827E2080 (this+1FC0) and MainDirector's ctor.
'''+s[b:]
s=s.replace('    CarScoreData   mScoreData;', '    BrnGameState::GameStateModuleIO::CarScoreData mScoreData;')
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
p=ROOT/'src/GameSource/Director/Utils/BrnDirectorVehicleTracker.cpp';s=p.read_text(encoding='utf-8');s=s.replace('mScoreData = CarScoreData();','mScoreData.ClearData();');p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
p=ROOT/'src/GameSource/Director/Camera/BrnCollisionPolicy.h';s=p.read_text(encoding='utf-8');assert s.count('    virtual ~CollisionPolicy() {}')==1
s=s.replace('    virtual ~CollisionPolicy() {}','    // ARTIST8200A150/8200A158 are adjacent two-entry policy tables.\n    // No destructor slot precedes their Generate/Process entries.')
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
