#include "../src/visuals/projection.h"
#include <cstdio>
#include <limits>
int main() {
    const float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0};
    ImVec2 s;
    int failed = 0;
    auto check = [&](bool result, const char *label) {
        std::printf("%s %s\n", result ? "PASS" : "FAIL", label);
        if (!result)
            ++failed;
    };
    check(project({0, 0, 1}, m, {1920, 1080}, s) && s.x == 960 && s.y == 540, "centro");
    check(project({1, 1, 1}, m, {1920, 1080}, s) && s.x == 1920 && s.y == 0, "extremo superior direito");
    check(!project({0, 0, -1}, m, {1920, 1080}, s), "ponto atras da camera");
    check(!project({0, 0, 0}, m, {1920, 1080}, s), "divisao por zero");
    check(!project({std::numeric_limits<float>::quiet_NaN(), 0, 1}, m, {1920, 1080}, s), "NaN");
    check(!project({std::numeric_limits<float>::infinity(), 0, 1}, m, {1920, 1080}, s), "infinito");
    return failed ? 1 : 0;
}
