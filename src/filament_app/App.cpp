#include <filament_app/App.h>
#include <filament_app/Gui.h>

#include <backend/DriverEnums.h>
#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <iostream>
#include <SDL3/SDL.h>

namespace filament_app {

App::App() {

}

App::~App() {
    gui_.reset();

    engine_->destroy(renderer_);
    engine_->destroy(swapChain_);
    SDL_DestroyWindow(window_);
    filament::Engine::destroy(&engine_);
    engine_ = nullptr;

    SDL_Quit();
}

Gui* App::getGui() {
    return gui_.get();
}

filament::Engine* App::getEngine() {
    return engine_;
}

void App::init() {
    if (window_ != nullptr) {
        std::cerr << "App::init() called more than once.\n";
        abort();
    }
    SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

    std::cout << "TGD: create engine\n";
    engine_ = filament::Engine::Builder()
        .backend(filament::Engine::Backend::VULKAN)
        .build();

    SDL_WindowFlags windowFlags = SDL_WINDOW_HIGH_PIXEL_DENSITY;  // FIXME: do we need vulkan window? SDL_WINDOW_VULKAN;// | SDL_WINDOW_RESIZABLE;
    window_ = SDL_CreateWindow("Filament Test", 640, 480, windowFlags);
    if (window_ == nullptr) {
        std::cerr << "SDL failed to create window: " << SDL_GetError() << "\n";
        abort();
    }

    std::cout << "TGD: create swapchain\n";
    void* nativeWindow = nullptr;
    #if defined(SDL_PLATFORM_WIN32)
        nativeWindow = SDL_GetPointerProperty(SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    #else
        // FIXME: more platforms need support
        // https://github.com/libsdl-org/SDL/blob/main/docs/README-migration.md#sdl_syswmh
        static_assert(false, "platform is not yet supported");
    #endif
    swapChain_ = engine_->createSwapChain(nativeWindow);  // FIXME: filamentapp uses flags: , filament::SwapChain::CONFIG_HAS_STENCIL_BUFFER);
    std::cout << "TGD: create renderer\n";
    renderer_ = engine_->createRenderer();
    std::cout << "TGD: ready to render\n";

    // FIXME: FilamentApp::Window::Window() creates cameras and views, runs configureCamerasForWindow().

    // FIXME: FilamentApp::run() does other nonsense after creating the window.

    gui_ = std::make_unique<Gui>(window_, engine_);
}

void App::processEvent(const SDL_Event* event) {
    gui_->processEvent(event);
}

bool App::renderIsReady() const {
    return renderer_->shouldRenderFrame();
}

bool App::startRender(filament::View* view) {
    if (!renderer_->beginFrame(swapChain_)) {
        return false;
    }

    static unsigned int frame = 0;
    ++frame;
    if (frame % 100 == 0) {
        std::cout << "frame " << frame << "\n";
    }
    gui_->view_->setViewport({ 0, 0, 640, 480 });    // FIXME: again with the hardcoded window size. where should we set the viewport size, and what about projection?
    gui_->camera_->setProjection(filament::Camera::Projection::ORTHO, 0.0, 640, 480, 0.0, 0.0, 1.0);
    renderer_->render(view);
    renderer_->render(gui_->view_);  // FIXME: accessor for view
    renderer_->endFrame();
    return true;
}

}
