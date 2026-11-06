import sys,hashlib,json,pathlib,struct
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re'); import x360rd
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r27-loading-terra'); x360rd._init()
u=lambda a: struct.unpack('>I',x360rd.rd(a,4))[0]
assert u(0x823ce2ec)==0x3d6082ce
# Independently checked byte patterns in the constructor and first words of every texture.
checks={0x82cdc078:0x05000000,0x82ce0078:0x0500ffff,0x82ee0078:0x05000000,0x82f20078:0x05000000}
for a,v in checks.items(): assert u(a)==v
items=[('gpLoadingScreenTextureArrow',0x82cdc078,0x4000),('gpLoadingScreenTextureCar',0x82ce0078,0x200000),('gpLoadingScreenTextureText',0x82ee0078,0x40000),('gpLoadingScreenTextureBox',0x82f20078,0x4000)]
assert all(items[i][1]+items[i][2]==items[i+1][1] for i in range(3))
def read(a,n):
 for start,end,cumulative in x360rd._segs:
  if start<=a and a+n<=end:
   idx=cumulative+a-start
   return bytes(x360rd._m[idx:idx+n]) if x360rd._base is None else bytes(x360rd._m[x360rd._base+idx*4:x360rd._base+(idx+n)*4:4])
 raise ValueError('segment boundary')
manifest={}; cpp=out/'LoadingScreenEmbeddedTextures.cpp'
with cpp.open('w',encoding='ascii',newline='\n') as f:
 f.write('// Generated from the ARTIST image; mutable because the car array is reused as scratch.\nnamespace BrnGame {\n')
 for name,addr,size in items:
  data=read(addr,size); assert len(data)==size and data[:32]==x360rd.rd(addr,32) and data[-32:]==x360rd.rd(addr+size-32,32)
  manifest[name]={'artist_address':hex(addr),'size':size,'sha256':hashlib.sha256(data).hexdigest()}
  f.write('__declspec(align(16)) unsigned char '+name+'['+str(size)+'] = {\n')
  for i in range(0,size,32): f.write(','.join('0x%02x'%b for b in data[i:i+32])+',\n')
  f.write('};\n')
 f.write('}\n')
(out/'image-evidence.json').write_text(json.dumps({'data':manifest},indent=2))
print(json.dumps(manifest,indent=2))
