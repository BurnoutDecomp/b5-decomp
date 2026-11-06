import hashlib,pathlib,re
repo=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a')
p=repo/'src/GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.cpp'
data=p.read_bytes(); before=hashlib.sha256(data).hexdigest()
pattern=re.compile(rb'(?m)^#include <xtl\.h>[ \t]*//[^\r\n]*(\r?)$')
data,n=pattern.subn(rb'#include <xtl.h>\1',data)
if n!=1: raise SystemExit('expected one xtl include with trailing comment, got %d' % n)
for old,new in ((b'CgsDev::Assert::gAssertMutex.Enter();',b'CgsDev::Assert::BeginAssert();'),(b'CgsDev::Assert::gAssertMutex.Leave();',b'CgsDev::Assert::EndAssert();')):
    if data.count(old)!=1: raise SystemExit('assert API marker mismatch '+old.decode())
    data=data.replace(old,new,1)
p.write_bytes(data)
print('pre_sha256='+before)
print('post_sha256='+hashlib.sha256(data).hexdigest())
