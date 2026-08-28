#include "gview/sdl3_renderer.hpp"

#include "sdl3_font.hpp"

#include <algorithm>

namespace gview {

struct Sdl3Renderer::FontSystem {
    explicit FontSystem(SDL_Renderer* renderer, const std::string& path) : atlas(renderer, path) {}
    sdl3_detail::FontAtlas atlas;
};

namespace {

SDL_Color color(Color source, float opacity = 1.0f) {
    return SDL_Color{source.r, source.g, source.b,
                     static_cast<std::uint8_t>(static_cast<float>(source.a) * opacity)};
}

SDL_FRect rect(glayout::Rect source) {
    return SDL_FRect{source.x, source.y, source.w, source.h};
}

void set_color(SDL_Renderer* renderer, SDL_Color value) {
    SDL_SetRenderDrawColor(renderer, value.r, value.g, value.b, value.a);
}

} // namespace

// Adapts renderer-neutral commands to SDL while retaining text and image resources.
Sdl3Renderer::Sdl3Renderer(SDL_Renderer* renderer, const std::string& font_path)
    : renderer_(renderer), fonts_(std::make_unique<FontSystem>(renderer, font_path)) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
}

Sdl3Renderer::~Sdl3Renderer() = default;
bool Sdl3Renderer::ready() const { return renderer_ && fonts_ && fonts_->atlas.ready(); }

void Sdl3Renderer::register_texture(std::string id, SDL_Texture* texture) {
    textures_[std::move(id)] = texture;
}

void Sdl3Renderer::unregister_texture(std::string_view id) {
    textures_.erase(std::string(id));
}

// Draws stable paint commands in their compiled stratum order.
void Sdl3Renderer::render(const std::vector<PaintCommand>& commands) {
    for (const PaintCommand& command : commands) {
        const SDL_Rect clip{static_cast<int>(command.clip.x), static_cast<int>(command.clip.y),
                            static_cast<int>(command.clip.w), static_cast<int>(command.clip.h)};
        SDL_SetRenderClipRect(renderer_, &clip);
        const SDL_FRect target = rect(command.rect);
        if (command.kind == PaintKind::Box) {
            set_color(renderer_, color(command.box.fill, command.box.opacity));
            SDL_RenderFillRect(renderer_, &target);
            if (command.box.border_width > 0.0f) {
                set_color(renderer_, color(command.box.border, command.box.opacity));
                SDL_RenderRect(renderer_, &target);
            }
            continue;
        }
        if (command.kind == PaintKind::Image || command.kind == PaintKind::Sprite) {
            const auto texture = textures_.find(command.asset);
            if (texture != textures_.end()) SDL_RenderTexture(renderer_, texture->second, nullptr, &target);
            continue;
        }
        if (command.kind == PaintKind::Progress) {
            SDL_FRect progress = target;
            progress.w *= static_cast<float>(std::clamp(command.value, 0.0, 1.0));
            set_color(renderer_, color(command.box.text, command.box.opacity));
            SDL_RenderFillRect(renderer_, &progress);
            continue;
        }
        if (command.kind != PaintKind::Text || command.text.empty()) continue;
        const sdl3_detail::TextLayout layout = fonts_->atlas.layout(
            command.text, command.text_style.size, target.w, command.text_style.wrap);
        float origin_x = target.x;
        if (command.text_style.horizontal == TextAlign::Center) origin_x += (target.w - layout.width) * 0.5f;
        else if (command.text_style.horizontal == TextAlign::End) origin_x += target.w - layout.width;
        float origin_y = target.y;
        if (command.text_style.vertical == TextAlign::Center) origin_y += (target.h - layout.height) * 0.5f;
        else if (command.text_style.vertical == TextAlign::End) origin_y += target.h - layout.height;
        const SDL_Color tint = color(command.box.text, command.box.opacity);
        SDL_SetTextureColorMod(fonts_->atlas.texture(), tint.r, tint.g, tint.b);
        SDL_SetTextureAlphaMod(fonts_->atlas.texture(), tint.a);
        for (const sdl3_detail::PositionedGlyph& glyph : layout.glyphs) {
            const SDL_FRect glyph_target{origin_x + glyph.x, origin_y + glyph.y, glyph.width, glyph.height};
            SDL_RenderTexture(renderer_, fonts_->atlas.texture(), &glyph.source, &glyph_target);
        }
    }
    SDL_SetRenderClipRect(renderer_, nullptr);
}

} // namespace gview
