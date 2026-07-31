#include <Common.h>
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
#include <filament/View.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>
#include <filament/Viewport.h>

#include "../gen/res/filagui_resources.h"

struct Vertex {
    filament::math::float2 position;
    uint32_t color;
};

static const double M_PI = 3.1415927;

static const Vertex TRIANGLE_VERTICES[3] = {
    {{1, 0}, 0xffff0000u},
    {{cos(M_PI * 2 / 3), sin(M_PI * 2 / 3)}, 0xff00ff00u},
    {{cos(M_PI * 4 / 3), sin(M_PI * 4 / 3)}, 0xff0000ffu},
};

static constexpr uint16_t TRIANGLE_INDICES[3] = { 0, 1, 2 };

int main() {
    F f;
    std::cout << "start main, x = " << f.x << "\n";

    filament_app::App app;
    app.init();

    // Add a custom font. Can be skipped to use the default font.
    //app.getGui()->addFontTTF("Roboto", const_cast<uint8_t*>(FILAGUI_RESOURCES_ROBOTO_MEDIUM_DATA), FILAGUI_RESOURCES_ROBOTO_MEDIUM_SIZE, 18);

    auto engine = app.getEngine();
    filament::View* view = engine->createView();
    filament::Scene* scene = engine->createScene();

    view->setViewport({ 0, 0, 640, 480 });  // FIXME: just hardcode window size for now
    view->setScene(scene);

    // Copied from filament/samples/hellotriangle.cpp
    auto skybox = filament::Skybox::Builder().color({0.1, 0.125, 0.25, 1.0}).build(*engine);
    scene->setSkybox(skybox);
    view->setPostProcessingEnabled(false);
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
            .package(FILAGUI_RESOURCES_BAKEDCOLOR_DATA, FILAGUI_RESOURCES_BAKEDCOLOR_SIZE)
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
    scene->addEntity(renderable);
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
            }
        }

        if (!app.renderIsReady()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        constexpr float ZOOM = 1.5f;
        const uint32_t w = 640;    // FIXME: just hardcode window size for now
        const uint32_t h = 480;
        const float aspect = (float) w / h;
        cam->setProjection(filament::Camera::Projection::ORTHO,
            -aspect * ZOOM, aspect * ZOOM,
            -ZOOM, ZOOM, 0, 1);
        auto& tcm = engine->getTransformManager();
        double now = (double) SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
        tcm.setTransform(tcm.getInstance(renderable),
            filament::math::mat4f::rotation(now, filament::math::float3{ 0, 0, 1 }));

        app.getGui()->startFrame();

        // Some ImGui UI to test.
        ImGui::ShowDemoWindow();

        app.getGui()->endFrame();
        app.startRender(view);
    }

    engine->destroy(view);
    engine->destroy(scene);
    engine->destroy(skybox);
    engine->destroy(renderable);
    engine->destroy(mat);
    engine->destroy(vb);
    engine->destroy(ib);
    engine->destroyCameraComponent(camera);
    utils::EntityManager::get().destroy(camera);

    return 0;
}
