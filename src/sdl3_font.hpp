#pragma once

#include "gview/types.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

namespace gview::sdl3_detail {

struct PositionedGlyph {
    SDL_FRect source;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct TextLayout {
    std::vector<PositionedGlyph> glyphs;
    float width = 0.0f;
    float height = 0.0f;
    float ascender = 0.0f;
};

class FontAtlas {
  public:
    FontAtlas(SDL_Renderer* renderer, const std::string& path);
    ~FontAtlas();
    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    bool ready() const;
    TextLayout layout(const std::string& text, float pixel_size, float maximum_width, bool wrap);
    SDL_Texture* texture() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gview::sdl3_detail
