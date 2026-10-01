#pragma once

#include <math/vec3.h>
#include <string>

namespace filament {
    class Engine;
    class IndirectLight;
    class Skybox;
    class Texture;
}

namespace filament_app {

static constexpr float IBL_INTENSITY = 30000.0f;

struct IBL {
    IBL(filament::Engine& engine) : mEngine(engine) {}
    ~IBL();
    bool loadFromEquirect(const std::string& path);

    filament::Engine& mEngine;

    filament::math::float3 mBands[9] = {};
    bool mHasSphericalHarmonics = false;

    filament::Texture* mTexture = nullptr;
    filament::IndirectLight* mIndirectLight = nullptr;
    filament::Texture* mSkyboxTexture = nullptr;
    filament::Texture* mFogTexture = nullptr;
    filament::Skybox* mSkybox = nullptr;
};

} // namespace filament_app
