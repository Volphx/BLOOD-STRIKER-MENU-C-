#include "../src/input/aim_target.h"
#include <cstdio>
#include <limits>

int main() {
    int failures = 0;
    auto check = [&](bool pass, const char *label) {
        std::printf("%s %s\n", pass ? "PASS" : "FAIL", label);
        if (!pass)
            ++failures;
    };
    Entity entity;
    Vec3 point;
    check(!selectedAimPoint(entity, 0, point), "missing bones do not become estimated targets");
    check(!selectedAimPoint(entity, -1, point) && !selectedAimPoint(entity, 4, point),
          "invalid dropdown regions rejected");
    for (int region = 0; region < 4; ++region) {
        entity.aimPointsKnown[region] = true;
        entity.aimPoints[region] = {float(region + 1), 2, 3};
        check(selectedAimPoint(entity, region, point) && point.x == region + 1,
              "selected region returns its own pose");
    }
    entity.aimPoints[0].x = std::numeric_limits<float>::quiet_NaN();
    check(!selectedAimPoint(entity, 0, point), "invalid coordinates rejected");
    check(aimBonePriority("Biped Spine2", 2) > aimBonePriority("biped spine", 2),
          "chest prefers upper spine");
    check(aimBonePriority("biped neck", 1) > 0, "neck recognized");
    check(aimBonePriority("biped foot", 0) == 0, "limb is not a head target");
    return failures ? 1 : 0;
}
