#include <filament_app/App.h>
#include <filament_app/Gui.h>

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <iostream>
#include <SDL3/SDL.h>

namespace filament_app {

App App::Builder::build() {
    return App(*this);
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

void App::init(std::string title, uint32_t width, uint32_t height, bool resizable) {
    if (window_ != nullptr) {
        std::cerr << "App::init() called more than once.\n";
        abort();
    }
    SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

    engine_ = filament::Engine::Builder()
        .backend(backend_)
        .platform(platform_)
        .featureLevel(featureLevel_)
        .build();

    SDL_WindowFlags windowFlags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (resizable) {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }
    window_ = SDL_CreateWindow(title.c_str(), width, height, windowFlags);
    if (window_ == nullptr) {
        std::cerr << "SDL failed to create window: " << SDL_GetError() << "\n";
        abort();
    }

    void* nativeWindow = nullptr;
    #if defined(SDL_PLATFORM_WIN32)
        nativeWindow = SDL_GetPointerProperty(SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    #else
        // FIXME: more platforms need support
        // https://github.com/libsdl-org/SDL/blob/main/docs/README-migration.md#sdl_syswmh
        static_assert(false, "platform is not yet supported");
    #endif
    swapChain_ = engine_->createSwapChain(nativeWindow);  // FIXME: filamentapp uses flags: , filament::SwapChain::CONFIG_HAS_STENCIL_BUFFER);
    renderer_ = engine_->createRenderer();

    // FIXME: FilamentApp::Window::Window() creates cameras and views, runs configureCamerasForWindow().

    // FIXME: FilamentApp::run() does other nonsense after creating the window.
}

Gui* App::createGui() {
    if (gui_) {
        std::cerr << "App::createGui() called more than once.\n";
        abort();
    }
    gui_ = std::make_unique<Gui>(window_, engine_);
    return gui_.get();
}

SDL_Window* App::getWindow() const {
    return window_;
}

filament::Engine* App::getEngine() const {
    return engine_;
}

Gui* App::getGui() const {
    return gui_.get();
}

void App::getWindowSize(uint32_t* width, uint32_t* height) const {
    int iw = 0, ih = 0;
    SDL_GetWindowSize(window_, &iw, &ih);
    *width = iw;
    *height = ih;
}

void App::getWindowSizeInPixels(uint32_t* width, uint32_t* height) const {
    int iw = 0, ih = 0;
    SDL_GetWindowSizeInPixels(window_, &iw, &ih);
    *width = iw;
    *height = ih;
}

bool App::processEvent(const SDL_Event* event) {
    if (gui_) {
        return gui_->processEvent(event);
    }
    return false;
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
        std::cout << "frame " << frame << "\n";    // FIXME
    }
    renderer_->render(view);
    if (gui_) {
        renderer_->render(gui_->getView());
    }
    renderer_->endFrame();
    return true;
}

App::App(const Builder& builder) :
    backend_(builder.backend_),
    platform_(builder.platform_),
    featureLevel_(builder.featureLevel_) {
}

}
