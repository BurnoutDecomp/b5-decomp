import struct,hashlib,json,pathlib
base=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r27-loading-terra-relative'); generated=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\triangle_cache_luna_3534\build\x360\generated'); obj=next((base/'objects').glob('*.obj')); b=obj.read_bytes(); machine,n,_,symoff,count,opt,_=struct.unpack_from('<HHIIIHH',b); assert machine==0x1f2
sections=[struct.unpack_from('<8sIIIIIIHHI',b,20+opt+40*i) for i in range(n)]; data=next((i+1,x) for i,x in enumerate(sections) if x[0].rstrip(b'\0')==b'.data'); index,row=data; assert row[9]&0x80000000
strings=symoff+count*18; symbols={}; i=0
while i<count:
 raw,val,sec,typ,storage,aux=struct.unpack_from('<8sIhHBB',b,symoff+18*i);name=(b[strings+struct.unpack_from('<I',raw,4)[0]:].split(b'\0',1)[0] if raw[:4]==b'\0'*4 else raw.rstrip(b'\0')).decode('ascii','replace');symbols[name]=(val,sec,storage);i+=1+aux
manifest=json.loads((generated/'BrnLoadingScreenTextures.manifest.json').read_text()); received={}
for name,entry in manifest.items():
 key='?'+name+'@BrnGame@@3PAEA'; offset,sec,storage=symbols[key]; assert sec==index and storage==2 and offset%8==0
 size=entry['size']; raw=b[row[4]+offset:row[4]+offset+size];digest=hashlib.sha256(raw).hexdigest();assert digest==entry['sha256'];received[name]={'section_offset':offset,'size':size,'sha256':digest}
print(json.dumps({'machine':hex(machine),'section':'.data writable','arrays':received,'obj_sha256':hashlib.sha256(b).hexdigest(),'include_sha256':hashlib.sha256((generated/'BrnLoadingScreenTextures.generated.inc').read_bytes()).hexdigest(),'manifest_sha256':hashlib.sha256((generated/'BrnLoadingScreenTextures.manifest.json').read_bytes()).hexdigest()},indent=2))

