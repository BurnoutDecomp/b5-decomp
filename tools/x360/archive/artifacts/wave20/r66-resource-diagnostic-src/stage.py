from pathlib import Path


source = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r41-voip-send/src/GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModule.cpp")
body = source.read_text(encoding="utf-8")


def insert(anchor, diagnostic):
    global body
    assert body.count(anchor) == 1, anchor
    body = body.replace(anchor, diagnostic + anchor)


insert("#include <cstdlib>", "#include <xtl.h>\n")
insert('''        lpOutput->GetResourceRequestQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), 4,''', '''        char lacRequestProbe[240];
        const u64 luRequestedHash = lRequest.mResourceId.GetHash();
        CgsCore::SPrintf(lacRequestProbe, sizeof(lacRequestProbe),
            "R66_GUI_REQUEST kind=%d id=%u pool=%d hash=%08X%08X name=%s\\n",
            static_cast<s32>(lrBundle.meResourceType), lrBundle.muRequestId,
            lRequest.miPoolId, static_cast<u32>(luRequestedHash >> 32),
            static_cast<u32>(luRequestedHash),
            lrBundle.mpcBundleNameToLoad ? lrBundle.mpcBundleNameToLoad : "(none)");
        OutputDebugStringA(lacRequestProbe);

''')
insert('''                for (s32 li = 0; li < miNumPendingResources; ++li)
                {
                    GuiBundleToLoad& lrBundle = maBundlesPending[li];
                    if (!lrBundle.mbParsed &&
                        lrBundle.meLoadUnload == E_GUI_RESOURCEREQUEST_LOAD &&''', '''                if (liSize >= static_cast<s32>(sizeof(*lpResponse)))
                {
                    char lacResponseProbe[200];
                    const u64 luReplyHash = lpResponse->mResourceId.GetHash();
                    CgsCore::SPrintf(lacResponseProbe, sizeof(lacResponseProbe),
                        "R66_GUI_REPLY id=%d pool=%d hash=%08X%08X handle=%08X/%08X size=%d\\n",
                        lpResponse->miEventId, lpResponse->miPoolId,
                        static_cast<u32>(luReplyHash >> 32), static_cast<u32>(luReplyHash),
                        reinterpret_cast<u32>(lpResponse->mResourceHandle.mpResourceMemory),
                        reinterpret_cast<u32>(lpResponse->mResourceHandle.mpSourceEntry), liSize);
                    OutputDebugStringA(lacResponseProbe);
                }

''')
insert('''                    CGS_ASSERT(lrBundle.mbParsed, "lBundle.mbParsed");''', '''                    if (!lrBundle.mbParsed)
                    {
                        char lacPendingProbe[240];
                        CgsCore::SPrintf(lacPendingProbe, sizeof(lacPendingProbe),
                            "R66_GUI_UNPARSED slot=%d/%d kind=%d op=%d id=%u pool=%d loaded=%d waiting=%d received=%d hash=%08X%08X\\n",
                            li, miNumPendingResources, static_cast<s32>(lrBundle.meResourceType),
                            static_cast<s32>(lrBundle.meLoadUnload), lrBundle.muRequestId,
                            lrBundle.miBundleBank, static_cast<s32>(lrBundle.mbLoaded),
                            mNumWaitingResourceAllocations, mReceiverQueue.GetLength(),
                            static_cast<u32>(lrBundle.muResourceId >> 32),
                            static_cast<u32>(lrBundle.muResourceId));
                        OutputDebugStringA(lacPendingProbe);
                    }
''')

with Path(__file__).with_name(source.name).open("x", encoding="utf-8") as output:
    output.write(body)
