#pragma once

#include "gview/paint.hpp"

#include <SDL3/SDL.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gview {

class Sdl3Renderer {
  public:
    using SurfaceRenderer = std::function<void(SDL_Renderer*, const PaintCommand&)>;
    Sdl3Renderer(SDL_Renderer* renderer, const std::string& font_path);
    ~Sdl3Renderer();
    Sdl3Renderer(const Sdl3Renderer&) = delete;
    Sdl3Renderer& operator=(const Sdl3Renderer&) = delete;

    bool ready() const;
    void register_texture(std::string id, SDL_Texture* texture);
    void unregister_texture(std::string_view id);
    void register_surface(std::string id, SurfaceRenderer renderer);
    void unregister_surface(std::string_view id);
    void set_device_pixel_ratio(float scale);
    void set_nearest_sampling(bool nearest);
    void render(const std::vector<PaintCommand>& commands);

  private:
    struct FontSystem;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<FontSystem> fonts_;
    std::unordered_map<std::string, SDL_Texture*> textures_;
    std::unordered_map<std::string, SurfaceRenderer> surfaces_;
    float device_pixel_ratio_ = 1.0f;
    bool nearest_sampling_ = false;
};

} // namespace gview
