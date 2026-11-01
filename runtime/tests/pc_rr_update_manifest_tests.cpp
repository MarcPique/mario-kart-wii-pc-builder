#include "pc_rr_update_manifest.h"
#include <cassert>

int main() {
    using namespace PcRetroUpdate;
    assert(ParseVersion("6.13.4") == Version({6,13,4}));
    assert(!ParseVersion("6.13"));
    assert(!ParseVersion("6.13.4x"));
    assert(!ParseVersion("6.-1.4"));
    assert(!ParseVersion("6.13.4.1"));
    assert(!Latest("junk https://example.invalid\n"));
    const auto latest=Latest("6.9.9 https://example.invalid\n6.13.4 https://example.invalid\n6.12.9 https://example.invalid\n");
    assert(latest && *latest==Version({6,13,4}));
    assert(Format(*latest)=="6.13.4");
}
