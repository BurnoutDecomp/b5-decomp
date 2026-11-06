from pathlib import Path
import bisect,hashlib,json,re,struct,subprocess,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
import x360rd,ppcdis
OUT=Path(__file__).resolve().parent
EX=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
ROOT=Path('D:/codex-native/rce-constraints-sol')
targets={0x8264C4C0:'Construct',0x822A3400:'GetStaticLayout',0x82657A48:'CheckStaticLayoutCleared',0x8264C518:'Read',0x8264C618:'Write'}
exports=sorted(int(p.stem,16) for p in EX.glob('0x*.json'))
cache={}
def containing(ea):
    start=exports[bisect.bisect_right(exports,ea)-1]
    if start not in cache:cache[start]=json.loads((EX/('0x%08X.json'%start)).read_text(encoding='utf-8'))
    return {'nearest_export':hex(start),'name':cache[start]['name']}
x360rd._init();assert x360rd.u32(0x82815570)==0x7C8A2378
branches=[];literals=[];strings=[];segments=[]
needle=b'RaceCarEntitySerialiser'
for s,e,cum in x360rd._segs:
    if x360rd._base is None:data=x360rd._m[s-x360rd.FLAT_BASE:e-x360rd.FLAT_BASE]
    else:data=x360rd._m[x360rd._base+cum*4:x360rd._base+(cum+e-s)*4:4]
    segments.append({'start':hex(s),'end':hex(e),'sha256':hashlib.sha256(data).hexdigest()})
    for i in range(0,len(data)-3,4):
        word=struct.unpack_from('>I',data,i)[0];ea=s+i
        if word in targets:literals.append({'ea':hex(ea),'target':hex(word),'method':targets[word]})
        if s!=0x821F0000 or word>>26!=18:continue
        disp=word&0x3FFFFFC
        if disp&0x2000000:disp-=0x4000000
        target=(disp if word&2 else ea+disp)&0xFFFFFFFF
        if target in targets:
            branches.append({'ea':hex(ea),'target':hex(target),'method':targets[target],
                'link':bool(word&1),**containing(ea),'context':ppcdis.disasm(ea-24,11)})
    start=0
    while True:
        at=data.find(needle,start)
        if at<0:break
        begin=data.rfind(b'\0',max(0,at-140),at)+1;end=data.find(b'\0',at)
        if end<0:end=at+len(needle)
        strings.append({'ea':hex(s+at),'literal':data[begin:min(end,at+160)].decode('ascii',errors='replace')})
        start=at+len(needle)
source=[]
names=subprocess.check_output(['git','-C',str(ROOT),'grep','-l','-F','RaceCarEntitySerialiser','--','src']).decode('utf-8').splitlines()
for rel in names:
    p=ROOT/rel
    if p.suffix not in ['.h','.cpp','.hpp','.inl']:continue
    text=p.read_text(encoding='utf-8-sig',errors='replace')
    text=re.sub(r'/\*.*?\*/','',text,flags=re.S);text=re.sub(r'//[^\n]*','',text)
    lines=[{'line':i,'text':l.strip()} for i,l in enumerate(text.splitlines(),1) if 'RaceCarEntitySerialiser' in l]
    if lines:source.append({'path':rel,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'lines':lines})
report={'scope':'All aligned PPC direct branch opcodes in calibrated mapped text, all aligned target-valued words and exact class-name byte substrings in retained mapped segments. This is not a proof against computed pointers, unnamed inlined copies, external/unloaded code or eliminated functions. Nearest export is navigation, not an automatically proven caller receiver.',
 'segments':segments,'targets':{hex(k):v for k,v in targets.items()},'branches':branches,'literal_target_words':literals,'class_name_strings':strings,'source_code_uses':source}
p=OUT/'native-derived-user-census.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'direct_branches':len(branches),'branch_methods':{v:sum(b['method']==v for b in branches)for v in targets.values()},'target_words':len(literals),'strings':len(strings),'source_code_paths':[s['path']for s in source]},indent=2))
