import argparse,struct,pathlib,hashlib,json
parser=argparse.ArgumentParser(); parser.add_argument('--friends',required=True); parser.add_argument('--owner',required=True); parser.add_argument('--crt',required=True); parser.add_argument('--out',required=True); args=parser.parse_args()
def coff(b):
 machine,n,ts,sp,ns,opt,ch=struct.unpack_from('<HHIIIHH',b,0)
 if machine != 0x1f2: raise ValueError('not X360 PPC COFF')
 stringbase=sp+ns*18
 def nm(raw):
  if raw[:4]==bytes(4):
   start=stringbase+struct.unpack_from('<I',raw,4)[0]; return b[start:b.index(b'\0',start)].decode('ascii')
  return raw.rstrip(b'\0').decode('ascii')
 syms={}; i=0
 while i<ns:
  raw,value,sec,typ,storage,aux=struct.unpack_from('<8sIhHBB',b,sp+i*18); syms[i]=(nm(raw),value,sec,storage); i+=1+aux
 sections={}
 for i in range(n):
  raw,vs,va,size,data,rel,lin,nr,nl,fl=struct.unpack_from('<8sIIIIIIHHI',b,20+opt+i*40); rs=[]
  for k in range(nr):
   off,si,rt=struct.unpack_from('<IIH',b,rel+10*k); rs.append((off,rt,syms[si][0]))
  sections[i+1]=(b[data:data+size],rs)
 return syms,sections
def find(symbol, syms, secs):
 found=[x for x in syms.values() if x[0]==symbol and x[2]>0]
 if len(found)!=1: raise ValueError('symbol not uniquely defined: '+symbol)
 _,value,sec,_=found[0]; return secs[sec],value
worker='?NetConnX360CloseWorker@@YAXPAUNetConnX360State@@@Z'; wrapper='?NetConnX360CloseThread@@YAKPAX@Z'
fs,fc=coff(pathlib.Path(args.friends).read_bytes()); os,oc=coff(pathlib.Path(args.owner).read_bytes()); (w,wr),wo=find(worker,fs,fc); (t,tr),to=find(wrapper,fs,fc); (o,orr),oo=find('NetConnStartup',os,oc)
assert wo==to==0
expected_worker=bytes.fromhex('7d8802a6 4bfffffd 9421ff70 3f830001 3fa30001 3fe30001 3b9c3428 3bbd3424 3bff3408 3b40ffff 7fe3fb78 4bffffd5 7fe3fb78 83dd0000 837c0000 4bffffc5 2f1effff 419a0020 7fc3f378 4bffffb5 7fe3fb78 4bffffad 7fe3fb78 935d0000 4bffffa1 2f1b0000 409a0010 38600010 4bffff91 4bffffb4 7fe3fb78 4bffff85 39600000 7fe3fb78 917c0000 4bffff75 38210090 4bffff6c')
expected_wrapper=bytes.fromhex('7d8802a6 9181fff8 9421ffa0 90610074 80610074 4bffffed 80210000 8181fff8 7d8803a6 4e800020 38210060 8181fff8 7d8803a6 4e800020')
assert w==expected_worker, 'worker instruction bytes differ; re-audit all paths'
assert t==expected_wrapper, 'wrapper frame, argument forwarding or exit differs'
assert (140,6,'RtlLeaveCriticalSection') in wr and (148,6,'__restgprlr_26') in wr
assert tr==[(20,6,worker)], 'wrapper does not call exact worker via REL24'
assert (596,16,wrapper) in orr and (612,17,wrapper) in orr and (628,6,'CreateThread') in orr, 'startup entry relocations differ'
lib=pathlib.Path(args.crt).read_bytes(); assert lib[:8]==b'!<arch>\n'; pos=68; count=struct.unpack_from('>I',lib,pos)[0]; offsets=struct.unpack_from('>'+str(count)+'I',lib,pos+4); p=pos+4+count*4; member=None
for i in range(count):
 e=lib.index(b'\0',p); name=lib[p:e]; p=e+1
 if name==b'__restgprlr_26':
  h=offsets[i]; size=int(lib[h+48:h+58]); member=lib[h+60:h+60+size]; break
assert member is not None
ss,sc=coff(member); (code,rels),value=find('__restgprlr_26',ss,sc)
expected_restore=bytes.fromhex('eb41ffc8 eb61ffd0 eb81ffd8 eba1ffe0 ebc1ffe8 ebe1fff0 8181fff8 7d8803a6 4e800020')
assert code[value:value+len(expected_restore)]==expected_restore, 'SDK restore thunk differs'
report={'ok':True,'friends_sha256':hashlib.sha256(pathlib.Path(args.friends).read_bytes()).hexdigest(),'owner_sha256':hashlib.sha256(pathlib.Path(args.owner).read_bytes()).hexdigest(),'crt_sha256':hashlib.sha256(lib).hexdigest(),'crt_restore_member_sha256':hashlib.sha256(member).hexdigest(),'worker_size':len(w),'wrapper_size':len(t),'status':'compile-time codegen and relocation check only; async unwind and linked-image binding require separate verification'}
pathlib.Path(args.out).write_text(json.dumps(report,indent=2)); print(json.dumps(report,indent=2))
