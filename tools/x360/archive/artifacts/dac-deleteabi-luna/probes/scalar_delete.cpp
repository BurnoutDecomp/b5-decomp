struct PlugIn {
    virtual void ReleaseEvent();
    virtual void EventEvent(int, void*);
    virtual unsigned int GetPpuTicksEvent() const;
    virtual ~PlugIn();
};
void PlugIn::ReleaseEvent() {}
void PlugIn::EventEvent(int, void*) {}
unsigned int PlugIn::GetPpuTicksEvent() const { return 0; }
PlugIn::~PlugIn() {}
struct Dac : PlugIn {
    virtual void EventEvent(int, void*);
    virtual ~Dac();
};
void Dac::EventEvent(int, void*) {}
Dac::~Dac() {}
void DeleteScalar(Dac* p) { delete p; }
extern "C" int probe_main() { Dac local; return 0; }
