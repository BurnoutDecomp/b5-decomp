import hashlib,pathlib
repo=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a')
rel='src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidget.cpp'
p=repo/rel; old=p.read_bytes()
inc=b'#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidget.h"\r\n'
newinc=inc+b'#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"\r\n'
if old.count(inc)!=1: raise SystemExit('include marker mismatch')
data=old.replace(inc,newinc,1)
for method in ('GetPalette','GetMetrics','Get2DRenderer'):
    before=b'return GetUI().'+method.encode()+b'();'
    after=b'return DebugManager::GetInstance()->GetUI().'+method.encode()+b'();'
    if data.count(before)!=1: raise SystemExit('body marker mismatch '+method)
    data=data.replace(before,after,1)
p.write_bytes(data)
print('pre_sha256='+hashlib.sha256(old).hexdigest())
print('post_sha256='+hashlib.sha256(data).hexdigest())
