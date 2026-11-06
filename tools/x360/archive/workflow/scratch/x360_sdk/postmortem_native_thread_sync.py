"""Private work postmortems; never writes the production work ledger."""
from pathlib import Path
import json
import sqlite3
import sys
from types import SimpleNamespace

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).parent
sys.path.insert(0,str(ROOT/'tools/work'))
import work,dossier,gen_skeleton
for module in [dossier,gen_skeleton]:
    module.X360_EXPORTS='D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX'
dossier.FEB2007='D:/Reverse/BP-Decomp_Workflow/references/Feb-2007/BrnEntityModuleUnity'
dossier.DECFIGS_DWARFDUMP='D:/Reverse/BP-Decomp_Workflow/references/DecFIGS/dwarfdump'
identity=json.loads((ROOT/'progress/identity.json').read_text())
index=json.loads((ROOT/'progress/tu_index.json').read_text())
curated=[['EA::Thread::Thread::Begin','EA::Thread::ThreadSleep'],
         ['EA::Thread::Mutex::Init','EA::Thread::Mutex::Lock','EA::Thread::Mutex::Unlock'],
         ['EAConditionData::EAConditionData','EA::Thread::Condition::Condition',
          'EA::Thread::Condition::~Condition','EA::Thread::Condition::Init',
          'EA::Thread::Condition::Wait','EA::Thread::Condition::Signal'],
         ['EARWMutexData::EARWMutexData','EA::Thread::RWMutex::Init',
          'EA::Thread::RWMutex::Lock','EA::Thread::RWMutex::Unlock'],
         ['RealmcCore::MessageQueue::_WaitForResponse','RealmcCore::MessageQueue::ShutDown']]
by_tu={}
for names in curated:
    for name in names:
        assert name in identity,name
        tus=[tu for tu,row in index.items() if name in row['functions']]
        assert len(tus)==1,(name,tus)
        by_tu.setdefault(tus[0],[]).append(name)
database=OUT/'native-thread-sync-postmortem.sqlite'
assert not database.exists(),'Do not rerun this creation helper'
source=sqlite3.connect(OUT/'native-timing-postmortem.sqlite')
con=sqlite3.connect(database);con.row_factory=sqlite3.Row
for name,sql in source.execute("SELECT name,sql FROM sqlite_master WHERE type='table' AND name!='sqlite_sequence'"):
    con.execute(sql)
packets=[]
for n,(tu,names) in enumerate(by_tu.items(),1):
    info=index[tu]
    con.execute('INSERT INTO tu(id,source,n_funcs,n_decfigs) VALUES(?,?,?,?)',
                (tu,info['source'],len(names),sum(bool(identity[name].get('primary_file')) for name in names)))
    for name in names:
        con.execute('INSERT INTO func(name,tu_id,x360_addr) VALUES(?,?,?)',
                    (name,tu,identity[name]['x360_addrs'][0]))
    con.commit();work.connect=lambda:con
    output=OUT/f'POSTMORTEM_NATIVE_THREAD_SYNC_{n}.md'
    work.cmd_postmortem(SimpleNamespace(tu=tu,out=str(output)))
    packets.append({'tu':tu,'functions':names,'path':str(output)})
(OUT/'native-thread-sync-postmortems.json').write_text(json.dumps(packets,indent=2)+'\n')
print(len(packets),'private packets; production ledger untouched')
