#include "../src/core/types.h"
#include <cstdio>

int main() {
    Options options;
    Entity target;
    int failures = 0;
    auto check = [&](bool pass, const char *name) {
        std::printf("%s %s\n", pass ? "PASS" : "FAIL", name);
        if (!pass)
            ++failures;
    };
    check(skipTeamEntity(target, options, 1, false), "match without local team hides unknown targets");
    check(!skipTeamEntity(target, options, 0, false, true), "confirmed training displays unassigned targets");
    check(skipTeamEntity(target, options, 1, true, false),
          "leaving training restores unknown-team filtering");
    target.teamKnown = true;
    target.team = 1;
    check(skipTeamEntity(target, options, 1, true), "known allies remain hidden in matches");
    target.team = 2;
    check(!skipTeamEntity(target, options, 1, true), "known opponents remain visible in matches");
    options.teamFilter = false;
    check(!skipTeamEntity(target, options, 0, false), "unchecked filter allows targets without local team");
    return failures ? 1 : 0;
}
