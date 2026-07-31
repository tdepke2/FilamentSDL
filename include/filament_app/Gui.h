#pragma once

#include <filament/TextureSampler.h>
#include <string>
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
struct ImFont;
struct ImGuiIO;
struct ImGuiContext;
union SDL_Event;
struct SDL_Window;

namespace filament_app {

class Gui {
public:
    Gui(SDL_Window* window, filament::Engine* engine);
    ~Gui();

    ImFont* addFontTTF(const std::string& name, void* data, size_t dataSize, float fontSize, bool freeDataWhenDone = false);
    void processEvent(const SDL_Event* event);
    // Always prepare the gui frame before rendering. If using custom fonts then load them before starting the frame.
    void startFrame();
    void endFrame();

private:
    void rebuildAtlasTexture();
    void processImGuiCommands(ImDrawData* commands, const ImGuiIO& io);
    void syncThreads();
    void createVertexBuffer(size_t bufferIndex, uint32_t capacity);
    void createIndexBuffer(size_t bufferIndex, uint32_t capacity);
    void createBuffers(size_t numRequiredBuffers);
    void populateVertexData(size_t bufferIndex, size_t vbSizeInBytes, void* vbImguiData, size_t ibSizeInBytes, void* ibImguiData);

    ImGuiContext* imGuiContext_ = nullptr;
    filament::Engine* engine_ = nullptr;

    std::vector<filament::VertexBuffer*> vertexBuffers_;
    std::vector<filament::IndexBuffer*> indexBuffers_;
    filament::Material* material_ = nullptr;
    std::vector<filament::MaterialInstance*> materialInstances_;
    filament::Texture* texture_ = nullptr;
    filament::TextureSampler sampler_;
    bool customFont_ = false;
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
