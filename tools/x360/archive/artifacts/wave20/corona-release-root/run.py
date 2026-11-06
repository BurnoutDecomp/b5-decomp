import hashlib,json,os,pathlib,subprocess,sys
OUT=pathlib.Path(__file__).resolve().parent
WORK=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
PARENT='18430d2df92efdd34fa777f988eae5af1866d362'
MGR='src/GameSource/Graphics/BrnCoronaManager.cpp'
MGR_H=MGR[:-3]+'h'
SDK_SRC='src/SDKs/RenderEngineClub/MAIN/components/src/coronas/rwgcoronarenderer.cpp'
SDK_HDR='src/SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronarenderer.h'
CALLER='src/GameSource/Graphics/BrnRendererModule.cpp'

def git(*args):
    return subprocess.check_output(['git',*args],cwd=WORK).decode().strip()
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def update(rel,old,new):
    p=WORK/rel
    b=p.read_bytes()
    eol='\r\n' if b'\r\n' in b else '\n'
    s=b.decode().replace('\r\n','\n')
    assert s.count(old)==1,(rel,old)
    p.write_bytes(s.replace(old,new).replace('\n',eol).encode())

assert git('rev-parse','HEAD')==PARENT and not git('status','--porcelain=v1')
update(MGR_H,"""    // BrnCoronaManager.h:193. Not X360-attested for this TU (attested elsewhere: ledger TU
    // SDKs/EATech/include/cmn/rw/core/resource/resourceallocator.h @ 0x823F7720, not bodied there
    // either) -- declared only here.
    bool Release(const rw::IResourceAllocator& lpAllocator);""",
"""    // ARTIST 0x823F7720. Release buffer, atlas state and shared shader resources.
    bool Release(rw::IResourceAllocator& lrAllocator);""")
marker='// Inlined by ARTIST Renderer StartOfFrame/SwapBuffers/Update.'
update(MGR,marker,"""// ARTIST 0x823F7720
bool BrnCoronaManager::Release(rw::IResourceAllocator& lrAllocator)
{
    if (m_textureStateAtlas != 0)
        lrAllocator.DoFree(m_textureStateAtlasResource);
    m_coronaBuffer1->Release();
    m_coronaBuffer0->Release();
    lrAllocator.DoFree(m_coronaBuffer1Resource);
    lrAllocator.DoFree(m_coronaBuffer0Resource);
    renderengine::CoronaRenderer::Release(lrAllocator);
    return true;
}

"""+marker)
marker='        static void SetBlendState('
update(SDK_HDR,marker,'        static void Release(rw::IResourceAllocator& lrAllocator);\n'+marker)
old="""        // :111/:112/:113 s_vertexDescriptorResource / s_pixelProgramResource / s_vertexProgramResource
        // are the three FIVE-WORD rw::Resource handle arrays the console's allocator route fills
        // (dword_82FAC1B8 / dword_82FAB850 / dword_82FAC1CC). The PC route adopts pre-built images
        // and carves the descriptor from the immediate-mode leaf's own arena, so no resource is ever
        // issued -- they are deliberately NOT declared here rather than declared and left null.
        // (Same call as BrnPostFxShader.cpp:910, "a slot built by ProgramBufferPC_Adopt owns NOTHING
        // the allocator ever issued".)"""
update(SDK_HDR,old,'        // Initialize and Release own the three resource records in rwgcoronarenderer.cpp.')
marker='void CoronaRenderer::SetBlendState('
update(SDK_SRC,marker,"""// ARTIST 0x8227B490
void CoronaRenderer::Release(rw::IResourceAllocator& lrAllocator)
{
    VertexDescriptor::Release(s_vertexDescriptor);
    ProgramBuffer::Release(s_vertexProgram);
    ProgramBuffer::Release(s_pixelProgram);
    lrAllocator.DoFree(gsVertexDescriptorResource);
    lrAllocator.DoFree(gsVertexProgramResource);
    lrAllocator.DoFree(gsPixelProgramResource);
}

"""+marker)
paths=(MGR,MGR_H,SDK_SRC,SDK_HDR)
assert set(git('diff','--name-only').splitlines())==set(paths)
git('diff','--check','--',*paths)
args=json.loads((OUT.parent/'huffman-combined-root/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(OUT/'native1')
for p in (MGR,SDK_SRC,CALLER):args+=['--source',p]
temp=OUT/'temp'
temp.mkdir()
(OUT/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=WORK,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
rp=OUT/'native1/compile-results.json'
r=json.loads(rp.read_text())
assert r['requested']==r['passed']==3 and not r['failed']
rows={x['source']:x for x in r['results']}
assert all(x['machine']==498 for x in rows.values())
def asm(p):
    return pathlib.Path(rows[p]['assembly']).read_text(errors='replace')
m='?Release@BrnCoronaManager@@QAA_NAAUIResourceAllocator@rw@@@Z'
s='?Release@CoronaRenderer@renderengine@@SAXAAUIResourceAllocator@rw@@@Z'
assert 'Begin code for function: '+m in asm(MGR)
assert 'Begin code for function: '+s in asm(SDK_SRC)
assert any(m in l and ' bl ' in l for l in asm(CALLER).splitlines())
msg=OUT/'COMMIT_MESSAGE.txt'
msg.write_text('Restore native corona resource release order\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',*paths)
assert set(git('diff','--cached','--name-only').splitlines())==set(paths)
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
receipt={'parent':PARENT,'commit':git('rev-parse','HEAD'),'paths':{p:sha(WORK/p) for p in paths},
         'artist_manager':'0x823F7720..0x823F77C3','artist_renderer':'0x8227B490..0x8227B52F',
         'original_buffer_release':'both calls folded to blr at 0x8284CB38',
         'decfigs':'BrnCoronaManager.h:193 and rwgcoronarenderer.h:108 use non-const allocator references',
         'report':str(rp),'report_sha256':sha(rp)}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])
