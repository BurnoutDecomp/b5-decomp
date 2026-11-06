class Volume { public: void f(); };
extern "C" unsigned member_size() { return sizeof(void (Volume::*)()); }
