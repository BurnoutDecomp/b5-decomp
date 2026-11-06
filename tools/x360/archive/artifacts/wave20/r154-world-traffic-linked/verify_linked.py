"""Carry R153 World/Traffic object windows into R154 to a separately pinned official linked image."""
import argparse,hashlib,importlib.util,json,re,struct,subprocess
from pathlib import Path
W=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20');OUT=W/'r154-world-traffic-linked'; L=W/'r154-link';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
ap=argparse.ArgumentParser();ap.add_argument('--invocation-sha256',required=True);ap.add_argument('--seal-sha256',required=True);ap.add_argument('--result-sha256',required=True);ap.add_argument('--pe-sha256',required=True);ap.add_argument('--map-sha256',required=True);ap.add_argument('--xex-sha256',required=True);args=ap.parse_args()
for fn,h in [('invocation.json',args.invocation_sha256),('seal.json',args.seal_sha256),('result.json',args.result_sha256),('native-game.pe',args.pe_sha256),('native-game.map',args.map_sha256),('native-game.exe',args.xex_sha256)]:assert sha(L/fn)==h,(fn,sha(L/fn),h)
inv=json.loads((L/'invocation.json').read_text());seal=json.loads((L/'seal.json').read_text());result=json.loads((L/'result.json').read_text());assert seal['head']=='13dfc2ea9153ab2f36c06ad09e2570998e0a2d6d';assert result['exit_code']==0 and result['all_post_checks_pass'];assert inv['sealed_sha256']==args.seal_sha256
for filename,digest in [('native-game.exe',args.xex_sha256),('native-game.pe',args.pe_sha256),('native-game.map',args.map_sha256)]:
 assert result['outputs'][str(L/filename)]['sha256']==digest
tree=subprocess.check_output(['git','--no-optional-locks','-C','C:/Users/adri1/.codex/worktrees/bp360/wave4-intake','rev-parse',seal['head']+'^{tree}'],text=True).strip();assert tree=='ae764bcba446118b5bc8bda12ac461f5ede4a9a5'
coff=json.loads((OUT/'official-coff.json').read_text());assert coff['prior_linked_proof_sha256']=='637dcf4ff43eec690dbc7d126b0cf418eccdefe18bfc72dcdf6c315dfd02be8b' and sha(OUT/'official-coff.json')=='0ec61c1db1194d7fed3a9d8aea6c90be1b9ecb3db1120abd9e109b728b24161f'
reader=W/'r120-debug2d-match/compare.py'; env={'__file__':str(reader)};exec(reader.read_text().split('\nrows = []',1)[0],env)
image=(L/'native-game.pe').read_bytes(); hdr=struct.unpack_from('<I',image,0x3c)[0];assert image[hdr:hdr+4]==b'PE\0\0';machine,nsec,_,_,_,opt,_=struct.unpack_from('<HHIIIHH',image,hdr+4);base=struct.unpack_from('<I',image,hdr+52)[0];assert machine==0x1f2 and base==0x82000000
sections=[struct.unpack_from('<8sIIII',image,hdr+24+opt+40*i) for i in range(nsec)]
def readva(va,n):
 rva=va-base
 for _,_,start,size,offset in sections:
  if start<=rva and rva+n<=start+size:return image[offset+rva-start:offset+rva-start+n]
 raise ValueError(hex(va))
maptext=(L/'native-game.map').read_text(errors='replace');selected={r['source'].lower():r for r in inv['objects']};out={}; masks={0x2:0xffffffff,0x6:0x03fffffc,0x7:0xfffc,0x10:0xffff,0x11:0xffff,0x12:0}
for label,x in coff['objects'].items():
 matches=[r for r in inv['objects'] if Path(r['object']).name.lower()==Path(x['path']).name.lower()];assert len(matches)==1,(label,matches);r=matches[0];assert sha(r['object'])==r['sha256']==x['sha256'],label
 env['symbol']=x['symbol_prefix'].encode(); name,raw,relocs=env['read_function'](Path(r['object'])); pattern=rf'(?m)^\s+0003:[0-9a-fA-F]+\s+{re.escape(name)}\s+([0-9a-fA-F]{{8}})\s'; matches=re.findall(pattern,maptext);
 if label=='tow' and not matches:
  out[label]={'source':r['source'],'object':r['object'],'object_sha256':r['sha256'],'symbol':name,'status':'not found in linked map; retained COFF only'};continue
 assert len(matches)==1,(name,matches);va=int(matches[0],16);linked=readva(va,len(raw));rm={}
 for off,_,kind in relocs:rm[off//4]=rm.get(off//4,0)|masks[kind]
 a=struct.unpack('>'+str(len(raw)//4)+'I',raw);z=struct.unpack('>'+str(len(linked)//4)+'I',linked);assert all(not ((q^t)&~rm.get(i,0)) for i,(q,t) in enumerate(zip(a,z))),label
 out[label]={'source':r['source'],'object':r['object'],'object_sha256':r['sha256'],'symbol':name,'linked_va':hex(va),'length':len(raw),'linked_sha256':hashlib.sha256(linked).hexdigest(),'relocation_count':len(relocs)}
# Verify relocated direct calls in the scoped code against the actual mapped provider VAs.
def target(label,offset):
 va=int(out[label]['linked_va'],16);word=struct.unpack('>I',readva(va+offset,4))[0];assert word>>26==18 and word&3==1;delta=word&0x03fffffc;return va+offset+(delta if delta<0x02000000 else delta-0x04000000)
def eq(label,off,provider):assert target(label,off)==int(out[provider]['linked_va'],16),(label,hex(off),hex(target(label,off)),out[provider])
eq('world',0x3b8,'in_get');eq('world',0x3c4,'out_set');eq('hull',0x94,'light');eq('network',0x24,'out_get')
# Resolve all other control-transfer witnesses against their mapped symbols.
spec=importlib.util.spec_from_file_location('coffrefs',W/'r143-cih-proof/verify.py');cf=importlib.util.module_from_spec(spec);spec.loader.exec_module(cf)
for label,offsets in {'world':[0x314,0x3b0,0x3cc,0x404,0x414], 'hull':[0x8c], 'network':[0x18,0x34], 'out_set':[0x5c]}.items():
 _,refs=cf.coff(Path(out[label]['object']),coff['objects'][label]['symbol_prefix'])
 for off in offsets:
  name=refs[off//4]; match=re.findall(rf'(?m)^\s+0003:[0-9a-fA-F]+\s+{re.escape(name)}\s+([0-9a-fA-F]{{8}})\s',maptext);assert len(match)==1,(label,name,match);assert target(label,off)==int(match[0],16),(label,hex(off),name)

assert '?InitialiseAsTrailer@Vehicle@BrnTraffic@@' not in maptext
assert image.count(bytes.fromhex('101c58c31000028c11be50c3119e58c3100c682e1000e9c3'))==0
receipt={'head':seal['head'],'tree':'ae764bcba446118b5bc8bda12ac461f5ede4a9a5','official_coff_sha256':sha(OUT/'official-coff.json'),'artist_windows_sha256':coff['prior_artist_windows_sha256'],'pins':{'invocation':args.invocation_sha256,'seal':args.seal_sha256,'result':args.result_sha256,'pe':args.pe_sha256,'map':args.map_sha256,'xex':args.xex_sha256},'functions':out,'tow_evidence':'selected COFF checked; both CalcTowBarPos and InitialiseAsTrailer absent from link map; exact selected VMX six-word sequence absent from PE', 'resolved_edges':['world input getter','world output setter','network output getter','hull traffic light update'],'scope':'linked machine-code windows plus direct-call targets; tow only selected COFF because no linked map symbol; static only, not gameplay proof'}
(OUT/'linked-proof.json').write_text(json.dumps(receipt,indent=2)+'\n');print('LINKED PASS',sha(OUT/'linked-proof.json'))


