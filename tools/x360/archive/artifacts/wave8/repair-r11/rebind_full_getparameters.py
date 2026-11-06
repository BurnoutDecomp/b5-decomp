"""Exact48-byte GetParameters proof on the actual integrated selected provider."""
from datetime import datetime,timezone
import hashlib,importlib.util,json,struct,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;proofpath=HERE.parent.parent/'indexbuffer-byte-proof-root/PROOF.json'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
proof=json.loads(proofpath.read_text());assert sha(proofpath)=='fed70bdf5dd9edfe78ee9b0cecfd8949124730e819d95e1bbaa4cdca19c85ae1'
reportpath=HERE/'full-stage1/compile-results.json';report=json.loads(reportpath.read_text());rows=[r for r in report['results']if Path(r['source']).name=='indexbuffer.cpp'];assert len(rows)==1 and rows[0]['passed'];row=rows[0]
b=Path(row['object']).read_bytes();machine,n,_,off,ns,opt,_=struct.unpack_from('<HHLLLHH',b);assert machine==0x1f2;st=off+18*ns;i=0;matches=[]
while i<ns:
 at=off+18*i;raw=b[at:at+8]
 if raw[:4]==bytes(4):start=st+struct.unpack_from('<I',raw,4)[0];name=b[start:b.index(0,start)].decode()
 else:name=raw.split(bytes(1),1)[0].decode()
 value,sec=struct.unpack_from('<Ih',b,at+8)
 if name==proof['symbol']and sec>0:matches.append((name,value,sec))
 i+=1+b[at+17]
assert len(matches)==1;name,value,sec=matches[0];sh=20+opt+(sec-1)*40;size,raw,rel=struct.unpack_from('<III',b,sh+16);nr=struct.unpack_from('<H',b,sh+32)[0]
assert size==48 and value==0 and nr==0;code=b[raw:raw+48]
reader=Path(proof['reader']);assert sha(reader)==proof['reader_sha256'];s=importlib.util.spec_from_file_location('r11_calibrated_raw_reader',reader);x=importlib.util.module_from_spec(s);s.loader.exec_module(x)
assert x.u32(0x82815570)==0x7c8a2378 and x.u32(0x82f264a8)==4
original=x.rd(0x82b60730,48);assert original.hex()==proof['original_bytes']and code==original
head=json.loads((HERE/'repair-r11-receipts.json').read_text())['head'];source=Path(json.loads((HERE/'campaign.json').read_text())['workspace'])/row['source']
result={'head':head,'function':proof['function'],'symbol':name,'source':row['source'],'source_sha256':sha(source),'actual_integrated_object':row['object'],'object_sha256':sha(row['object']),
 'actual_full_report_sha256':sha(reportpath),'section':sec,'section_size':size,'symbol_offset':value,'exact48':True,'instruction_count':12,'code_relocations':[],
 'original_start':'0x82B60730','original_last_instruction':'0x82B6075C','original_bytes':original.hex(),'integrated_bytes':code.hex(),'normalization':False,
 'raw_reader':str(reader),'raw_reader_sha256':sha(reader),'calibration_sentinels_verified':True,'private_proof_sha256':sha(proofpath),
 'limits':'One complete48-byte method only,zero code relocations. Companion memory-protection method remains explicitly not byte-identical. No whole-TU/program/data/runtime/hardware/link/image/launch or overall matching-percentage claim.',
 'created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'full-getparameters-proof.json').open('x')as f:json.dump(result,f,indent=2)
print('FULL_INTEGRATED_EXACT48',sha(HERE/'full-getparameters-proof.json'),flush=True)
