#include <filament_app/App.h>
#include <filament_app/Gui.h>

#include <backend/DriverEnums.h>
#include <filament/Engine.h>
#include <filament/SwapChain.h>
#include <iostream>
#include <SDL3/SDL.h>

namespace filament_app {

App::App() {

}

App::~App() {
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

void App::init() {
    SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

    engine_ = filament::Engine::Builder()
        .backend(filament::Engine::Backend::VULKAN)
        .build();

    SDL_WindowFlags windowFlags = SDL_WINDOW_HIGH_PIXEL_DENSITY;  // FIXME: do we need vulkan window? SDL_WINDOW_VULKAN;// | SDL_WINDOW_RESIZABLE;
    window_ = SDL_CreateWindow("Filament Test", 640, 480, windowFlags);
    if (window_ == nullptr) {
        std::cerr << "SDL failed to create window: " << SDL_GetError() << "\n";
        abort();
    }

    swapChain_ = engine_->createSwapChain(window_);  // FIXME: filamentapp uses flags: , filament::SwapChain::CONFIG_HAS_STENCIL_BUFFER);
    renderer_ = engine_->createRenderer();

    // FIXME: FilamentApp::Window::Window() creates cameras and views.

    // FIXME: FilamentApp::run() does other nonsense after creating the window.
}

void App::processEvent(const SDL_Event* event) {
    gui_->processEvent(event);
}

bool App::run() {

}

void App::startGuiFrame() {
    gui_->startFrame();
}

void App::endGuiFrame() {
    gui_->render();
}

bool App::startRender() {

}

}
