#include <filament_app/Gui.h>

#include <backends/imgui_impl_sdl3.h>    // FIXME: in a change to ImGui.cmake I removed the backends, may need to add this back for SDL.
#include <filament/Engine.h>
#include <filament/Material.h>
#include <filament/Scene.h>
#include <filament/Texture.h>
#include <filament/View.h>
#include <imgui.h>
#include <utils/EntityManager.h>

namespace filament_app {

Gui::Gui(filament::Engine* engine) :
    engine_(engine) {

    using namespace filament;

    IMGUI_CHECKVERSION();
    imGuiContext_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imGuiContext_);

    ImGui::StyleColorsDark();

    const void* FILAGUI_RESOURCES_UIBLIT_DATA = nullptr;
    size_t FILAGUI_RESOURCES_UIBLIT_SIZE = 0;

    material_ = Material::Builder()
        .package(FILAGUI_RESOURCES_UIBLIT_DATA, FILAGUI_RESOURCES_UIBLIT_SIZE)
        .build(*engine_);

    // Use the font file if provided, otherwise fall back to the default font.
    //if (fontPath.isFile()) { // FIXME
    //    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
    //}
    createAtlasTexture();

    // For default font, switch to NEAREST for pixel-perfect text.
    //if (!fontPath.isFile()) {
        sampler_ = TextureSampler(TextureSampler::MinFilter::NEAREST, TextureSampler::MagFilter::NEAREST);
        material_->setDefaultParameter("albedo", texture_, sampler_);
    //}

    auto& em = utils::EntityManager::get();

    view_ = engine_->createView();
    view_->setPostProcessingEnabled(false);
    view_->setBlendMode(View::BlendMode::TRANSLUCENT);
    view_->setShadowingEnabled(false);

    scene_ = engine->createScene();

    cameraEntity_ = em.create();
    camera_ = engine_->createCamera(cameraEntity_);
    view_->setCamera(camera_);

    view_->setScene(scene_);
    guiEntity_ = em.create();
    scene_->addEntity(guiEntity_);
}

Gui::~Gui() {
    engine_->destroy(guiEntity_);
    engine_->destroyCameraComponent(cameraEntity_);
    engine_->destroy(scene_);
    engine_->destroy(view_);

    auto& em = utils::EntityManager::get();
    em.destroy(guiEntity_);
    em.destroy(cameraEntity_);

    engine_->destroy(material_);
    engine_->destroy(texture_);

    ImGui::DestroyContext(imGuiContext_);
}

void Gui::createAtlasTexture() {
    using namespace filament;

    ImGui::SetCurrentContext(imGuiContext_);

    engine_->destroy(texture_);
    unsigned char* pixels;
    int width, height;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);    // FIXME: do we need a copy of the pixels? filament-glfw-imgui does this but it doesn't seem necessary.
    size_t size = width * height * 4;

    texture_ = Texture::Builder()
        .width(static_cast<uint32_t>(width))
        .height(static_cast<uint32_t>(height))
        .levels(1)
        .format(Texture::InternalFormat::RGBA8)
        .sampler(Texture::Sampler::SAMPLER_2D)
        .build(*engine_);

    Texture::PixelBufferDescriptor pb(pixels, size, Texture::Format::RGBA, Texture::Type::UBYTE);
    texture_->setImage(*engine_, 0, std::move(pb));

    sampler_ = TextureSampler(TextureSampler::MinFilter::LINEAR, TextureSampler::MagFilter::LINEAR);
    material_->setDefaultParameter("albedo", texture_, sampler_);
}

void Gui::processEvent(const SDL_Event* event) {
    ImGui::SetCurrentContext(imGuiContext_);
    ImGui_ImplSDL3_ProcessEvent(event);  // FIXME: consume event?
}

void Gui::startFrame() {
    ImGui::SetCurrentContext(imGuiContext_);
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Gui::render() {
    ImGui::Render();
    processImGuiCommands(ImGui::GetDrawData(), ImGui::GetIO());
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

    // FIXME: much more to do here...
    // basing this on filament's filamentapp and filagui code (the authoritative version) with the older filament-glfw-imgui code.
}

}
