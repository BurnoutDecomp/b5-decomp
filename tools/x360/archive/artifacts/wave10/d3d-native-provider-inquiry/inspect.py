import pathlib,struct,hashlib,json,re
sdk=pathlib.Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK\lib\xbox')
r7=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\native-r7-strict-link-diagnostic-root\duplicate-input-preflight-sol.json'); r=json.loads(r7.read_text()); dup=[d for d in r['duplicates'] if d['category'].lower().find('sdk')>=0];print('sdk dup',len(dup),set(d['category'] for d in r['duplicates']))
for name in ['d3d9.lib','d3d9d.lib','d3d9i.lib','d3d9ltcg.lib','d3d9ltcgi.lib','xgraphics.lib','xgraphicsd.lib','xapilib.lib','xapilibd.lib','xapilibi.lib','xboxkrnl.lib']:
 p=sdk/name; b=p.read_bytes(); assert b[:8]==b'!<arch>\n'; off=8; members={}; longnames=b''; first=None
 while off+60<=len(b):
  h=b[off:off+60]; n=h[:16].decode('ascii','replace').strip(); sz=int(h[48:58].decode().strip()); data=b[off+60:off+60+sz]; members[off]=(n,data); 
  if n=='/' and first is None:first=data
  if n=='//':longnames=data
  off+=60+sz+(sz%2)
 def mname(of):
  n=members[of][0]
  if n.startswith('/') and n[1:].isdigit():
   x=int(n[1:]); return longnames[x:].split(b'/\n',1)[0].split(b'\0',1)[0].decode('ascii','replace')
  return n.rstrip('/')
 count=struct.unpack_from('>I',first)[0]; offsets=struct.unpack_from('>'+str(count)+'I',first,4); names=first[4+4*count:].split(b'\0'); assert len(names)>=count
 hits=[]
 for o,s in zip(offsets,names):
  sym=s.decode('ascii','replace')
  if 'CBlocker' in sym or 'GPUTimingMarkerSmall' in sym:
   member,dat=members[o];hits.append((sym,mname(o),hashlib.sha256(dat).hexdigest()))
 print(name,'symbols',count,'matches',hits)
