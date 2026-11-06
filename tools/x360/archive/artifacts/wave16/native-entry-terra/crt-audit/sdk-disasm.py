import pathlib,struct,capstone
base=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave16\native-link-providers-terra'); ns={};exec((base/'inventory-r19.py').read_text().split('records=[]; failures=[]')[0],ns)
sdk=ns['sdk']; targets={'??0exception@std@@QAA@ABV01@@Z','??1exception@std@@UAA@XZ','?what@exception@std@@UBAPBDXZ','??_Gexception@std@@UAAPAXI@Z'}
cs=capstone.Cs(capstone.CS_ARCH_PPC,capstone.CS_MODE_32|capstone.CS_MODE_BIG_ENDIAN)
for lib,member in [('libcMT.lib','new.obj'),('libcpMT.lib','string.obj')]:
 for name,b in ns['archive'](sdk/lib):
  if not name.lower().endswith('\\'+member):continue
  machine,nsec,ts,so,count,opt,flags=struct.unpack_from('<HHIIIHH',b);st=b[so+count*18:];i=0
  while i<count:
   raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',b,so+i*18)
   if raw[:4]==b'\0'*4:
    offset=struct.unpack_from('<I',raw,4)[0];end=st.find(b'\0',offset); sym=st[offset:end].decode('latin1') if 4<=offset<len(st) and end>=0 else ''
   else:sym=raw.rstrip(b'\0').decode('latin1')
   if sym in targets and cl==2 and sec>0:
    h=struct.unpack_from('<8sIIIIIIHHI',b,20+opt+(sec-1)*40);rawbytes=b[h[4]:h[4]+h[3]]
    print('\n',lib,name,sym,'section',sec,'bytes',len(rawbytes))
    for ins in cs.disasm(rawbytes,0):print('%04x %-10s %s'%(ins.address,ins.mnemonic,ins.op_str))
   i+=1+aux
