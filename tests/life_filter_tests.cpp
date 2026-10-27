#include "../src/core/types.h"
#include <cstdio>
#include <limits>
int main() {
    int failures = 0;
    auto check = [&](bool pass, const char *name) {
        std::printf("%s %s\n", pass ? "PASS" : "FAIL", name);
        if (!pass)
            ++failures;
    };
    Options options;
    Entity entity;
    check(!skipDeadEntity(entity, options), "unknown default is not classified as dead");
    entity.health = 0;
    entity.healthKnown = false;
    check(!skipDeadEntity(entity, options), "missing health is not zero health");
    entity.healthKnown = true;
    entity.health = 100;
    entity.life = LifeState::Alive;
    check(!skipDeadEntity(entity, options), "living entity remains visible");
    entity.health = 0;
    entity.life = LifeState::Unknown;
    check(skipDeadEntity(entity, options), "known zero health is filtered");
    entity.health = 100;
    entity.life = LifeState::Dead;
    check(skipDeadEntity(entity, options), "explicit dead state overrides stale positive health");
    entity.healthKnown = false;
    entity.life = LifeState::Unknown;
    entity.health = std::numeric_limits<float>::quiet_NaN();
    check(!skipDeadEntity(entity, options), "invalid unknown health is not evidence of death");
    options.ignoreDead = false;
    entity.life = LifeState::Dead;
    check(!skipDeadEntity(entity, options), "disabled filter keeps confirmed dead entity");
    return failures ? 1 : 0;
}
