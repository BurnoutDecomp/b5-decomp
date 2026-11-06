import pathlib,struct,hashlib,json,re,subprocess
root=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\native_render_headers_sol_4277'); sdk=pathlib.Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK\lib\xbox'); out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave10\d3d-native-provider-inquiry'); rp=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\native-r7-strict-link-diagnostic-root\duplicate-input-preflight-sol.json'); report=json.loads(rp.read_text()); sha=lambda b:hashlib.sha256(b).hexdigest(); fh=lambda p:sha(pathlib.Path(p).read_bytes()); dup=[d for d in report['duplicates'] if d['category']=='sdk_provider_overlap']; symbols={re.search(r'\((\?[^()]*)\)\s*$',d['symbol']).group(1) for d in dup}; archives=[]; allhits={}; target=[]
for name in ['d3d9.lib','d3d9d.lib','d3d9i.lib','d3d9ltcg.lib','d3d9ltcgi.lib','xgraphics.lib','xgraphicsd.lib','xapilib.lib','xapilibd.lib','xapilibi.lib','xboxkrnl.lib']:
 p=sdk/name;b=p.read_bytes();assert b[:8]==b'!<arch>\n';off=8;members={};longnames=b'';first=None
 while off+60<=len(b):
  h=b[off:off+60];n=h[:16].decode('ascii','replace').strip();sz=int(h[48:58].decode().strip());data=b[off+60:off+60+sz];members[off]=(n,data)
  if n=='/' and first is None:first=data
  if n=='//':longnames=data
  off+=60+sz+(sz%2)
 def mname(of):
  n=members[of][0]
  if n.startswith('/') and n[1:].isdigit():
   x=int(n[1:]);return longnames[x:].split(b'/\n',1)[0].split(b'\0',1)[0].decode('ascii','replace')
  return n.rstrip('/')
 count=struct.unpack_from('>I',first)[0]; offsets=struct.unpack_from('>'+str(count)+'I',first,4); names=first[4+4*count:].split(b'\0');archives.append({'path':str(p),'sha256':sha(b),'indexed_symbols':count})
 for offset,bs in zip(offsets,names):
  sym=bs.decode('ascii','replace')
  if sym in symbols or 'CBlocker' in sym or 'GPUTimingMarkerSmall' in sym:
   record={'archive':str(p),'member':mname(offset),'member_sha256':sha(members[offset][1]),'symbol':sym}
   if sym in symbols:allhits.setdefault(sym,[]).append(record)
   if 'CBlocker' in sym or 'GPUTimingMarkerSmall' in sym:target.append(record)
for d in dup:
 sym=re.search(r'\((\?[^()]*)\)\s*$',d['symbol']).group(1); d['mangled_symbol']=sym;d['candidate_archive_providers']=allhits.get(sym,[]);p=root/d['source_b'];d['current_source_exists']=p.is_file();d['current_source_sha256']=fh(p) if p.is_file() else None
expected=[{'source_member':'CBlocker::CBlocker(CDevice*, s32)','expected_mangled_inferred':'??0CBlocker@D3D@@QAA@PAVCDevice@1@H@Z'}, {'source_member':'CBlocker::~CBlocker()','expected_mangled_inferred':'??1CBlocker@D3D@@QAA@XZ'}, {'source_member':'CBlocker::Check() -> bool','expected_mangled_inferred':'?Check@CBlocker@D3D@@QAA_NXZ'}, {'source_member':'GPUTimingMarkerSmall::GPUTimingMarkerSmall(s32,CDevice*)','expected_mangled_inferred':'??0GPUTimingMarkerSmall@D3D@@QAA@HPAVCDevice@1@@Z'}, {'source_member':'GPUTimingMarkerSmall::~GPUTimingMarkerSmall()','expected_mangled_inferred':'??1GPUTimingMarkerSmall@D3D@@QAA@XZ'}]
for item in expected:item['exact_index_providers']=[h for h in target if h['symbol']==item['expected_mangled_inferred']]
sources=['src/SDKs/D3D/D3DBlocker.h','src/SDKs/D3D/D3DBlocker.cpp','src/SDKs/D3D/GPUTimingMarkerSmall.h','src/SDKs/D3D/GPUTimingMarkerSmall.cpp']
data={'scope':'Read-only archive first-linker-member index and historical R7 duplicate rows; no link, build, source change or provider selection. Index exports do not establish runtime equivalence or safe ownership.','head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'tree':subprocess.check_output(['git','rev-parse','HEAD^{tree}'],cwd=root,text=True).strip(),'source_hashes':{p:fh(root/p) for p in sources},'r7_report':str(rp),'r7_report_sha256':fh(rp),'archive_indexes':archives,'target_class_archive_symbols':target,'current_declaration_mangled_names':expected,'mangling_qualification':'Expected source manglings inferred from MSVC PPC naming and source declarations; no current object was built to verify them. Destructor names are exact archive string matches.','historical_r7_sdk_overlaps':dup,'overlap_scope_qualification':'These 69 rows are historical R7 linker diagnostics, with current source file hashes and archive index matches. Current source definition persistence is not re-established by source parsing; no current link rerun.','limitations':['No claim that alternative SDK builds are ABI or behavior compatible with ARTIST.','Archive index presence is not proof a member can be selected without further duplicate symbols.','Internal thread-control access remains unresolved.']}
(out/'providers.json').write_text(json.dumps(data,indent=2)+'\n'); print('JSON',out/'providers.json','sha256',fh(out/'providers.json'));print('69 rows indexed',sum(bool(d['candidate_archive_providers']) for d in dup));print('exact',[(x['source_member'],len(x['exact_index_providers'])) for x in expected])
