import struct,pathlib
p=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r173-state-descriptor\CgsStateManager.obj');b=p.read_bytes();mach,nsec,tm,sp,ns,opt,ch=struct.unpack_from('<HHIIIHH',b);st=sp+ns*18
def name(raw):
 if raw[:4]==b'\0'*4: return b[st+struct.unpack_from('<I',raw,4)[0]:].split(b'\0',1)[0].decode(errors='replace')
 if raw.startswith(b'/'): return b[st+int(raw[1:].split(b'\0',1)[0]):].split(b'\0',1)[0].decode(errors='replace')
 return raw.split(b'\0',1)[0].decode(errors='replace')
syms={}; i=0
while i<ns:
 off=sp+18*i; n=name(b[off:off+8]); val,sec,typ,cls,aux=struct.unpack_from('<IhHBB',b,off+8);syms[i]=(n,val,sec);i+=1+aux
for i,(n,val,sec) in syms.items():
 if n.startswith('?sTypeInfo@StateManager@Logic@CgsSound@@'):
  print('symbol',i,n,'value',val,'section',sec);off=20+40*(sec-1);sh=b[off:off+40]; print('section',name(sh[:8]),struct.unpack_from('<IIIIIIHHI',sh,8)); vsize,vaddr,size,raw,rel,line,nrel,nline,flags=struct.unpack_from('<IIIIIIHHI',sh,8); print('data',b[raw+val:raw+val+16].hex());
  for j in range(nrel):
   va,si,ty=struct.unpack_from('<IIH',b,rel+10*j)
   if val<=va<val+16: print('reloc',hex(va-val),hex(ty),si,syms.get(si))
for i in range(nsec):
 sh=b[20+40*i:20+40*(i+1)]; n=name(sh[:8]);vsize,vaddr,size,raw,rel,line,nrel,nline,flags=struct.unpack_from('<IIIIIIHHI',sh,8)
 if n=='.CRT$XCU':
  print('CRT section',i+1,'size',size,'raw',b[raw:raw+size].hex())
  for j in range(nrel):
   va,si,ty=struct.unpack_from('<IIH',b,rel+10*j); print('CRT reloc',hex(va),hex(ty),syms.get(si))
