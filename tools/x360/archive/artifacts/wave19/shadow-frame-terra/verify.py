import sys,struct,hashlib,json,pathlib
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re')
import x360rd
read=lambda a: struct.unpack('>I',x360rd.rd(a,4))[0]
assert read(0x827ED758)==0x7d8802a6 and read(0x827ED764)==0x4ba8920d
assert read(0x827ED7a8)==0x7d8802a6 and read(0x827ED7b4)==0x4ba891bd
assert read(0x827ED7e8)==0x814b1624 and read(0x827ED7f0)==0x914b1624 and read(0x827ED800)==0x4e800020
for a in (0x8240e22c,0x8240e27c):
 w=read(a); assert w>>26==18 and w&3==1
 delta=w&0x03fffffc
 if delta&0x02000000: delta-=0x04000000
 assert (a+delta)&0xffffffff==0x827ed7a8
p=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave19\shadow-frame-terra\objects\shadowingdevice-8b4d0d55.obj')
b=p.read_bytes(); assert struct.unpack_from('<H',b)[0]==498
syms=['?FrameBegin@Device@shadow@@SA_NXZ','?FrameEnd@Device@shadow@@SAXXZ','?gDeviceFrameCounter83271624@renderengine@@3IA']
assert all(s.encode() in b for s in syms)
print(json.dumps({'sentinels':'pass','render_branches':['8240E22C -> 827ED7A8','8240E27C -> 827ED7A8'],'object_machine':498,'object_sha256':hashlib.sha256(b).hexdigest(),'symbols':syms},indent=2))
