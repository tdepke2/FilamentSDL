// Based on the following implementations:
// https://github.com/google/filament/tree/main/libs/filamentapp
// https://github.com/ambrusc/filament-glfw-imgui

#pragma once

#include <backend/DriverEnums.h>
#include <backend/Platform.h>
#include <memory>
#include <string>

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
    class Builder {
    public:
        Builder& backend(filament::backend::Backend backend) {
            backend_ = backend;
            return *this;
        }
        Builder& platform(filament::backend::Platform* platform) {
            platform_ = platform;
            return *this;
        }
        Builder& featureLevel(filament::backend::FeatureLevel featureLevel) {
            featureLevel_ = featureLevel;
            return *this;
        }
        App build();

    private:
        filament::backend::Backend backend_ = filament::backend::Backend::DEFAULT;
        filament::backend::Platform* platform_ = nullptr;
        filament::backend::FeatureLevel featureLevel_ = filament::backend::FeatureLevel::FEATURE_LEVEL_3;

        friend class App;
    };

    App(const App& rhs) = delete;
    App& operator=(const App& rhs) = delete;
    ~App();

    void init(std::string title, uint32_t width = 640, uint32_t height = 480, bool resizable = true);
    Gui* createGui();

    SDL_Window* getWindow() const;
    filament::Engine* getEngine() const;
    Gui* getGui() const;

    void getWindowSize(uint32_t* width, uint32_t* height) const;
    void getWindowSizeInPixels(uint32_t* width, uint32_t* height) const;

    // Returns true if event was consumed (and should not be processed further).
    bool processEvent(const SDL_Event* event);

    // Returns false if filament is not ready to render the frame yet (due to fps limiting).
    bool renderIsReady() const;
    // Also returns false for the reasons above.
    bool startRender(filament::View* view);

private:
    explicit App(const Builder& builder);

    filament::backend::Backend backend_ = filament::backend::Backend::DEFAULT;
    filament::backend::Platform* platform_ = nullptr;
    filament::backend::FeatureLevel featureLevel_ = filament::backend::FeatureLevel::FEATURE_LEVEL_3;

    SDL_Window* window_ = nullptr;
    filament::Engine* engine_ = nullptr;
    filament::SwapChain* swapChain_ = nullptr;
    filament::Renderer* renderer_ = nullptr;

    std::unique_ptr<Gui> gui_ = nullptr;
};

} // namespace filament_app
