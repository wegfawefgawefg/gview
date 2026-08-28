#include "gview/sdl3_renderer.hpp"

#include "sdl3_font.hpp"

#include <algorithm>
#include <cmath>

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

SDL_FRect rect(glayout::Rect source) { return SDL_FRect{source.x, source.y, source.w, source.h}; }

SDL_Rect clip_rect(glayout::Rect source) {
    const int left = static_cast<int>(std::floor(source.x));
    const int top = static_cast<int>(std::floor(source.y));
    const int right = static_cast<int>(std::ceil(source.x + source.w));
    const int bottom = static_cast<int>(std::ceil(source.y + source.h));
    return {left, top, std::max(0, right - left), std::max(0, bottom - top)};
}

void set_color(SDL_Renderer* renderer, SDL_Color value) {
    SDL_SetRenderDrawColor(renderer, value.r, value.g, value.b, value.a);
}

void render_contained(SDL_Renderer* renderer, SDL_Texture* texture, SDL_FRect target,
                      bool cover) {
    float width = 0.0f;
    float height = 0.0f;
    if (!SDL_GetTextureSize(texture, &width, &height) || width <= 0.0f || height <= 0.0f)
        return;
    const float scale = cover ? std::max(target.w / width, target.h / height)
                              : std::min(target.w / width, target.h / height);
    if (!cover) {
        SDL_FRect destination{target.x + (target.w - width * scale) * 0.5f,
                              target.y + (target.h - height * scale) * 0.5f,
                              width * scale, height * scale};
        SDL_RenderTexture(renderer, texture, nullptr, &destination);
        return;
    }
    const float source_width = target.w / scale;
    const float source_height = target.h / scale;
    SDL_FRect source{(width - source_width) * 0.5f, (height - source_height) * 0.5f,
                     source_width, source_height};
    SDL_RenderTexture(renderer, texture, &source, &target);
}

void render_natural(SDL_Renderer* renderer, SDL_Texture* texture, SDL_FRect target) {
    float width = 0.0f;
    float height = 0.0f;
    if (!SDL_GetTextureSize(texture, &width, &height)) return;
    SDL_FRect destination{target.x + (target.w - width) * 0.5f,
                          target.y + (target.h - height) * 0.5f, width, height};
    SDL_RenderTexture(renderer, texture, nullptr, &destination);
}

void render_tiled(SDL_Renderer* renderer, SDL_Texture* texture, SDL_FRect target) {
    float width = 0.0f;
    float height = 0.0f;
    if (!SDL_GetTextureSize(texture, &width, &height) || width <= 0.0f || height <= 0.0f)
        return;
    for (float y = target.y; y < target.y + target.h; y += height) {
        for (float x = target.x; x < target.x + target.w; x += width) {
            const float draw_width = std::min(width, target.x + target.w - x);
            const float draw_height = std::min(height, target.y + target.h - y);
            const SDL_FRect source{0.0f, 0.0f, draw_width, draw_height};
            const SDL_FRect destination{x, y, draw_width, draw_height};
            SDL_RenderTexture(renderer, texture, &source, &destination);
        }
    }
}

void render_nine_slice(SDL_Renderer* renderer, SDL_Texture* texture, SDL_FRect target,
                       float requested_slice) {
    float width = 0.0f;
    float height = 0.0f;
    if (!SDL_GetTextureSize(texture, &width, &height) || width <= 0.0f || height <= 0.0f)
        return;
    const float source_slice = std::clamp(requested_slice, 0.0f, std::min(width, height) * 0.5f);
    const float target_slice = std::min(source_slice, std::min(target.w, target.h) * 0.5f);
    const float source_x[4]{0.0f, source_slice, width - source_slice, width};
    const float source_y[4]{0.0f, source_slice, height - source_slice, height};
    const float target_x[4]{target.x, target.x + target_slice,
                            target.x + target.w - target_slice, target.x + target.w};
    const float target_y[4]{target.y, target.y + target_slice,
                            target.y + target.h - target_slice, target.y + target.h};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const SDL_FRect source{source_x[column], source_y[row],
                                   source_x[column + 1] - source_x[column],
                                   source_y[row + 1] - source_y[row]};
            const SDL_FRect destination{target_x[column], target_y[row],
                                        target_x[column + 1] - target_x[column],
                                        target_y[row + 1] - target_y[row]};
            SDL_RenderTexture(renderer, texture, &source, &destination);
        }
    }
}

void render_image(SDL_Renderer* renderer, SDL_Texture* texture, const PaintCommand& command,
                  SDL_FRect target) {
    SDL_SetTextureColorMod(texture, command.tint.r, command.tint.g, command.tint.b);
    SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(
                                    static_cast<float>(command.tint.a) * command.image_opacity));
    switch (command.image_mode) {
    case ImageMode::Natural: render_natural(renderer, texture, target); break;
    case ImageMode::Contain: render_contained(renderer, texture, target, false); break;
    case ImageMode::Cover: render_contained(renderer, texture, target, true); break;
    case ImageMode::Tile: render_tiled(renderer, texture, target); break;
    case ImageMode::NineSlice:
        render_nine_slice(renderer, texture, target, command.slice);
        break;
    case ImageMode::Stretch: SDL_RenderTexture(renderer, texture, nullptr, &target); break;
    }
}

} // namespace

// Adapts renderer-neutral commands to SDL while retaining text and image
// resources.
Sdl3Renderer::Sdl3Renderer(SDL_Renderer* renderer, const std::string& font_path)
    : renderer_(renderer), fonts_(std::make_unique<FontSystem>(renderer, font_path)) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
}

Sdl3Renderer::~Sdl3Renderer() = default;
bool Sdl3Renderer::ready() const { return renderer_ && fonts_ && fonts_->atlas.ready(); }

void Sdl3Renderer::register_texture(std::string id, SDL_Texture* texture) {
    SDL_SetTextureScaleMode(texture,
                            nearest_sampling_ ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
    textures_[std::move(id)] = texture;
}

void Sdl3Renderer::unregister_texture(std::string_view id) { textures_.erase(std::string(id)); }

void Sdl3Renderer::register_surface(std::string id, SurfaceRenderer renderer) {
    surfaces_[std::move(id)] = std::move(renderer);
}

void Sdl3Renderer::unregister_surface(std::string_view id) { surfaces_.erase(std::string(id)); }

void Sdl3Renderer::set_device_pixel_ratio(float scale) {
    scale = std::clamp(scale, 0.5f, 4.0f);
    if (std::fabs(scale - device_pixel_ratio_) < 0.001f) return;
    device_pixel_ratio_ = scale;
    fonts_->atlas.clear();
}

void Sdl3Renderer::set_nearest_sampling(bool nearest) {
    nearest_sampling_ = nearest;
    const SDL_ScaleMode mode = nearest ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR;
    SDL_SetTextureScaleMode(fonts_->atlas.texture(), mode);
    for (const auto& [id, texture] : textures_) {
        (void)id;
        SDL_SetTextureScaleMode(texture, mode);
    }
}

// Draws stable paint commands in their compiled stratum order.
void Sdl3Renderer::render(const std::vector<PaintCommand>& commands) {
    for (const PaintCommand& command : commands) {
        const SDL_Rect clip = clip_rect(command.clip);
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
            if (texture != textures_.end())
                render_image(renderer_, texture->second, command, target);
            continue;
        }
        if (command.kind == PaintKind::Progress) {
            SDL_FRect progress = target;
            progress.w *= static_cast<float>(std::clamp(command.value, 0.0, 1.0));
            set_color(renderer_, color(command.box.text, command.box.opacity));
            SDL_RenderFillRect(renderer_, &progress);
            continue;
        }
        if (command.kind == PaintKind::CustomSurface) {
            const auto surface = surfaces_.find(command.asset);
            if (surface != surfaces_.end()) surface->second(renderer_, command);
            continue;
        }
        if (command.kind != PaintKind::Text || command.text.empty()) continue;
        const sdl3_detail::TextLayout layout = fonts_->atlas.layout(
            command.text, command.text_style.size * device_pixel_ratio_,
            command.text_style.line_height * device_pixel_ratio_,
            target.w * device_pixel_ratio_, command.text_style.wrap);
        const float layout_width = layout.width / device_pixel_ratio_;
        const float layout_height = layout.height / device_pixel_ratio_;
        float origin_x = target.x;
        if (command.text_style.horizontal == TextAlign::Center)
            origin_x += (target.w - layout_width) * 0.5f;
        else if (command.text_style.horizontal == TextAlign::End)
            origin_x += target.w - layout_width;
        float origin_y = target.y;
        if (command.text_style.vertical == TextAlign::Center)
            origin_y += (target.h - layout_height) * 0.5f;
        else if (command.text_style.vertical == TextAlign::End)
            origin_y += target.h - layout_height;
        const SDL_Color tint = color(command.box.text, command.box.opacity);
        SDL_SetTextureColorMod(fonts_->atlas.texture(), tint.r, tint.g, tint.b);
        SDL_SetTextureAlphaMod(fonts_->atlas.texture(), tint.a);
        for (const sdl3_detail::PositionedGlyph& glyph : layout.glyphs) {
            const SDL_FRect glyph_target{origin_x + glyph.x / device_pixel_ratio_,
                                         origin_y + glyph.y / device_pixel_ratio_,
                                         glyph.width / device_pixel_ratio_,
                                         glyph.height / device_pixel_ratio_};
            SDL_RenderTexture(renderer_, fonts_->atlas.texture(), &glyph.source, &glyph_target);
        }
    }
    SDL_SetRenderClipRect(renderer_, nullptr);
}

} // namespace gview
