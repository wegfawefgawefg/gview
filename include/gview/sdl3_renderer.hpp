#pragma once

#include "gview/paint.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gview {

class Sdl3Renderer {
  public:
    Sdl3Renderer(SDL_Renderer* renderer, const std::string& font_path);
    ~Sdl3Renderer();
    Sdl3Renderer(const Sdl3Renderer&) = delete;
    Sdl3Renderer& operator=(const Sdl3Renderer&) = delete;

    bool ready() const;
    void register_texture(std::string id, SDL_Texture* texture);
    void unregister_texture(std::string_view id);
    void render(const std::vector<PaintCommand>& commands);

  private:
    struct FontSystem;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<FontSystem> fonts_;
    std::unordered_map<std::string, SDL_Texture*> textures_;
};

} // namespace gview
