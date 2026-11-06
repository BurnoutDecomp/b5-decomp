from pathlib import Path
import hashlib
p=Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root/src/GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.cpp');assert hashlib.sha256(p.read_bytes()).hexdigest()=='638b1fc901ed46ccd6f07359418d4d32baf5e8f841d438e6396b663d3bfb123a'
s=p.read_text();marker='        // Primary image words82014AF0/AE0/AD0/AC0, in the original order.'
helper='''        // Tagged local-force/impulse helpers: ARTIST825A17EC/19F4/1AA4
        // start with Y's rounded product, then fuse X and Z in that order.
        inline __vector4 RotateTaggedToWorld(const Matrix44Affine& transform, __vector4 value)
        {
            __vector4 result = __vmulfp(transform.yAxis.mV, __vspltw(value, 1));
            result = __vmaddfp(transform.xAxis.mV, __vspltw(value, 0), result);
            return __vmaddfp(transform.zAxis.mV, __vspltw(value, 2), result);
        }

'''
assert s.count(marker)==1;s=s.replace(marker,helper+marker)
for name in ['AddLocalForce','AddLocalImpulse','GetImpulsesFromLocalImpulse']:
    start=s.index('    void ExternalPhysicsBody::'+name+'(');end=s.index('\n    }',start)+6
    chunk=s[start:end];assert chunk.count('RotateToWorld(')==2;s=s[:start]+chunk.replace('RotateToWorld(','RotateTaggedToWorld(')+s[end:]
p.write_bytes(s.replace('\r\n','\n').replace('\n','\r\n').encode());print(hashlib.sha256(p.read_bytes()).hexdigest())
