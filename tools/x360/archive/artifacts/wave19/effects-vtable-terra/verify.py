import sys,struct,re,json,hashlib,pathlib
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re')
import x360rd
r=lambda a:struct.unpack('>I',x360rd.rd(a,4))[0]
assert r(0x827e361c)==0x3d60820d and r(0x827e3624)==0x396b0d48 and r(0x827e3628)==0x917f0000
assert [r(0x820d0d48+4*i) for i in range(16,20)]==[0x8229e690,0x8229ec28,0x8227fe10,0x8227fe88]
assert r(0x8240c328)==0x816b004c
p=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave19\effects-vtable-terra\objects\EffectsModule-87aee4b9.asm')
s=p.read_text(errors='replace').splitlines(); i=next(i for i,v in enumerate(s) if v.startswith('??_7EffectsModule@BrnEffects@@6B@ DD'))
t=s[i:i+20]; assert len(t)==20
for i,name in enumerate(['?Prepare@EffectsModule@','?Update@EffectsModule@','?PreRenderUpdate@EffectsModule@','?DispatchThreadUpdate@EffectsModule@'],16): assert name in t[i]
o=p.with_suffix('.obj'); b=o.read_bytes(); assert struct.unpack_from('<H',b)[0]==498
print(json.dumps({'raw_constructor_vtable_caller_asserts':'pass','emitted_vtable_offsets':{'0x40':'Prepare','0x44':'Update','0x48':'PreRenderUpdate','0x4c':'DispatchThreadUpdate'},'obj_sha256':hashlib.sha256(b).hexdigest(),'consumer_caveat':'true renderer compile needs additional provider declarations'},indent=2))
