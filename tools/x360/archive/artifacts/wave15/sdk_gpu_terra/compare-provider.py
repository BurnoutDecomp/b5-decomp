import sys, struct, json, pathlib, hashlib
sys.path.insert(0, r'D:\Reverse\BP-Decomp_Workflow\tools\re')
import x360rd
assert x360rd.u32(0x82815570) == 0x7c8a2378 and x360rd.u32(0x82f264a8) == 4
p = pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave15\sdk_gpu_terra')
functions = json.loads((p/'provider-functions.json').read_text())
addresses = [0x82947498, 0x82947518, 0x82947608, 0x829442b0, 0x82944348]
results = []
for f, addr in zip(functions, addresses):
 b=(p/f['member']).read_bytes(); sh=20+(f['section']-1)*40
 size, raw=struct.unpack_from('<II', b, sh+16); original=x360rd.rd(addr,size); sdk=b[raw:raw+size]; masks={}
 for reloc in f['relocations']:
  typ=int(reloc['type'],16); off=int(reloc['offset'],16)
  if typ==6: masks[off]=0xfc000003
  elif typ in (0x10,0x11): masks[off]=0xffff0000
 bad=[]
 for offset in range(0,size,4):
  x=int.from_bytes(original[offset:offset+4],'big'); y=int.from_bytes(sdk[offset:offset+4],'big'); mask=masks.get(offset,0xffffffff)
  if x&mask != y&mask: bad.append({'offset':hex(offset),'artist':f'{x:08x}','sdk':f'{y:08x}','mask':f'{mask:08x}'})
 results.append({'symbol':f['symbol'],'artist_address':hex(addr),'size':size,'artist_sha256':hashlib.sha256(original).hexdigest(),'sdk_sha256':hashlib.sha256(sdk).hexdigest(),'relocation_masked_words':len(masks),'nonrelocation_differences':bad})
print(json.dumps({'sentinels':{'82815570':'7c8a2378','82f264a8':'00000004'},'comparison':results},indent=2))
