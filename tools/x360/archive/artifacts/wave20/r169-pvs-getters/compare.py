import json,re,struct,hashlib,pathlib,sys
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re'); import x360rd
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r169-pvs-getters'); asm=(out/'BrnPVSModule.asm').read_text(errors='replace'); obj=(out/'BrnPVSModule.obj').read_bytes()
specs=[('GetInputInterface',0x822baf78,73,122,'lpInputBuffer','GetInputStructure'),('GetOutputInterface',0x822baff0,82,130,'lpOutputBuffer','GetOutputStructure'),('GetGameDataRequestInt',0x822bb068,82,138,'lpOutputBuffer','GetOutputStructure')]; string_offsets=[0x24,0x2c,0x30,0x34,0x44,0x4c,0x50,0x54]; branch_offsets=[0x10,0x20,0x38,0x3c,0x40,0x58,0x5c]; found=[]
def strptr(words,hi,lo):
 h=words[hi//4]&0xffff; l=words[lo//4]&0xffff; l=l-0x10000 if l&0x8000 else l;return ((h<<16)+l)&0xffffffff
def cstr(addr): return x360rd.rd(addr,256).split(b'\0',1)[0]
for name,addr,line1,line2,msg,getter in specs:
 pattern=r'^\?'+name+r'@PVSModule@BrnWorld@@[^\n]* PROC NEAR'; m=re.search(pattern,asm,re.M); assert m,name; chunk=asm[m.start():asm.index(' ENDP',m.start())]; pairs=re.findall(r'^\s*([0-9a-f]{5})\s+([0-9a-f]{8})\s+([^\r\n]*)',chunk,re.M); actual=[int(x[1],16) for x in pairs]; orig=list(struct.unpack('>30I',x360rd.rd(addr,120))); assert len(actual)==30
 diffs=[]
 for i,(x,y) in enumerate(zip(actual,orig)):
  offset=i*4
  if offset in branch_offsets: assert (x&0xfc000003)==(y&0xfc000003); diffs.append({'offset':hex(offset),'kind':'branch relocation','listing':pairs[i][2]})
  elif offset in string_offsets: assert (x&0xffff0000)==(y&0xffff0000); diffs.append({'offset':hex(offset),'kind':'string relocation','listing':pairs[i][2]})
  else: assert x==y,(name,hex(offset),hex(x),hex(y))
 calls=[pairs[o//4][2] for o in branch_offsets]; assert getter in calls[0] and 'BeginAssert' in calls[1] and 'FireAssert' in calls[2] and 'EndAssert' in calls[3] and 'BeginAssert' in calls[4] and 'FireAssert' in calls[5] and 'EndAssert' in calls[6]
 expected=[b'..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleSingleBufferedTemplate.h',b'lpBuffer != NULL',b'd:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\entitymodules\\worldentitymodule\\PVSModule/BrnPVSModule.h',msg.encode()]; ptrs=[strptr(orig,0x24,0x2c),strptr(orig,0x30,0x34),strptr(orig,0x44,0x4c),strptr(orig,0x50,0x54)];
 for v,ptr in zip(expected,ptrs): assert cstr(ptr)==v,(name,hex(ptr),cstr(ptr),v); assert v+b'\0' in obj
 assert actual[0x28//4]&0xffff==line1 and actual[0x48//4]&0xffff==line2
 found.append({'function':name,'artist_address':hex(addr),'words':len(actual),'fixed_words_match':True,'string_addresses':[hex(x) for x in ptrs],'lines':[line1,line2],'continuation':hex(actual[24]),'relocations':diffs})
(out/'comparison.json').write_text(json.dumps(found,indent=2)); print(json.dumps([{k:v for k,v in f.items() if k!='relocations'} for f in found],indent=2));print('sha',hashlib.sha256((out/'comparison.json').read_bytes()).hexdigest())
