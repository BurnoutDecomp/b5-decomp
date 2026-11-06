#include <xtl.h>
#include <xhv.h>

int main()
{
    XHV_PROCESSING_MODE modes[2] = {XHV_VOICECHAT_MODE, XHV_LOOPBACK_MODE};
    XHV_INIT_PARAMS params = {};
    params.dwMaxRemoteTalkers = 16;
    params.dwMaxLocalTalkers = 4;
    params.localTalkerEnabledModes = modes;
    params.dwNumLocalTalkerEnabledModes = 2;
    params.remoteTalkerEnabledModes = modes;
    params.dwNumRemoteTalkerEnabledModes = 1;
    HANDLE worker = 0;
    IXHVEngine* engine = 0;
    return XHVCreateEngine(&params, &worker, &engine);
}
