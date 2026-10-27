#include "../src/input/recoil.h"
#include <cstdio>
#include <limits>
int main() {
    int failures = 0;
    auto test = [&](bool good, const char *name) {
        std::printf("%s %s\n", good ? "PASS" : "FAIL", name);
        if (!good)
            ++failures;
    };
    auto sum = [](int fps) {
        RecoilAccumulator a;
        long n = 0;
        for (int i = 0; i < fps; ++i)
            n += a.step(1.0f / fps, true, 0, 90).second;
        return n;
    };
    test(std::abs(sum(60) - sum(240)) <= 1 && std::abs(sum(240) - 90) <= 1,
         "compensation independent of FPS");
    RecoilAccumulator a;
    a.step(.001f, true, 0, 90);
    auto r = a.step(.016f, false, 0, 90);
    test(r.first == 0 && r.second == 0 && a.x == 0 && a.y == 0,
         "release or focus/menu gate clears remainder");
    r = a.step(.016f, true, -100, 0);
    test(r.first < 0 && r.second == 0, "horizontal direction configurable");
    a = {};
    r = a.step(10, true, 0, 100);
    test(r.second == 5, "long pause does not cause large jump");
    a = {};
    r = a.step(.016f, true, 0, std::numeric_limits<float>::quiet_NaN());
    test(r.second == 0 && a.y == 0, "invalid settings never generate input");
    a = {};
    r = a.step(.016f, true, 0, -100);
    test(r.second == 0, "vertical compensation cannot invert");
    return failures ? 1 : 0;
}
