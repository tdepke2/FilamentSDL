#pragma once

// https://github.com/google/filament/tree/main/libs/filamentapp
// https://github.com/ambrusc/filament-glfw-imgui

#include <memory>

namespace filament {
    class SwapChain;
    class Engine;
    class Renderer;
}

union SDL_Event;
struct SDL_Window;

namespace filament_app {

class Gui;

class App {
public:
    App();
    ~App();

    Gui* getGui(); // FIXME: keep this inaccessible instead? this seems to make more sense. then Gui doesn't need to make App a friend.

    void init();
    void processEvent(const SDL_Event* event);
    bool run();

    void startGuiFrame();
    void endGuiFrame();
    bool startRender();

private:
    SDL_Window* window_ = nullptr;
    filament::Engine* engine_ = nullptr;
    filament::SwapChain* swapChain_ = nullptr;
    filament::Renderer* renderer_ = nullptr;

    std::unique_ptr<Gui> gui_ = nullptr;
};

} // namespace filament_app
