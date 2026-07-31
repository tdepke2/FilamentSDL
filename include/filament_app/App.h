// Based on the following implementations:
// https://github.com/google/filament/tree/main/libs/filamentapp
// https://github.com/ambrusc/filament-glfw-imgui

#pragma once

#include <memory>

namespace filament {
    class SwapChain;
    class Engine;
    class Renderer;
    class View;
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
    // we could keep this accessible and allow app to instantiate the gui only if it is needed.
    filament::Engine* getEngine();

    void init();
    void processEvent(const SDL_Event* event);

    // Returns false if filament is not ready to render the frame yet (due to fps limiting).
    bool renderIsReady() const;
    // Also returns false for the reasons above.
    bool startRender(filament::View* view);

private:
    SDL_Window* window_ = nullptr;
    filament::Engine* engine_ = nullptr;
    filament::SwapChain* swapChain_ = nullptr;
    filament::Renderer* renderer_ = nullptr;

    std::unique_ptr<Gui> gui_ = nullptr;
};

} // namespace filament_app
