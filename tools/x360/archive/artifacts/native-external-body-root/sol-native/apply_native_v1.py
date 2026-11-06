from pathlib import Path
import hashlib
OUT=Path(__file__).resolve().parent;p=Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root/src/GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.cpp')
assert hashlib.sha256(p.read_bytes()).hexdigest()=='14f63f33ae5703cc46684b58341ded2cbea25fe9a974205abcfc045b0c72d078'
old=p.read_text();a=old.index('#include <cstddef>');b=old.index('namespace BrnPhysics\n{',a);guards=old[a:b]
header='''#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// Native ARTIST ExternalPhysicsBody. Real XDK VMX operations retain all
// lanes, ordered fused arithmetic, reciprocal refinements and store order.
// Root's PC-only state removal and the native120-byte class gates remain.
'''
new=header+guards+(OUT/'native-body.txt').read_text();p.write_bytes(new.replace('\r\n','\n').replace('\n','\r\n').encode());print(hashlib.sha256(p.read_bytes()).hexdigest())
