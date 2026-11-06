from pathlib import Path
ROOT=Path(r'D:/codex-native/game-module-sol');p=ROOT/'src/GameSource/Director/BrnDirectorModule.h';s=p.read_text(encoding='utf-8');needle='class DirectorModule : public CgsModule::ModuleSingleBuffered\n{\npublic:'
if needle not in s:
 import re
 m=re.search(r'class DirectorModule\s*:\s*public CgsModule::ModuleSingleBuffered\s*\{\s*public:',s);assert m;needle=m[0]
assert '    DirectorModule();'not in s;s=s.replace(needle,needle+'\n    DirectorModule();',1);p.write_bytes(s.replace('\n','\r\n').encode())
p=ROOT/'src/GameSource/Director/BrnDirectorModule.cpp';s=p.read_text(encoding='utf-8').replace('ARTIST827E5210','ARTIST827E5048');p.write_bytes(s.replace('\n','\r\n').encode())
