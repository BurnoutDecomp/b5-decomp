from pathlib import Path
import re, subprocess
root=Path.cwd()
def load(p):return subprocess.check_output(['git','show','HEAD:'+p]).decode('utf8')
def save(p,s):root.joinpath(p).write_text(s,encoding='utf8',newline='\n')
def cutlines(s,ranges):
 L=s.splitlines(True); bad={i-1 for a,b in ranges for i in range(a,b+1)};return ''.join(l for i,l in enumerate(L) if i not in bad)
p='src/GameSource/Director/BrnDirectorDevTools.cpp';s=load(p)
s=re.sub(r'const Vector3 lAggregateValue = \{ ([^{}]+), 0.0f \};',r'const Vector3 lAggregateValue(\1);',s)
for axis in ['wAxis','zAxis']:
 for lane in 'xyz':s=s.replace('lrTransform.'+axis+'.'+lane,'static_cast<f32>(lrTransform.'+axis+'.Get'+lane.upper()+'())')
save(p,s)
p='src/GameSource/Director/BrnMainDirector.cpp';s=load(p)
s=cutlines(s,[(80,83),(98,104),(169,202),(895,895),(950,955),(1119,1141),(1365,1366),(1385,1385),(1394,1394),(1517,1528),(1640,1647),(1752,1766),(1801,1804),(1858,1858),(2078,2079),(2413,2418),(2525,2528),(3194,3194),(3235,3236),(3248,3248),(3367,3367),(3436,3438),(3449,3449),(3455,3457)])
# Full-register subtraction is already the genuine native Vector3 operator.
s=s.replace('const Vector3 lPlayerToLight = { lLightPos.x - lPlayerPos.x, lLightPos.y - lPlayerPos.y,\n                                                 lLightPos.z - lPlayerPos.z, lLightPos.w - lPlayerPos.w };','const Vector3 lPlayerToLight = lLightPos - lPlayerPos;')
s=re.sub(r'const Vector3 lAxis = \{([^{}]+),\s*0.0f \};',r'const Vector3 lAxis(\1);',s)
for lane in 'xyz':s=s.replace('lrCamera.mTransform.wAxis.'+lane,'static_cast<f32>(lrCamera.mTransform.wAxis.Get'+lane.upper()+'())')
s=s.replace('Vector2 lv2Position = { lfX, lfY, 0.0f, 0.0f };','Vector2 lv2Position(lfX, lfY);')
# Vector2 remains the native scalar pair.
# ARTIST required inputs are directly dereferenced in these four entry points.
s=s.replace('        if (lpInput == 0)\n            return;\n\n','')
s=s.replace('        if (lpVehicleInput == 0)\n            return;\n\n','')
s=s.replace('if (lpControl == 0 || !lpControl->IsGameTalkRefreshRequested())','if (!lpControl->IsGameTalkRefreshRequested())')
save(p,s)
p='src/GameSource/Game/BrnGameModule.cpp';s=load(p)
s=s.replace('const Vector2 lPosition = {200.0f, 40.0f};','const Vector2 lPosition(200.0f, 40.0f);')
for axis in ['xAxis','yAxis','zAxis','wAxis']:
 s=s.replace('const Vector3 lAggregateValue = {'+', '.join('lrView.'+axis+'.'+lane for lane in 'xyzw')+'};','const Vector3 lAggregateValue(lrView.'+axis+'.mV);')
save(p,s)
p='src/GameSource/Game/GameBridgeWorldToX.cpp';s=load(p)
s=cutlines(s,[(130,131),(168,173),(281,284),(413,441),(539,552),(636,644)])
s=s.replace('        if (lpDirectorInput == 0 || lpWorldOutput == 0)\n        {\n            return;\n        }\n\n','')
s=s.replace('const Vector3 lAggregateValue = { -lHalfExtent.x, -lHalfExtent.y, -lHalfExtent.z, -lHalfExtent.w };','const Vector3 lAggregateValue = rw::math::vpu::Negate(lHalfExtent);')
s=s.replace('const Vector3 lAggregateValue = { 0.0f, 1.0f, 0.0f, 0.0f };','const Vector3 lAggregateValue(0.0f, 1.0f, 0.0f);')
save(p,s)
p='src/GameSource/Physics/BrnPhysicsModuleGameActions.cpp';s=load(p)
s=cutlines(s,[(153,209),(219,229),(242,242),(258,258),(348,385),(489,498),(556,556),(580,590)])
save(p,s)
