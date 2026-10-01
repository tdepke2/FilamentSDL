#include <filament_app/Gui.h>

#include <backends/imgui_impl_sdl3.h>
#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/Fence.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/Texture.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <imgui.h>
#include <SDL3/SDL.h>
#include <utils/EntityManager.h>

#include <generated/resources/filament_app.h>

namespace filament_app {

Gui::Gui(SDL_Window* window, filament::Engine* engine) :
    window_(window),
    engine_(engine) {

    using namespace filament;

    IMGUI_CHECKVERSION();
    imGuiContext_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imGuiContext_);

    // Use a generic rendering backend in ImGui as we will convert the draw commands into Filament objects.
    ImGui_ImplSDL3_InitForOther(window);

    ImGui::StyleColorsDark();

    material_ = Material::Builder()
        .package(FILAMENT_APP_GUIBLIT_DATA, FILAMENT_APP_GUIBLIT_SIZE)
        .build(*engine_);

    auto& em = utils::EntityManager::get();

    view_ = engine_->createView();
    view_->setPostProcessingEnabled(false);
    view_->setBlendMode(View::BlendMode::TRANSLUCENT);
    view_->setShadowingEnabled(false);

    scene_ = engine_->createScene();

    cameraEntity_ = em.create();
    camera_ = engine_->createCamera(cameraEntity_);
    view_->setCamera(camera_);

    view_->setScene(scene_);
    guiEntity_ = em.create();
    scene_->addEntity(guiEntity_);

    windowResized();
}

Gui::~Gui() {
    engine_->destroy(guiEntity_);
    engine_->destroyCameraComponent(cameraEntity_);
    engine_->destroy(scene_);
    engine_->destroy(view_);

    auto& em = utils::EntityManager::get();
    em.destroy(guiEntity_);
    em.destroy(cameraEntity_);

    for (auto mi : materialInstances_) {
        engine_->destroy(mi);
    }
    engine_->destroy(material_);
    engine_->destroy(texture_);
    for (auto vb : vertexBuffers_) {
        engine_->destroy(vb);
    }
    for (auto ib : indexBuffers_) {
        engine_->destroy(ib);
    }

    for (auto tex : imGuiTextures_) {
        engine_->destroy(tex);
    }

    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(imGuiContext_);
}

filament::View* Gui::getView() const {
    return view_;
}

ImFont* Gui::addFontTTF(const std::string& name, void* data, size_t dataSize, float fontSize, bool freeDataWhenDone) {
    ImGui::SetCurrentContext(imGuiContext_);
    customFont_ = true;

    ImFontConfig fontConfig = {};
    fontConfig.FontDataOwnedByAtlas = freeDataWhenDone;
    std::memcpy(fontConfig.Name, name.c_str(), std::min(name.size() + 1, sizeof(fontConfig.Name)));
    fontConfig.Name[sizeof(fontConfig.Name) - 1] = '\0';

    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, static_cast<int>(dataSize), fontSize, &fontConfig);
}

bool Gui::processEvent(const SDL_Event* event) {
    ImGui::SetCurrentContext(imGuiContext_);
    ImGui_ImplSDL3_ProcessEvent(event);

    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        windowResized();
        return false;
    }

    const auto& io = ImGui::GetIO();
    return io.WantCaptureMouse || io.WantCaptureKeyboard;
}

void Gui::startFrame() {
    ImGui::SetCurrentContext(imGuiContext_);
    if (!ImGui::GetIO().Fonts->IsBuilt()) {
        rebuildAtlasTexture();
    }
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Gui::endFrame() {
    ImGui::Render();
    processImGuiCommands(ImGui::GetDrawData(), ImGui::GetIO());
}

void Gui::windowResized() {
    int iw = 0, ih = 0;
    SDL_GetWindowSizeInPixels(window_, &iw, &ih);
    uint32_t width = iw;
    uint32_t height = ih;

    view_->setViewport({ 0, 0, width, height });
    camera_->setProjection(filament::Camera::Projection::ORTHO, 0.0, width, height, 0.0, 0.0, 1.0);
}

void Gui::rebuildAtlasTexture() {
    using namespace filament;

    ImGui::SetCurrentContext(imGuiContext_);

    filament::Fence::waitAndDestroy(engine_->createFence());
    engine_->destroy(texture_);

    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    size_t size = width * height * 4;

    texture_ = Texture::Builder()
        .width(static_cast<uint32_t>(width))
        .height(static_cast<uint32_t>(height))
        .levels(1)
        .format(Texture::InternalFormat::RGBA8)
        .sampler(Texture::Sampler::SAMPLER_2D)
        .build(*engine_);

    void* pixelsCopy = malloc(size);
    std::memcpy(pixelsCopy, pixels, size);
    texture_->setImage(
        *engine_, 0,
        Texture::PixelBufferDescriptor(
            pixelsCopy, size, Texture::Format::RGBA, Texture::Type::UBYTE,
            [](void* buffer, size_t /*size*/, void* /*user*/) {
                free(buffer);
            }, nullptr
        )
    );

    if (customFont_) {
        sampler_ = TextureSampler(TextureSampler::MinFilter::LINEAR, TextureSampler::MagFilter::LINEAR);
        material_->setDefaultParameter("albedo", texture_, sampler_);
    } else {
        // For default font, switch to NEAREST for pixel-perfect text.
        sampler_ = TextureSampler(TextureSampler::MinFilter::NEAREST, TextureSampler::MagFilter::NEAREST);
        material_->setDefaultParameter("albedo", texture_, sampler_);
    }
}

void Gui::processImGuiCommands(ImDrawData* commands, const ImGuiIO& io) {
    using namespace filament;

    if (commands->Textures != nullptr) {
        for (ImTextureData* tex : *commands->Textures) {
            if (tex->Status == ImTextureStatus_OK) {
                continue;
            } else if (tex->Status == ImTextureStatus_WantCreate) {
                IM_ASSERT(tex->TexID == 0 && tex->BackendUserData == nullptr);
                IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);
                Texture* ftex = Texture::Builder()
                    .width(tex->Width)
                    .height(tex->Height)
                    .levels(1)
                    .format(Texture::InternalFormat::RGBA8)
                    .sampler(Texture::Sampler::SAMPLER_2D)
                    .build(*engine_);

                size_t size = tex->Width * tex->Height * 4;
                Texture::PixelBufferDescriptor pb(tex->GetPixels(), size, Texture::Format::RGBA, Texture::Type::UBYTE);
                ftex->setImage(*engine_, 0, std::move(pb));

                tex->SetTexID(reinterpret_cast<ImTextureID>(ftex));
                tex->SetStatus(ImTextureStatus_OK);
                imGuiTextures_.insert(ftex);
            } else if (tex->Status == ImTextureStatus_WantUpdates) {
                Texture* ftex = reinterpret_cast<Texture*>(tex->TexID);
                size_t size = tex->Width * tex->Height * 4;
                Texture::PixelBufferDescriptor pb(tex->GetPixels(), size, Texture::Format::RGBA, Texture::Type::UBYTE);
                ftex->setImage(*engine_, 0, std::move(pb));

                tex->SetStatus(ImTextureStatus_OK);
            } else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0) {
                Texture* ftex = reinterpret_cast<Texture*>(tex->TexID);
                engine_->destroy(ftex);
                tex->SetTexID(ImTextureID_Invalid);
                tex->SetStatus(ImTextureStatus_Destroyed);
                imGuiTextures_.erase(ftex);
            }
        }
    }

    // Avoid rendering when minimized and scale coordinates for retina displays.
    int fbwidth = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fbheight = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fbwidth == 0 || fbheight == 0) {
        return;
    }
    commands->ScaleClipRects(io.DisplayFramebufferScale);

    // Ensure that we have enough vertex buffers and index buffers.
    hasSynced_ = false;
    createBuffers(commands->CmdLists.size());

    // Count how many primitives we'll need, then create a Renderable builder.
    size_t nPrims = 0;
    std::unordered_map<uint64_t, MaterialInstance*> scissorRects;
    for (int cmdListIndex = 0; cmdListIndex < commands->CmdLists.size(); ++cmdListIndex) {
        const ImDrawList* cmds = commands->CmdLists[cmdListIndex];
        nPrims += cmds->CmdBuffer.size();
    }
    auto rbuilder = RenderableManager::Builder(nPrims);
    rbuilder.boundingBox({{ 0, 0, 0 }, { 10000, 10000, 10000 }}).culling(false);

    // Recreate the Renderable component and point it to the vertex buffers.
    auto& rcm = engine_->getRenderableManager();
    rcm.destroy(guiEntity_);
    size_t bufferIndex = 0;
    size_t primIndex = 0;
    size_t materialIndex = 0;
    for (int cmdListIndex = 0; cmdListIndex < commands->CmdLists.size(); ++cmdListIndex) {
        const ImDrawList* cmds = commands->CmdLists[cmdListIndex];
        populateVertexData(
            bufferIndex,
            cmds->VtxBuffer.Size * sizeof(ImDrawVert), cmds->VtxBuffer.Data,
            cmds->IdxBuffer.Size * sizeof(ImDrawIdx), cmds->IdxBuffer.Data
        );
        for (const auto& pcmd : cmds->CmdBuffer) {
            if (pcmd.UserCallback) {
                pcmd.UserCallback(cmds, &pcmd);
            } else {
                auto texture = reinterpret_cast<Texture const*>(pcmd.GetTexID());
                if (materialIndex == materialInstances_.size()) {
                    materialInstances_.push_back(material_->createInstance());
                }
                MaterialInstance* materialInstance = materialInstances_[materialIndex++];

                auto scissorLeft = static_cast<uint32_t>(std::max(0.0f, pcmd.ClipRect.x));
                auto scissorBottom = static_cast<uint32_t>(std::max(0.0f, fbheight - pcmd.ClipRect.w));
                auto scissorWidth = static_cast<uint32_t>(pcmd.ClipRect.z - pcmd.ClipRect.x);
                auto scissorHeight = static_cast<uint32_t>(pcmd.ClipRect.w - pcmd.ClipRect.y);
                materialInstance->setScissor(scissorLeft, scissorBottom, scissorWidth, scissorHeight);
                if (texture) {
                    TextureSampler sampler(TextureSampler::MinFilter::LINEAR, TextureSampler::MagFilter::LINEAR);
                    materialInstance->setParameter("albedo", texture, sampler);
                } else {
                    materialInstance->setParameter("albedo", texture_, sampler_);
                }
                rbuilder.geometry(
                        primIndex, RenderableManager::PrimitiveType::TRIANGLES,
                        vertexBuffers_[bufferIndex], indexBuffers_[bufferIndex],
                        pcmd.IdxOffset, pcmd.ElemCount
                    )
                    .blendOrder(primIndex, static_cast<uint16_t>(primIndex))
                    .material(primIndex, materialInstance);
                ++primIndex;
            }
        }
        ++bufferIndex;
    }
    if (!commands->CmdLists.empty()) {
        rbuilder.build(*engine_, guiEntity_);
    }
}

void Gui::syncThreads() {
    if (!hasSynced_) {
        // This is called only when ImGui needs to grow a vertex buffer, which occurs a few times
        // after launching and rarely (if ever) after that.
        filament::Fence::waitAndDestroy(engine_->createFence());
        hasSynced_ = true;
    }
}

void Gui::createVertexBuffer(size_t bufferIndex, uint32_t capacity) {
    using namespace filament;

    syncThreads();
    engine_->destroy(vertexBuffers_[bufferIndex]);
    vertexBuffers_[bufferIndex] = VertexBuffer::Builder()
        .vertexCount(capacity)
        .bufferCount(1)
        .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT2, 0, sizeof(ImDrawVert))
        .attribute(VertexAttribute::UV0, 0, VertexBuffer::AttributeType::FLOAT2, sizeof(math::float2), sizeof(ImDrawVert))
        .attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::UBYTE4, 2 * sizeof(math::float2), sizeof(ImDrawVert))
        .normalized(VertexAttribute::COLOR)
        .build(*engine_);
}

void Gui::createIndexBuffer(size_t bufferIndex, uint32_t capacity) {
    using namespace filament;

    syncThreads();
    engine_->destroy(indexBuffers_[bufferIndex]);
    indexBuffers_[bufferIndex] = IndexBuffer::Builder()
        .indexCount(capacity)
        .bufferType(IndexBuffer::IndexType::USHORT)
        .build(*engine_);
}

void Gui::createBuffers(size_t numRequiredBuffers) {
    if (numRequiredBuffers > vertexBuffers_.size()) {
        size_t previousSize = vertexBuffers_.size();
        vertexBuffers_.resize(numRequiredBuffers, nullptr);
        for (size_t i = previousSize; i < vertexBuffers_.size(); ++i) {
            // Pick a reasonable starting capacity.
            createVertexBuffer(i, 1000);
        }
    }
    if (numRequiredBuffers > indexBuffers_.size()) {
        size_t previousSize = indexBuffers_.size();
        indexBuffers_.resize(numRequiredBuffers, nullptr);
        for (size_t i = previousSize; i < indexBuffers_.size(); ++i) {
            // Pick a reasonable starting capacity.
            createIndexBuffer(i, 5000);
        }
    }
}

void Gui::populateVertexData(size_t bufferIndex, size_t vbSizeInBytes, void* vbImguiData, size_t ibSizeInBytes, void* ibImguiData) {
    // Create a new vertex buffer if the size isn't large enough, then copy the ImGui data into
    // a staging area since Filament's render thread might consume the data at any time.
    size_t requiredVertCount = vbSizeInBytes / sizeof(ImDrawVert);
    size_t capacityVertCount = vertexBuffers_[bufferIndex]->getVertexCount();
    if (requiredVertCount > capacityVertCount) {
        createVertexBuffer(bufferIndex, static_cast<uint32_t>(requiredVertCount));
    }
    size_t nVbBytes = requiredVertCount * sizeof(ImDrawVert);
    void* vbFilamentData = malloc(nVbBytes);
    std::memcpy(vbFilamentData, vbImguiData, nVbBytes);
    vertexBuffers_[bufferIndex]->setBufferAt(
        *engine_, 0,
        filament::VertexBuffer::BufferDescriptor(
            vbFilamentData, nVbBytes,
            [](void* buffer, size_t /*size*/, void* /*user*/) {
                free(buffer);
            }, nullptr
        )
    );

    // Create a new index buffer if the size isn't large enough, then copy the ImGui data into
    // a staging area since Filament's render thread might consume the data at any time.
    size_t requiredIndexCount = ibSizeInBytes / 2;
    size_t capacityIndexCount = indexBuffers_[bufferIndex]->getIndexCount();
    if (requiredIndexCount > capacityIndexCount) {
        createIndexBuffer(bufferIndex, static_cast<uint32_t>(requiredIndexCount));
    }
    size_t nIbBytes = requiredIndexCount * 2;
    void* ibFilamentData = malloc(nIbBytes);
    std::memcpy(ibFilamentData, ibImguiData, nIbBytes);
    indexBuffers_[bufferIndex]->setBuffer(
        *engine_,
        filament::IndexBuffer::BufferDescriptor(
            ibFilamentData, nIbBytes,
            [](void* buffer, size_t /*size*/, void* /*user*/) {
                free(buffer);
            }, nullptr
        )
    );
}

}
