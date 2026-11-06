from pathlib import Path
import hashlib, json, re, subprocess

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
paths = ['src/GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.cpp',
         'src/GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h',
         'src/GameSource/Physics/BrnPhysicsModuleUpdateFunctions.cpp',
         'src/GameSource/Physics/DeformationManager/BrnDeformationManager_Contacts.cpp',
         'src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.cpp']
before = {p:(ROOT/p).read_text() for p in paths}
assert all(subprocess.check_output(['git','rev-parse','HEAD:'+p],cwd=ROOT,text=True).strip()
    == subprocess.check_output(['git','rev-parse','d81ba6aa1ab01fa5d321dea3f0ae45aeb5d27647:'+p],cwd=ROOT,text=True).strip() for p in paths)

def cut(s, start, end):
    assert s.count(start) == 1, start
    a=s.index(start); b=s.index(end,a)
    return s[:a]+s[b:]

def end_brace(s, start):
    assert s[start]=='{'
    depth=0; state='code'; i=start
    while i<len(s):
        a=s[i]; b=s[i:i+2]
        if state=='line':
            if a=='\n':state='code'
        elif state=='block':
            if b=='*/':state='code';i+=1
        elif state in ('"', "'"):
            if a=='\\':i+=1
            elif a==state:state='code'
        elif b=='//':state='line';i+=1
        elif b=='/*':state='block';i+=1
        elif a in ('"',"'"):state=a
        elif a=='{':depth+=1
        elif a=='}':
            depth-=1
            if not depth:return i+1
        i+=1
    raise AssertionError('unbalanced block')

def cut_block(s, marker, body_token):
    assert s.count(marker)==1,marker
    a=s.index(marker); a=s.rfind('\n',0,a)+1
    body=s.index(body_token,a); brace=s.index('{',body)
    b=end_brace(s,brace)
    if s[b:b+1]=='\n':b+=1
    return s[:a]+s[b:]

s=before[paths[0]]
a=s.index('#include <cstdlib>'); b=s.index('#endif',a)+len('#endif\n')
s=s[:a]+s[b:]
s=cut_block(s,'// ---- [bank] PC bring-up instrument','        {')
s=cut(s,'    // ---- [crash-response] PC bring-up instrument', '    void ExternalPhysicsBody::CalculateNewVelocity')
s=cut_block(s,'// [crash-response] -- see the banner', 'if ( this == gpCrashResponseDiagBody')
s=cut(s,'        // [dv] the drain witness','        // Linear:')
s=cut_block(s,'        if ( lbDvDrain )','        if ( lbDvDrain )')
after={paths[0]:s}
s=before[paths[1]]
s,n=re.subn(r'^        const (?:Matrix33|Vector3)&\s+Diag(?:WorldInverseInertia|TotalLinearImpulse|TotalAngularImpulse|TotalLinearForce|TotalTorque)\(\) const[^\n]*\n','',s,flags=re.M)
assert n==5,n
after[paths[1]]=s
s=before[paths[2]]
s=cut(s,'    // ---- [dv] the one-step velocity witness','    namespace Vehicle')
a=s.index('    namespace Vehicle\n    {'); b=end_brace(s,s.index('{',a))
assert 'guKerbProbeFrame' in s[a:b] and s[a:b].count(';')==1
s=s[:a]+s[b:]
s,n=re.subn(r'^\s*DvWitness(?:BeginStep|Mark|EndStep)\([^\n]*\);[^\n]*\n','\n',s,flags=re.M)
assert n==14,n
after[paths[2]]=s
s=before[paths[3]]
s=cut(s,'// [wedge] (FX-WEDGEVEL) -- DEFINED','namespace BrnPhysics\n{\nnamespace Deformation')
s=cut_block(s,'// ---- [wedge-pen] (FX-WEDGEVEL)','if ( DvWedgeStepRecording()')
after[paths[3]]=s
s=before[paths[4]]
s=cut(s,'// [wedge] PC bring-up instrument (FX-WEDGEVEL)','namespace Deformation')
s=cut_block(s,'// ---- [wedge-imp] (FX-WEDGEVEL)','if ( DvWedgeStepRecording()')
after[paths[4]]=s

for p,s in after.items():
    assert not any(x in s for x in ['gpDvWatchBody','gpCrashResponseDiagBody','gpcDvDrainTag','DvWitness','DvWedge','_ReturnAddress','__ImageBase']),p
    backup=HERE/'before'/p
    backup.parent.mkdir(parents=True,exist_ok=True)
    assert not backup.exists(),backup
    backup.write_text(before[p],encoding='utf-8')
for p,s in after.items():
    (ROOT/p).write_text(s,encoding='utf-8')
sha=lambda s:hashlib.sha256(s.encode()).hexdigest()
receipt={'state':'PRIVATE_PC_PROBE_REMOVAL_UNCOMPILED','base':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
         'paths':{p:{'before_sha256':sha(before[p]),'after_sha256':sha(after[p]),'lines_removed':len(before[p].splitlines())-len(after[p].splitlines())}for p in paths},
         'scope':'Remove PC bank/crash/dv/wedge diagnostic providers, their direct declarations/calls, unused diagnostic member accessors and Windows return-address/image-base dependency. No native math or engine operation edited. Other independent PC probes/native compile debt remain.'}
(HERE/'pc-removal-source.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'paths':len(paths),'lines_removed':sum(x['lines_removed']for x in receipt['paths'].values())}))
