struct TrivialName { unsigned value; };
struct CopyName { unsigned value; CopyName(const CopyName& x) : value(x.value) {} };
unsigned consume_trivial(TrivialName n) { return n.value; }
unsigned consume_copy(CopyName n) { return n.value; }
