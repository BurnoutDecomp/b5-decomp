from pathlib import Path
import re
p=Path(r'D:/codex-native/game-module-sol/src/GameSource/Game/BrnGameModule.cpp');s=p.read_text(encoding='utf-8')
assert s.count('const Vector2 lPosition = {200.0f, 40.0f};')==1
s=s.replace('const Vector2 lPosition = {200.0f, 40.0f};','const Vector2 lPosition(200.0f, 40.0f);')
for name in ['xAxis','yAxis','zAxis','wAxis']:
 old='const Vector3 lAggregateValue = {'+', '.join('lrView.'+name+'.'+c for c in ['x','y','z','w'])+'};'
 assert s.count(old)==1
 s=s.replace(old,'const Vector3 lAggregateValue(lrView.'+name+'.mV);')
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
