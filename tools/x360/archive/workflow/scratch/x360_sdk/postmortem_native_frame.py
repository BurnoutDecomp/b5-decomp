"""Private, assembly-backed work postmortems for the native frame batch."""
from pathlib import Path
import json,sqlite3,sys
from types import SimpleNamespace
ROOT=Path(__file__).resolve().parents[2];OUT=Path(__file__).parent
sys.path.insert(0,str(ROOT/'tools/work'))
import work,dossier,gen_skeleton
for module in [dossier,gen_skeleton]:module.X360_EXPORTS='D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX'
dossier.FEB2007='D:/Reverse/BP-Decomp_Workflow/references/Feb-2007/BrnEntityModuleUnity'
dossier.DECFIGS_DWARFDUMP='D:/Reverse/BP-Decomp_Workflow/references/DecFIGS/dwarfdump'
ident=json.loads((ROOT/'progress/identity.json').read_text());index=json.loads((ROOT/'progress/tu_index.json').read_text())
names=[n for n in ident if n.startswith('CgsSystem::ThreadLayout::') or n.startswith('CgsMemory::DataStreamResultReader::')]
names+=['EA::Thread::Barrier::Barrier','EA::Thread::Barrier::Init','EA::Thread::Barrier::Wait','EABarrierData::EABarrierData']
groups={}
for name in names:
    tus=[tu for tu,info in index.items() if name in info['functions']]
    assert len(tus)==1,(name,tus)
    groups.setdefault(tus[0],[]).append(name)
database=OUT/'native-frame-postmortem.sqlite';assert not database.exists()
con=sqlite3.connect(database);con.row_factory=sqlite3.Row
source=sqlite3.connect(OUT/'native-thread-sync-postmortem.sqlite')
for sql, in source.execute("SELECT sql FROM sqlite_master WHERE type='table' AND name!='sqlite_sequence'"):con.execute(sql)
packets=[];work.connect=lambda:con
for tu,functions in groups.items():
    con.execute('INSERT INTO tu(id,source,n_funcs,n_decfigs) VALUES(?,?,?,?)',(tu,index[tu]['source'],len(functions),sum(bool(ident[n].get('primary_file')) for n in functions)))
    for name in functions:con.execute('INSERT INTO func(name,tu_id,x360_addr) VALUES(?,?,?)',(name,tu,ident[name]['x360_addrs'][0]))
    con.commit();p=OUT/f'POSTMORTEM_NATIVE_FRAME_{len(packets)+1}.md'
    work.cmd_postmortem(SimpleNamespace(tu=tu,out=str(p)));packets.append({'tu':tu,'functions':functions,'path':str(p)})
(OUT/'native-frame-postmortems.json').write_text(json.dumps(packets,indent=2)+'\n')
print(len(packets),'private packets; no production-ledger mutation')
