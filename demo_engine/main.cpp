#include <filament_app/App.h>
#include <filament_app/Gui.h>

#include <camutils/Manipulator.h>
#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/IndirectLight.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/Texture.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <filament-iblprefilter/IBLPrefilterContext.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/MaterialProvider.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/TextureProvider.h>
#include <utils/EntityManager.h>
#include <utils/NameComponentManager.h>

#include <fstream>
#include <imgui.h>
#include <iostream>
#include <set>
#include <SDL3/SDL.h>
#include <stb_image.h>

#include <generated/resources/resources.h>

namespace gltfio = filament::gltfio;
namespace math = filament::math;



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



enum MaterialSource {
    JITSHADER,
    UBERSHADER,
};
static constexpr MaterialSource materialSource = JITSHADER;


// FIXME: put in resources
#define UBERARCHIVE_DEFAULT_DATA nullptr
#define UBERARCHIVE_DEFAULT_SIZE 0

static std::ifstream::pos_type getFileSize(const char* filename) {
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}



int main() {
    filament_app::App app;
    app.init();

    auto engine = app.getEngine();
    filament::View* view = engine->createView();
    filament::Scene* scene = engine->createScene();

    view->setViewport({ 0, 0, 640, 480 });  // FIXME: just hardcode window size for now
    view->setScene(scene);

    auto camera = utils::EntityManager::get().create();
    auto cam = engine->createCamera(camera);
    view->setCamera(cam);

    auto cameraMan = filament::camutils::Manipulator<float>::Builder()
        .viewport(640, 480) // FIXME
        .orbitHomePosition(2, 2, 2)
        .build(filament::camutils::Mode::ORBIT);



    auto names = new utils::NameComponentManager(utils::EntityManager::get());
    engine->enableAccurateTranslations();

    gltfio::MaterialProvider* materials = nullptr;
    if (materialSource == JITSHADER) {
        materials = gltfio::createJitShaderProvider(engine, false, {});  // FIXME: see FILAMENT_DISABLE_MATOPT in cmakelists, we will need something similar.
    } else {
        materials = gltfio::createUbershaderProvider(engine, UBERARCHIVE_DEFAULT_DATA, UBERARCHIVE_DEFAULT_SIZE);
    }

    // Load asset.
    const char* filename = "../filament/third_party/models/FlightHelmet/FlightHelmet.gltf";
    auto assetLoader = gltfio::AssetLoader::create({ engine, materials, names });
    gltfio::FilamentAsset* asset = nullptr;
    gltfio::FilamentInstance* instance = nullptr;
    {
        // Peek at the file size to allow pre-allocation.
        const long contentSize = static_cast<long>(getFileSize(filename));
        if (contentSize <= 0) {
            std::cerr << "Unable to open " << filename << std::endl;
            exit(1);
        }

        // Consume the glTF file.
        std::ifstream in(filename, std::ifstream::binary | std::ifstream::in);
        std::vector<uint8_t> buffer(static_cast<unsigned long>(contentSize));
        if (!in.read((char*) buffer.data(), contentSize)) {
            std::cerr << "Unable to read " << filename << std::endl;
            exit(1);
        }

        // Parse the glTF file and create Filament entities.
        asset = assetLoader->createAsset(buffer.data(), buffer.size());
        if (!asset) {
            std::cerr << "Unable to parse " << filename << std::endl;
            exit(1);
        }

        // pre-compile all material variants
        std::set<filament::Material*> materials;
        const filament::RenderableManager& rcm = engine->getRenderableManager();
        const utils::Slice<const utils::Entity> renderables{ asset->getRenderableEntities(),
            asset->getRenderableEntityCount() };
        for (const utils::Entity e : renderables) {
            auto ri = rcm.getInstance(e);
            const size_t c = rcm.getPrimitiveCount(ri);
            for (size_t i = 0; i < c; ++i) {
                const filament::MaterialInstance* mi = rcm.getMaterialInstanceAt(ri, i);
                filament::Material* ma = const_cast<filament::Material*>(mi->getMaterial());  // FIXME: is there a safer way to do this?
                materials.insert(ma);
            }
        }
        for (filament::Material* ma : materials) {
            // First compile high priority variants
            ma->compile(filament::Material::CompilerPriorityQueue::HIGH,
                filament::UserVariantFilterBit::DIRECTIONAL_LIGHTING |
                filament::UserVariantFilterBit::DYNAMIC_LIGHTING |
                filament::UserVariantFilterBit::SHADOW_RECEIVER);

            // and then, everything else at low priority, except STE, which is very uncommon.
            ma->compile(filament::Material::CompilerPriorityQueue::LOW,
                filament::UserVariantFilterBit::FOG |
                filament::UserVariantFilterBit::SKINNING |
                filament::UserVariantFilterBit::SSR |
                filament::UserVariantFilterBit::VSM);
        }

        instance = asset->getInstance();
    }

    // Load resources.
    gltfio::ResourceLoader* resourceLoader = nullptr;
    gltfio::TextureProvider* stbDecoder = nullptr;
    gltfio::TextureProvider* ktxDecoder = nullptr;
    gltfio::TextureProvider* webpDecoder = nullptr;
    {
        gltfio::ResourceConfiguration configuration = {};
        configuration.engine = engine;
        configuration.gltfPath = filename;
        configuration.normalizeSkinningWeights = true;

        if (!resourceLoader) {
            resourceLoader = new gltfio::ResourceLoader(configuration);
            stbDecoder = gltfio::createStbProvider(engine);
            ktxDecoder = gltfio::createKtx2Provider(engine);
            resourceLoader->addTextureProvider("image/png", stbDecoder);
            resourceLoader->addTextureProvider("image/jpeg", stbDecoder);
            resourceLoader->addTextureProvider("image/ktx2", ktxDecoder);
            if (gltfio::isWebpSupported()) {
                webpDecoder = gltfio::createWebpProvider(engine);
                resourceLoader->addTextureProvider("image/webp", webpDecoder);
            }
        } else {
            resourceLoader->setConfiguration(configuration);
        }

        if (!resourceLoader->asyncBeginLoad(asset)) {
            std::cerr << "Unable to start loading resources for " << filename << std::endl;
            exit(1);
        }

        if (/*app->recomputeAabb*/true) {    // FIXME
            asset->getInstance()->recomputeBoundingBoxes();
        }

        asset->releaseSourceData();

        // Enable stencil writes on all material instances.
        const size_t matInstanceCount = instance->getMaterialInstanceCount();
        auto instances = instance->getMaterialInstances();
        for (size_t mi = 0; mi < matInstanceCount; ++mi) {
            instances[mi]->setStencilWrite(true);
            instances[mi]->setStencilOpDepthStencilPass(filament::MaterialInstance::StencilOperation::INCR);
        }
    }



    auto ibl = std::make_unique<IBL>(*engine);
    if (!ibl->loadFromEquirect("assets/environments/lightroom_14b.hdr")) {
        std::cerr << "Could not load the specified IBL." << std::endl;
        ibl.reset();
    } else {
        //ibl->mSkybox->setLayerMask(0x7, 0x4);
        scene->setSkybox(ibl->mSkybox);
        scene->setIndirectLight(ibl->mIndirectLight);

        //ibl->mIndirectLight->setIntensity(100000);
    }



    SDL_Event event;
    bool closeWindow = false;
    while (!closeWindow) {
        while (SDL_PollEvent(&event)) {
            app.processEvent(&event);

            if (event.type == SDL_EVENT_QUIT) {
                closeWindow = true;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                cameraMan->grabBegin(event.button.x, -event.button.y, false);
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                cameraMan->grabEnd();
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                cameraMan->grabUpdate(event.motion.x, -event.motion.y);
            }
        }

        if (!app.renderIsReady()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        double now = (double) SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();



        const uint32_t w = 640;    // FIXME: just hardcode window size for now
        const uint32_t h = 480;
        double focalLength = 28.0f;
        double near = 0.1f;
        double far = 100.0f;
        auto aspectRatio = (float) w / h;
        cam->setLensProjection(focalLength, aspectRatio, near, far);
        //cam->lookAt({2, 2, 2}, {0, 0, 0}, {0, 1, 0});
        filament::math::float3 eye, center, up;
        cameraMan->getLookAt(&eye, &center, &up);
        cam->lookAt(eye, center, up);



        app.getGui()->startFrame();

        // Some ImGui UI to test.
        ImGui::ShowDemoWindow();

        app.getGui()->endFrame();
        app.startRender(view);
    }

    engine->destroy(view);
    engine->destroy(scene);
    engine->destroyCameraComponent(camera);
    utils::EntityManager::get().destroy(camera);
    delete cameraMan;
    ibl.reset();

    return 0;
}
