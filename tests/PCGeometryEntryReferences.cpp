#include "pc/gcm/renderengine/GeometryEntryReferences.h"
#include <cstdio>
#include <unordered_map>
#include <vector>

static unsigned checks, failures;
static void Check(bool value, const char* name)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", name); }
}

int main()
{
    using namespace renderengine;
    GeometryEntryReferencesPC<int> refs;
    int first = 1, second = 2;
    Check(!refs.Find(0) && !refs.Find(UINT64_MAX), "invalid identities resolve to nothing");
    Check(refs.Add(nullptr) == 0 && refs.StorageSlots() == 0, "null registration consumes no slot");
    const auto a = refs.Add(&first), b = refs.Add(&second);
    Check(a && b && a != b && refs.Find(a) == &first && refs.Find(b) == &second,
          "simultaneous identities retain their own nodes");
    Check(refs.Retire(a) && !refs.Retire(a) && !refs.Find(a), "retirement is immediate and idempotent");
    const auto c = refs.Add(&first);
    Check(c != a && static_cast<unsigned>(c) == static_cast<unsigned>(a) && !refs.Find(a),
          "reused slot and address cannot revive an old page reference");
    Check(refs.Find(b) == &second && refs.Find(c) == &first, "other live entries survive reuse");
    Check(!refs.Retire(c + (1ull << 32)) && refs.Find(c) == &first,
          "mismatched generation cannot retire a current node");
    refs.Retire(b); refs.Retire(c);
    bool bounded = true;
    for (unsigned i = 0; i < 100000; ++i)
    {
        const auto token = refs.Add(&first);
        bounded &= refs.Find(token) == &first && !refs.Find(a) && !refs.Find(b) && !refs.Find(c);
        bounded &= refs.Retire(token);
    }
    Check(bounded && refs.StorageSlots() == 2, "streaming churn reuses bounded storage without stale identities");

    std::unordered_map<unsigned, int> owner;
    GeometryEntryReferencesPC<decltype(owner)::value_type> nodes;
    std::vector<std::uint64_t> live;
    for (unsigned i = 0; i < 5000; ++i)
        live.push_back(nodes.Add(&*owner.emplace(i, i * 7).first));
    owner.rehash(20000);
    bool preserved = true;
    for (unsigned i = 0; i < live.size(); ++i)
    {
        const auto* node = nodes.Find(live[i]);
        preserved &= node && node->first == i && node->second == i * 7;
    }
    Check(preserved, "owner-map growth and rehash preserve every registered node");
    bool retired = true;
    for (auto token : live) retired &= nodes.Retire(token);
    owner.clear();
    for (auto token : live) retired &= nodes.Find(token) == nullptr;
    Check(retired, "full owner teardown leaves no borrowable node");

    GeometryEntryReferencesPC<int, 3> wrapping;
    std::vector<std::uint64_t> dead;
    bool noRevival = true;
    for (unsigned i = 0; i < 30; ++i)
    {
        const auto token = wrapping.Add(&first);
        for (auto old : dead) noRevival &= !wrapping.Find(old);
        noRevival &= wrapping.Find(token) == &first && wrapping.Retire(token);
        dead.push_back(token);
    }
    Check(noRevival && wrapping.StorageSlots() == 10,
          "exhausted generations retire their slots instead of wrapping identities");
    std::printf("PCGeometryEntryReferences: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
