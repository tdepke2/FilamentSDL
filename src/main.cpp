#include <Common.h>

#include <filament/FilamentAPI.h>
#include <filament/Engine.h>

//#include <spdlog/spdlog.h>

using namespace filament;

int main() {
    F f;
    //spdlog::info("spdlog works, x = {}", f.x);

    Engine *engine = Engine::create();
    engine->destroy(&engine);

    return 0;
}
