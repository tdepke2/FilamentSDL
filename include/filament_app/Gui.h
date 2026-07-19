#pragma once

#include <filament/TextureSampler.h>
#include <unordered_set>
#include <utils/Entity.h>
#include <vector>

namespace filament {
    class Camera;
    class Engine;
    class IndexBuffer;
    class Material;
    class MaterialInstance;
    class Scene;
    class Texture;
    class VertexBuffer;
    class View;
}

struct ImDrawData;
struct ImGuiIO;
struct ImGuiContext;
union SDL_Event;
struct SDL_Window;

namespace filament_app {

class Gui {
public:
    Gui(SDL_Window* window, filament::Engine* engine);
    ~Gui();

    void createAtlasTexture();
    void processEvent(const SDL_Event* event);

private:
    void startFrame();
    void render();
    void processImGuiCommands(ImDrawData* commands, const ImGuiIO& io);
    void syncThreads();
    void createVertexBuffer(size_t bufferIndex, size_t capacity);
    void createIndexBuffer(size_t bufferIndex, size_t capacity);
    void createBuffers(int numRequiredBuffers);
    void populateVertexData(size_t bufferIndex, size_t vbSizeInBytes, void* vbImguiData, size_t ibSizeInBytes, void* ibImguiData);

    ImGuiContext* imGuiContext_ = nullptr;
    filament::Engine* engine_ = nullptr;

    std::vector<filament::VertexBuffer*> vertexBuffers_;
    std::vector<filament::IndexBuffer*> indexBuffers_;
    filament::Material* material_ = nullptr;
    std::vector<filament::MaterialInstance*> materialInstances_;
    filament::Texture* texture_ = nullptr;
    filament::TextureSampler sampler_;
    bool hasSynced_ = false;
    std::unordered_set<filament::Texture*> imGuiTextures_;

    filament::View* view_ = nullptr;
    filament::Scene* scene_ = nullptr;
    utils::Entity cameraEntity_;
    filament::Camera* camera_ = nullptr;
    utils::Entity guiEntity_;

    friend class App;
};

} // namespace filament_app
