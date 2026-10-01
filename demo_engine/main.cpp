#include <filament_app/App.h>
#include <filament_app/Gui.h>
#include <filament_app/IBL.h>

#include <camutils/Manipulator.h>
#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/Texture.h>
#include <filament/TransformManager.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <gltfio/Animator.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/MaterialProvider.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/TextureProvider.h>
#include <utils/EntityManager.h>
#include <utils/NameComponentManager.h>

#include <chrono>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <set>
#include <SDL3/SDL.h>

#include <generated/resources/resources.h>

namespace gltfio = filament::gltfio;
namespace math = filament::math;

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



int main(int argc, char* argv[]) {
    auto app = filament_app::App::Builder().build();
    app.init("Filament Test");
    auto gui = app.createGui();

    auto engine = app.getEngine();
    filament::View* view = engine->createView();
    filament::Scene* scene = engine->createScene();

    view->setScene(scene);

    auto camera = utils::EntityManager::get().create();
    auto cam = engine->createCamera(camera);
    view->setCamera(cam);

    auto cameraMan = filament::camutils::Manipulator<float>::Builder()
        .orbitHomePosition(80, 80, 80)
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
    const char* filename = argv[1];//"../filament/third_party/models/DamagedHelmet/DamagedHelmet.glb";
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
        if (!in.read(reinterpret_cast<char*>(buffer.data()), contentSize)) {
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

        // Fetch the raw bytes and push them into ResourceLoader via
        // addResourceData(). This pre-populates the cache and bypasses
        // ResourceLoader's internal disk I/O.
        /*for (size_t i = 0, c = asset->getResourceUriCount(); i < c; ++i) {
            const char* uri = asset->getResourceUris()[i];
            utils::Path uriPath = filename.getParent() + uri;
            std::vector<uint8_t> buffer = app.rawAssetLoader->load(uriPath);
            if (!buffer.empty()) {
                auto* b = new std::vector<uint8_t>(std::move(buffer));
                gltfio::ResourceLoader::BufferDescriptor desc(
                    b->data(), b->size(),
                    [](void*, size_t, void* user) {
                        delete static_cast<std::vector<uint8_t>*>(user);
                    },
                    b
                );
                app.resourceLoader->addResourceData(uri, std::move(desc));
            }
        }*/

        if (!resourceLoader->asyncBeginLoad(asset)) {
            std::cerr << "Unable to start loading resources for " << filename << std::endl;
            exit(1);
        }

        if (/*app->recomputeAabb*/false) {    // FIXME
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



    auto ibl = std::make_unique<filament_app::IBL>(*engine);
    if (!ibl->loadFromEquirect("assets/environments/lightroom_14b.hdr")) {
        std::cerr << "Could not load the specified IBL." << std::endl;
        ibl.reset();
    } else {
        //ibl->mSkybox->setLayerMask(0x7, 0x4);
        scene->setSkybox(ibl->mSkybox);
        scene->setIndirectLight(ibl->mIndirectLight);

        //ibl->mIndirectLight->setIntensity(100000);
    }

    auto windowResized = [&app, &view, &cameraMan, &cam]() {
        uint32_t renderWidth, renderHeight;
        app.getWindowSizeInPixels(&renderWidth, &renderHeight);

        view->setViewport({ 0, 0, renderWidth, renderHeight });
        cameraMan->setViewport(renderWidth, renderHeight);

        double focalLength = 28.0f;
        double near = 0.1f;
        double far = 100.0f;
        double aspectRatio = static_cast<double>(renderWidth) / renderHeight;
        cam->setLensProjection(focalLength, aspectRatio, near, far);
    };
    windowResized();

    SDL_Event event;
    bool closeWindow = false;
    while (!closeWindow) {
        while (SDL_PollEvent(&event)) {
            if (app.processEvent(&event)) {
                continue;
            }

            if (event.type == SDL_EVENT_QUIT) {
                closeWindow = true;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                cameraMan->grabBegin(event.button.x, -event.button.y, false);
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                cameraMan->grabEnd();
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                cameraMan->grabUpdate(event.motion.x, -event.motion.y);
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                windowResized();
            }
        }

        if (!app.renderIsReady()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        double now = static_cast<double>(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency();



        //cam->lookAt({2, 2, 2}, {0, 0, 0}, {0, 1, 0});
        math::float3 eye, center, up;
        cameraMan->getLookAt(&eye, &center, &up);
        cam->lookAt(eye, center, up);



        resourceLoader->asyncUpdateLoad();
        names->gc();
        assetLoader->gc();

        static constexpr int kNumAvailable = 128;
        utils::Entity renderables[kNumAvailable];
        while (size_t numWritten = asset->popRenderables(renderables, kNumAvailable)) {
            gltfio::FilamentAsset::SceneMask mask;
            mask.reset();
            mask.set(0);
            asset->addEntitiesToScene(*scene, renderables, numWritten, mask);
        }

        if (true) { // animSettings.enabled
            gltfio::Animator& animator = *instance->getAnimator();
            const size_t animationCount = animator.getAnimationCount();
            static double startTime = now;
            double elapsedSeconds = now - startTime;  // NOTE: the animation must start from t 0 to interpolate correctly (bug?)
            if (animationCount > 0) {
                animator.applyAnimation(0, elapsedSeconds);
            }
            animator.updateBoneMatrices();
        }

        /*auto& tcm = engine->getTransformManager();
        tcm.setTransform(tcm.getInstance(asset->getEntities()[0]),
          math::mat4f::rotation(now,
                  math::float3{ 0, 0, 1 }));*/



        gui->startFrame();

        // Some ImGui UI to test.
        ImGui::ShowDemoWindow();

        if (ImGui::CollapsingHeader("Stats")) {
            ImGui::Indent();
            ImGui::Text("%zu entities in the asset", asset->getEntityCount());
            ImGui::Text("%zu renderables (excluding UI)", scene->getRenderableCount());
            ImGui::Text("%f time", now);
            ImGui::Unindent();
        }

        gui->endFrame();
        app.startRender(view);
    }

    resourceLoader->asyncCancelLoad();
    assetLoader->destroyAsset(asset);
    materials->destroyMaterials();
    delete materials;
    delete names;
    delete resourceLoader;
    delete stbDecoder;
    delete ktxDecoder;
    delete webpDecoder;
    gltfio::AssetLoader::destroy(&assetLoader);

    engine->destroy(view);
    engine->destroy(scene);
    engine->destroyCameraComponent(camera);
    utils::EntityManager::get().destroy(camera);
    delete cameraMan;
    ibl.reset();

    return 0;
}
