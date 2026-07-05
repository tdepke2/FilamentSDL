#pragma once

#include <filament/TextureSampler.h>
#include <utils/Entity.h>
#include <vector>

namespace filament {
    class Camera;
    class Engine;
    class Material;
    class MaterialInstance;
    class Scene;
    class Texture;
    class View;
}

struct ImDrawData;
struct ImGuiIO;
struct ImGuiContext;
union SDL_Event;

namespace filament_app {

class Gui {
public:
    Gui(filament::Engine* engine);
    ~Gui();

    void createAtlasTexture();
    void processEvent(const SDL_Event* event);

private:
    void startFrame();
    void render();
    void processImGuiCommands(ImDrawData* commands, const ImGuiIO& io);

    ImGuiContext* imGuiContext_ = nullptr;
    filament::Engine* engine_ = nullptr;

    filament::Material* material_ = nullptr;
    std::vector<filament::MaterialInstance*> materialInstances_;
    filament::Texture* texture_ = nullptr;
    filament::TextureSampler sampler_;

    filament::View* view_ = nullptr;
    filament::Scene* scene_ = nullptr;
    utils::Entity cameraEntity_;
    filament::Camera* camera_ = nullptr;
    utils::Entity guiEntity_;

    friend class App;
};

} // namespace filament_app
