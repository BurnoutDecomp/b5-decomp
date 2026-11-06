from pathlib import Path
import hashlib,json,struct

class Records:
    def __init__(self,path):
        self.path=Path(path);self.data=self.path.read_bytes();b=self.data
        machine,ns,_,so,nn,opt,_=struct.unpack_from('<HHIIIHH',b);assert machine==0x1F2
        self.sections={i+1:struct.unpack_from('<8sIIIIIIHHI',b,20+opt+40*i)for i in range(ns)}
        strings=b[so+nn*18:];self.symbols={};i=0
        while i<nn:
            raw,value,sec,typ,storage,aux=struct.unpack_from('<8sIhHBB',b,so+18*i)
            if raw[:4]==bytes(4):at=struct.unpack_from('<I',raw,4)[0];name=strings[at:strings.index(b'\0',at)].decode()
            else:name=raw.rstrip(b'\0').decode()
            self.symbols[i]=(name,value,sec,typ,storage);i+=1+aux
    def relocs(self,sec):
        h=self.sections[sec]
        return [struct.unpack_from('<IIH',self.data,h[5]+10*i)for i in range(h[7])]
    def functions(self):
        result={}
        for name,value,sec,typ,storage in self.symbols.values():
            if sec<=0 or not typ&0x20:continue
            h=self.sections[sec]
            if not h[9]&0x20:continue
            body=self.data[h[4]+value:h[4]+h[3]]
            rel=[(off-value,kind,self.symbols[idx][0])for off,idx,kind in self.relocs(sec)if off>=value]
            result[name]={'body_sha256':hashlib.sha256(body).hexdigest(),'size':len(body),'relocations':rel}
        return result
    def words(self,name,count):
        found=[v for v in self.symbols.values()if v[0]==name and v[2]>0];assert len(found)==1
        _,offset,sec,_,_=found[0];h=self.sections[sec]
        return list(struct.unpack_from('>'+str(count)+'I',self.data,h[4]+offset))

def compare(a,b):
    af,bf=Records(a).functions(),Records(b).functions()
    return {'a':str(a),'b':str(b),'common':len(af.keys()&bf.keys()),
            'only_a':sorted(af.keys()-bf.keys()),'only_b':sorted(bf.keys()-af.keys()),
            'changed':{n:{'a':af[n],'b':bf[n]}for n in af.keys()&bf.keys()if af[n]!=bf[n]}}

if __name__=='__main__':
    OUT=Path(__file__).resolve().parent
    kinds=['class128','class64','members128','members64']
    reports={k:json.loads((OUT/('candidate-'+k+'-v2/compile-results.json')).read_text()) for k in kinds}
    layouts={}
    fields=['serialiser_size','serialiser_align','module_size','module_align','serialiser_offset','matrix_offset','invalidated','rendered','damage','world_racecar','world_traffic','base_size','base_align']
    for k,r in reports.items():
        row=next(x for x in r['results']if 'candidate-probe.cpp' in x['source']);assert row['passed']
        layouts[k]=dict(zip(fields,Records(row['object']).words('gCandidateLayout',len(fields))))
    comparisons={}
    first={r['source']:r for r in reports['class128']['results']if r['passed']and 'candidate-probe.cpp' not in r['source']}
    for k in kinds[1:]:
        other={r['source']:r for r in reports[k]['results']if r['passed']and 'candidate-probe.cpp' not in r['source']}
        assert first.keys()==other.keys()
        comparisons[k]={p:compare(first[p]['object'],other[p]['object'])for p in first}
    baseline=json.loads((OUT/'candidate-provider-baseline/compile-results.json').read_text())['results'][0]
    provider='src/GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.cpp'
    provider_comparison=compare(baseline['object'],first[provider]['object'])
    old=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_containment_sol_r1/pre-replay-callers-v1/compile-results.json')
    oldrow=next(r for r in json.loads(old.read_text())['results']if r['source'].endswith('BrnRaceCarEntityModule_NearMissTailgate.cpp'))
    key=next(k for k in first if k.endswith('BrnRaceCarEntityModule_NearMissTailgate.cpp'))
    oldf=Records(oldrow['object']).functions();newf=Records(first[key]['object']).functions()
    fn=next(n for n in oldf if n.startswith('?UpdatePowerParking@RaceCarEntityModule@'))
    assert oldf[fn]==newf[fn]
    result={'layouts':layouts,'method_code_comparisons':comparisons,'provider_baseline_comparison':provider_comparison,
            'parking_retention':{'old_object':oldrow['object'],'new_object':first[key]['object'],'symbol':fn,'identical_function_bytes_and_symbolic_relocations':True,'record':newf[fn]},
            'limits':'Code/relocation identity compares actual emitted functions, not complete COFF debug metadata. No attribute is independently proven by identity. The retained parking128 witness is preserved and is not rerun/relabelled as a full module test.'}
    p=OUT/'candidate-codegen-comparison.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
    print('layouts',json.dumps(layouts,indent=2))
    print('changed real functions',{k:sum(len(x['changed']) for x in v.values()) for k,v in comparisons.items()})
    print('provider changed',len(provider_comparison['changed']),'parking identical')
