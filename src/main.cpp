#include <filament_app/App.h>
#include <filament_app/Gui.h>
#include <iostream>
#include <SDL3/SDL.h>
#include <imgui.h>

#include <thread>
#include <chrono>

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <utils/EntityManager.h>
#include <filament/LightManager.h>
#include <filament/View.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>
#include <filament/Viewport.h>
#include <filameshio/MeshReader.h>
#include <math/scalar.h>
#include <camutils/Manipulator.h>
#include <ktxreader/Ktx2Reader.h>
#include <stb_image.h>
#include <fstream>
#include <image/LinearImage.h>
#include <filament-iblprefilter/IBLPrefilterContext.h>
#include <filament/IndirectLight.h>

#include <generated/resources/resources.h>

struct Vertex {
    filament::math::float2 position;
    uint32_t color;
};

namespace math = filament::math;

static const Vertex TRIANGLE_VERTICES[3] = {
    {{1, 0}, 0xffff0000u},
    {{cos(math::d::PI * 2 / 3), sin(math::d::PI * 2 / 3)}, 0xff00ff00u},
    {{cos(math::d::PI * 4 / 3), sin(math::d::PI * 4 / 3)}, 0xff0000ffu},
};

static constexpr uint16_t TRIANGLE_INDICES[3] = { 0, 1, 2 };

static filament::Texture* loadNormalMap(filament::Engine* engine, const uint8_t* normals, size_t nbytes) {
    int w, h, n;
    unsigned char* data = stbi_load_from_memory(normals, nbytes, &w, &h, &n, 3);
    filament::Texture* normalMap = filament::Texture::Builder()
            .width(uint32_t(w))
            .height(uint32_t(h))
            .levels(0xff)
            .format(filament::Texture::InternalFormat::RGB8)
            .usage(filament::Texture::Usage::DEFAULT | filament::Texture::Usage::GEN_MIPMAPPABLE)
            .build(*engine);
    filament::Texture::PixelBufferDescriptor buffer(data, size_t(w * h * 3),
            filament::Texture::Format::RGB, filament::Texture::Type::UBYTE,
            (filament::Texture::PixelBufferDescriptor::Callback) &stbi_image_free);
    normalMap->setImage(*engine, 0, std::move(buffer));
    normalMap->generateMipmaps(*engine);
    return normalMap;
}

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

int main() {
    filament_app::App app;
    app.init();

    // Add a custom font. Can be skipped to use the default font.
    //app.getGui()->addFontTTF("Roboto", const_cast<uint8_t*>(RESOURCES_ROBOTO_MEDIUM_DATA), RESOURCES_ROBOTO_MEDIUM_SIZE, 18);

    auto engine = app.getEngine();
    filament::View* view = engine->createView();
    filament::Scene* scene = engine->createScene();

    view->setViewport({ 0, 0, 640, 480 });  // FIXME: just hardcode window size for now
    view->setScene(scene);

    auto cameraMan = filament::camutils::Manipulator<float>::Builder()
        .viewport(640, 480) // FIXME
        .orbitHomePosition(2, 2, 2)
        .build(filament::camutils::Mode::ORBIT);

    // Copied from filament/samples/hellotriangle.cpp
    /*auto skybox = filament::Skybox::Builder().color({0.1, 0.125, 0.25, 1.0}).build(*engine);
    scene->setSkybox(skybox);
    //view->setPostProcessingEnabled(false);
    static_assert(sizeof(Vertex) == 12, "Strange vertex size.");
    auto vb = filament::VertexBuffer::Builder()
            .vertexCount(3)
            .bufferCount(1)
            .attribute(filament::VertexAttribute::POSITION, 0, filament::VertexBuffer::AttributeType::FLOAT2, 0, 12)
            .attribute(filament::VertexAttribute::COLOR, 0, filament::VertexBuffer::AttributeType::UBYTE4, 8, 12)
            .normalized(filament::VertexAttribute::COLOR)
            .build(*engine);
    vb->setBufferAt(*engine, 0,
            filament::VertexBuffer::BufferDescriptor(TRIANGLE_VERTICES, 36, nullptr));
    auto ib = filament::IndexBuffer::Builder()
            .indexCount(3)
            .bufferType(filament::IndexBuffer::IndexType::USHORT)
            .build(*engine);
    ib->setBuffer(*engine,
            filament::IndexBuffer::BufferDescriptor(TRIANGLE_INDICES, 6, nullptr));
    auto mat = filament::Material::Builder()
            .package(RESOURCES_BAKEDCOLOR_DATA, RESOURCES_BAKEDCOLOR_SIZE)
            .build(*engine);
    auto renderable = utils::EntityManager::get().create();
    filament::RenderableManager::Builder(1)
            .boundingBox({{ -1, -1, -1 }, { 1, 1, 1 }})
            .material(0, mat->getDefaultInstance())
            .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES, vb, ib, 0, 3)
            .culling(false)
            .receiveShadows(false)
            .castShadows(false)
            .build(*engine, renderable);
    scene->addEntity(renderable);*/
    /*auto camera = utils::EntityManager::get().create();
    auto cam = engine->createCamera(camera);
    view->setCamera(cam);*/



    // Copied from hellopbr sample.
    /*auto& tcm = engine->getTransformManager();
    auto& rcm = engine->getRenderableManager();
    auto& em = utils::EntityManager::get();

    // Instantiate material.
    auto material = filament::Material::Builder()
        .package(RESOURCES_AIDEFAULTMAT_DATA, RESOURCES_AIDEFAULTMAT_SIZE).build(*engine);
    auto mi = material->createInstance();
    mi->setParameter("baseColor", filament::RgbType::LINEAR, filament::math::float3{0.8});
    mi->setParameter("metallic", 1.0f);
    mi->setParameter("roughness", 0.4f);
    mi->setParameter("reflectance", 0.5f);

    // Add geometry into the scene.
    auto mesh = filamesh::MeshReader::loadMeshFromBuffer(engine, RESOURCES_MONKEY_DATA, RESOURCES_MONKEY_SIZE, nullptr, nullptr, mi);
    //auto ti = tcm.getInstance(mesh.renderable);
    //auto transform = filament::math::mat4f{ filament::math::mat3f(1), filament::math::float3(0, 0, -1) } * tcm.getWorldTransform(ti);
    rcm.setCastShadows(rcm.getInstance(mesh.renderable), false);
    scene->addEntity(mesh.renderable);

    // Add light sources into the scene.
    auto light = em.create();
    filament::LightManager::Builder(filament::LightManager::Type::SUN)
            .color(filament::Color::toLinear<filament::ACCURATE>(filament::sRGBColor(0.98f, 0.92f, 0.89f)))
            .intensity(110000)
            .direction({ 0.7, -1, -0.8 })
            .sunAngularRadius(1.9f)
            .castShadows(false)
            .build(*engine, light);
    scene->addEntity(light);*/



    // Copied from suzanne sample.
    auto& tcm = engine->getTransformManager();
    auto& rcm = engine->getRenderableManager();
    auto& em = utils::EntityManager::get();

    ktxreader::Ktx2Reader reader(*engine);

    reader.requestFormat(filament::Texture::InternalFormat::DXT3_SRGBA);
    reader.requestFormat(filament::Texture::InternalFormat::DXT3_RGBA);

    // Uncompressed formats are lower priority, so they get added last.
    reader.requestFormat(filament::Texture::InternalFormat::SRGB8_A8);
    reader.requestFormat(filament::Texture::InternalFormat::RGBA8);

    constexpr auto sRGB = ktxreader::Ktx2Reader::TransferFunction::sRGB;
    constexpr auto LINEAR = ktxreader::Ktx2Reader::TransferFunction::LINEAR;

    auto albedo = reader.load(RESOURCES_ALBEDO_DATA, RESOURCES_ALBEDO_SIZE, sRGB);
    auto ao = reader.load(RESOURCES_AO_DATA, RESOURCES_AO_SIZE, LINEAR);
    auto metallic = reader.load(RESOURCES_METALLIC_DATA, RESOURCES_METALLIC_SIZE, LINEAR);
    auto roughness = reader.load(RESOURCES_ROUGHNESS_DATA, RESOURCES_ROUGHNESS_SIZE, LINEAR);

    std::cout << "Resolved format for albedo: " << static_cast<uint16_t>(albedo->getFormat()) << "\n";
    std::cout << "Resolved format for ambient occlusion: " << static_cast<uint16_t>(ao->getFormat()) << "\n";
    std::cout << "Resolved format for metallic: " << static_cast<uint16_t>(metallic->getFormat()) << "\n";
    std::cout << "Resolved format for roughness: " << static_cast<uint16_t>(roughness->getFormat()) << "\n";

    // Note: we could just load the normal from a ktx2 texture instead. Here we just load from the png to show Filament doing mipmap generation.
    auto normal = loadNormalMap(engine, RESOURCES_NORMAL_DATA, RESOURCES_NORMAL_SIZE);
    filament::TextureSampler sampler(filament::TextureSampler::MinFilter::LINEAR_MIPMAP_LINEAR,
            filament::TextureSampler::MagFilter::LINEAR);

    // Instantiate material.
    auto material = filament::Material::Builder()
            .package(RESOURCES_TEXTUREDLIT_DATA, RESOURCES_TEXTUREDLIT_SIZE).build(*engine);
    auto materialInstance = material->createInstance();
    materialInstance->setParameter("albedo", albedo, sampler);
    materialInstance->setParameter("ao", ao, sampler);
    materialInstance->setParameter("metallic", metallic, sampler);
    materialInstance->setParameter("normal", normal, sampler);
    materialInstance->setParameter("roughness", roughness, sampler);

    //auto ibl = FilamentApp::get().getIBL()->getIndirectLight();
    //ibl->setIntensity(100000);
    //ibl->setRotation(mat3f::rotation(0.5f, float3{ 0, 1, 0 }));

    // Add geometry into the scene.
    auto mesh = filamesh::MeshReader::loadMeshFromBuffer(engine, RESOURCES_MONKEY_DATA, RESOURCES_MONKEY_SIZE, nullptr,
            nullptr, materialInstance);
    auto ti = tcm.getInstance(mesh.renderable);
    //auto transform = math::mat4f{ math::mat3f(1), math::float3(0, 0, -4) } * tcm.getWorldTransform(ti);
    rcm.setCastShadows(rcm.getInstance(mesh.renderable), false);
    scene->addEntity(mesh.renderable);
    //tcm.setTransform(ti, transform);



    // FIXME: we shall figure out ibl later.
    /*auto light = em.create();
    filament::LightManager::Builder(filament::LightManager::Type::SUN)
            .color(filament::Color::toLinear<filament::ACCURATE>(filament::sRGBColor(0.98f, 0.92f, 0.89f)))
            .intensity(110000)
            .direction({ 0.7, -1, -0.8 })
            .sunAngularRadius(1.9f)
            .castShadows(false)
            .build(*engine, light);
    scene->addEntity(light);*/



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




    auto camera = utils::EntityManager::get().create();
    auto cam = engine->createCamera(camera);
    view->setCamera(cam);



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

        /*constexpr float ZOOM = 1.5f;
        const uint32_t w = 640;    // FIXME: just hardcode window size for now
        const uint32_t h = 480;
        const float aspect = (float) w / h;
        cam->setProjection(filament::Camera::Projection::ORTHO,
            -aspect * ZOOM, aspect * ZOOM,
            -ZOOM, ZOOM, 0, 1);*/
        //auto& tcm = engine->getTransformManager();
        //tcm.setTransform(tcm.getInstance(renderable),
        //    filament::math::mat4f::rotation(now, filament::math::float3{ 0, 0, 1 }));

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

        //auto& tcm = engine->getTransformManager();
        //auto ti = tcm.getInstance(mesh.renderable);
        //tcm.setTransform(ti, transform * filament::math::mat4f::rotation(now, filament::math::float3{ 0, 1, 0 }));


        app.getGui()->startFrame();

        // Some ImGui UI to test.
        ImGui::ShowDemoWindow();

        app.getGui()->endFrame();
        app.startRender(view);
    }

    engine->destroy(view);
    delete cameraMan;
    engine->destroy(scene);
    /*engine->destroy(skybox);
    engine->destroy(renderable);
    engine->destroy(mat);
    engine->destroy(vb);
    engine->destroy(ib);
    engine->destroyCameraComponent(camera);
    utils::EntityManager::get().destroy(camera);

    //engine->destroy(view);
    //engine->destroy(scene);
    engine->destroy(light);
    engine->destroy(mesh.renderable);
    engine->destroy(mi);
    engine->destroy(material);*/
    engine->destroyCameraComponent(camera);
    utils::EntityManager::get().destroy(camera);

    engine->destroy(mesh.renderable);
    engine->destroy(materialInstance);
    engine->destroy(material);
    engine->destroy(albedo);
    engine->destroy(normal);
    engine->destroy(roughness);
    engine->destroy(metallic);
    engine->destroy(ao);
    ibl.reset();

    return 0;
}
