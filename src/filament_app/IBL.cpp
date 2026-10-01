#include <filament_app/IBL.h>

#include <filament/Engine.h>
#include <filament/IndirectLight.h>
#include <filament/Skybox.h>
#include <filament-iblprefilter/IBLPrefilterContext.h>

#include <iostream>
#include <stb_image.h>

namespace math = filament::math;

namespace filament_app {

IBL::~IBL() {
    mEngine.destroy(mIndirectLight);
    mEngine.destroy(mTexture);
    mEngine.destroy(mSkybox);
    mEngine.destroy(mSkyboxTexture);
    mEngine.destroy(mFogTexture);
}

bool IBL::loadFromEquirect(const std::string& path) {
    int w = 0, h = 0;
    int n = 0;
    size_t size = 0;
    void* data = nullptr;
    void* user = nullptr;
    filament::Texture::PixelBufferDescriptor::Callback destroyer{};

    if (false/*path.getExtension() == "exr"*/) {
        // ???
    } else {
        stbi_info(path.c_str(), &w, &h, nullptr);
        // load image as float
        size = w * h * sizeof(math::float3);
        data = (math::float3*)stbi_loadf(path.c_str(), &w, &h, &n, 3);
        destroyer = [](void* data, size_t, void*) {
            stbi_image_free(data);
        };
    }

    if (data == nullptr || n != 3) {
        std::cerr << "Could not decode image " << std::endl;
        destroyer(data, size, user);
        return false;
    }

    if (w != h * 2) {
        std::cerr << "not an equirectangular image!" << std::endl;
        destroyer(data, size, user);
        return false;
    }

    // now load texture
    filament::Texture::PixelBufferDescriptor buffer(
            data, size,filament::Texture::Format::RGB, filament::Texture::Type::FLOAT, destroyer, user);

    filament::Texture* const equirect = filament::Texture::Builder()
            .width((uint32_t)w)
            .height((uint32_t)h)
            .levels(0xff)
            .format(filament::Texture::InternalFormat::R11F_G11F_B10F)
            .sampler(filament::Texture::Sampler::SAMPLER_2D)
            .usage(filament::Texture::Usage::DEFAULT | filament::Texture::Usage::GEN_MIPMAPPABLE)
            .build(mEngine);

    equirect->setImage(mEngine, 0, std::move(buffer));

    IBLPrefilterContext context(mEngine);
    IBLPrefilterContext::EquirectangularToCubemap equirectangularToCubemap(context);
    IBLPrefilterContext::SpecularFilter specularFilter(context);
    IBLPrefilterContext::IrradianceFilter irradianceFilter(context);

    mSkyboxTexture = equirectangularToCubemap(equirect);

    mEngine.destroy(equirect);

    mTexture = specularFilter(mSkyboxTexture);

    mFogTexture = irradianceFilter({ .generateMipmap = true }, mSkyboxTexture);
    mFogTexture->generateMipmaps(mEngine);

    mIndirectLight = filament::IndirectLight::Builder()
            .reflections(mTexture)
            .intensity(IBL_INTENSITY)
            .build(mEngine);

    mSkybox = filament::Skybox::Builder()
            .environment(mSkyboxTexture)
            .showSun(true)
            .build(mEngine);

    return true;
}

}
