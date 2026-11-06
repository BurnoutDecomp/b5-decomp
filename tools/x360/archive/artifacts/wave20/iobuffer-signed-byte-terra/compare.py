import struct,json,pathlib,hashlib
P=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/iobuffer-signed-byte-terra');orig=json.loads((P/'original.json').read_text()); fs={'LockForWrite':'0x82204a18','UnlockForWrite':'0x82204bb8','LockForRead':'0x82204c88','UnlockForRead':'0x82204d50','Destruct':'0x82216e60'}
def extract(path):
 b=path.read_bytes();_,n,_,ptr,ns,op,_=struct.unpack_from('<HHIIIHH',b);secs=[]
 for i in range(n):
  z=struct.unpack_from('<8sIIIIIIHHI',b,20+op+i*40);secs.append((z[3],z[4],z[5],z[7]))
 st=ptr+18*ns
 def name(a):
  if a[:4]==b'\0'*4:
   off=struct.unpack_from('<I',a,4)[0];j=b.index(b'\0',st+off);return b[st+off:j].decode(errors='replace')
  return a.rstrip(b'\0').decode(errors='replace')
 r={};i=0
 while i<ns:
  nme,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',b,ptr+i*18);name1=name(nme)
  for f in fs:
   if name1.startswith('?'+f+'@IOBuffer@CgsModule@@') and sec>0:
    size,raw,rp,rn=secs[sec-1]; data=b[raw:raw+size];rels=[struct.unpack_from('<IIH',b,rp+j*10) for j in range(rn)];r[f]=(data,rels,name1)
  i+=1+aux
 return r
res={}
for f,a in fs.items():
 o=bytes.fromhex(orig[a]['bytes']); res[f]={'original_size':len(o)}
 for v in ['before','after']:
  obj=next((P/v).glob('*.obj')); d,rels,n=extract(obj)[f]; mask={x[0]//4 for x in rels};ow=[o[i:i+4] for i in range(0,len(o),4)];dw=[d[i:i+4] for i in range(0,len(d),4)]; common=min(len(ow),len(dw)); pref=0
  for i in range(common):
   if i not in mask and ow[i]!=dw[i]: break
   pref+=1
  res[f][v]={'size':len(d),'raw_exact':d==o,'masked_exact':len(d)==len(o) and all(i in mask or ow[i]==dw[i] for i in range(common)),'masked_prefix_words':pref,'relocation_words':len(mask),'identical_unmasked_words_same_position':sum(ow[i]==dw[i] for i in range(common) if i not in mask),'compared_words':sum(i not in mask for i in range(common)),'first_unmasked_original':ow[pref].hex() if pref<common else None,'first_unmasked_object':dw[pref].hex() if pref<common else None,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()}
(P/'comparison.json').write_text(json.dumps(res,indent=2));print(json.dumps(res,indent=2))
