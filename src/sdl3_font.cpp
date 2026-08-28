#include "sdl3_font.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace gview::sdl3_detail {
namespace {

constexpr int atlas_size = 2048;

struct GlyphKey {
    std::uint32_t glyph = 0;
    int size = 0;
    bool operator==(const GlyphKey&) const = default;
};

struct GlyphHash {
    std::size_t operator()(const GlyphKey& key) const {
        return static_cast<std::size_t>(key.glyph) * 1315423911u ^ static_cast<std::size_t>(key.size);
    }
};

struct Glyph {
    SDL_FRect source;
    float bearing_x = 0.0f;
    float bearing_y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

} // namespace

struct FontAtlas::Impl {
    SDL_Renderer* renderer = nullptr;
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    hb_font_t* font = nullptr;
    SDL_Texture* texture = nullptr;
    std::vector<std::uint8_t> pixels;
    std::unordered_map<GlyphKey, Glyph, GlyphHash> glyphs;
    int cursor_x = 1;
    int cursor_y = 1;
    int row_height = 0;
    bool dirty = false;

    Glyph glyph(std::uint32_t index, int pixel_size) {
        const GlyphKey key{index, pixel_size};
        const auto cached = glyphs.find(key);
        if (cached != glyphs.end()) return cached->second;
        FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_size));
        if (FT_Load_Glyph(face, index, FT_LOAD_DEFAULT) != 0 ||
            FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0)
            return {};
        const FT_Bitmap& bitmap = face->glyph->bitmap;
        const int width = static_cast<int>(bitmap.width);
        const int height = static_cast<int>(bitmap.rows);
        if (cursor_x + width + 1 >= atlas_size) {
            cursor_x = 1;
            cursor_y += row_height + 1;
            row_height = 0;
        }
        if (cursor_y + height + 1 >= atlas_size) return {};
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t target = static_cast<std::size_t>(((cursor_y + y) * atlas_size +
                                                                     cursor_x + x) * 4);
                const std::size_t source = static_cast<std::size_t>(y * bitmap.pitch + x);
                pixels[target] = 255;
                pixels[target + 1] = 255;
                pixels[target + 2] = 255;
                pixels[target + 3] = bitmap.buffer[source];
            }
        }
        Glyph result{{static_cast<float>(cursor_x), static_cast<float>(cursor_y),
                      static_cast<float>(width), static_cast<float>(height)},
                     static_cast<float>(face->glyph->bitmap_left),
                     static_cast<float>(face->glyph->bitmap_top), static_cast<float>(width),
                     static_cast<float>(height)};
        cursor_x += width + 1;
        row_height = std::max(row_height, height);
        dirty = true;
        glyphs.emplace(key, result);
        return result;
    }

    void upload() {
        if (!dirty || !texture) return;
        SDL_UpdateTexture(texture, nullptr, pixels.data(), atlas_size * 4);
        dirty = false;
    }
};

// Owns one scalable glyph atlas shared by every text command.
FontAtlas::FontAtlas(SDL_Renderer* renderer, const std::string& path) : impl_(std::make_unique<Impl>()) {
    impl_->renderer = renderer;
    if (FT_Init_FreeType(&impl_->library) != 0 || FT_New_Face(impl_->library, path.c_str(), 0, &impl_->face) != 0)
        return;
    impl_->font = hb_ft_font_create_referenced(impl_->face);
    impl_->pixels.assign(static_cast<std::size_t>(atlas_size * atlas_size * 4), 0);
    impl_->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, atlas_size, atlas_size);
    if (impl_->texture) SDL_SetTextureBlendMode(impl_->texture, SDL_BLENDMODE_BLEND);
}

FontAtlas::~FontAtlas() {
    if (impl_->texture) SDL_DestroyTexture(impl_->texture);
    if (impl_->font) hb_font_destroy(impl_->font);
    if (impl_->face) FT_Done_Face(impl_->face);
    if (impl_->library) FT_Done_FreeType(impl_->library);
}

bool FontAtlas::ready() const { return impl_->texture && impl_->font; }

// Shapes Unicode with HarfBuzz and packs requested FreeType glyphs on demand.
TextLayout FontAtlas::layout(const std::string& text, float pixel_size, float maximum_width,
                             bool wrap) {
    TextLayout result;
    if (!ready() || text.empty()) return result;
    const int size = std::max(1, static_cast<int>(std::lround(pixel_size)));
    FT_Set_Pixel_Sizes(impl_->face, 0, static_cast<FT_UInt>(size));
    hb_ft_font_changed(impl_->font);
    const float line_height = static_cast<float>(impl_->face->size->metrics.height) / 64.0f;
    result.ascender = static_cast<float>(impl_->face->size->metrics.ascender) / 64.0f;
    float x = 0.0f;
    float baseline = result.ascender;
    std::size_t line_start = 0;
    while (line_start <= text.size()) {
        const std::size_t newline = text.find('\n', line_start);
        const std::size_t line_end = newline == std::string::npos ? text.size() : newline;
        hb_buffer_t* buffer = hb_buffer_create();
        hb_buffer_add_utf8(buffer, text.data() + line_start,
                           static_cast<int>(line_end - line_start), 0,
                           static_cast<int>(line_end - line_start));
        hb_buffer_guess_segment_properties(buffer);
        hb_shape(impl_->font, buffer, nullptr, 0);
        unsigned count = 0;
        const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, &count);
        const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, &count);
        for (unsigned index = 0; index < count; ++index) {
            const float advance = static_cast<float>(positions[index].x_advance) / 64.0f;
            if (wrap && maximum_width > 0.0f && x > 0.0f && x + advance > maximum_width) {
                result.width = std::max(result.width, x);
                x = 0.0f;
                baseline += line_height;
            }
            const Glyph glyph = impl_->glyph(info[index].codepoint, size);
            result.glyphs.push_back(PositionedGlyph{
                glyph.source,
                x + static_cast<float>(positions[index].x_offset) / 64.0f + glyph.bearing_x,
                baseline - static_cast<float>(positions[index].y_offset) / 64.0f - glyph.bearing_y,
                glyph.width, glyph.height});
            x += advance;
        }
        hb_buffer_destroy(buffer);
        result.width = std::max(result.width, x);
        if (newline == std::string::npos) break;
        x = 0.0f;
        baseline += line_height;
        line_start = newline + 1;
    }
    result.height = baseline - result.ascender + line_height;
    impl_->upload();
    return result;
}

SDL_Texture* FontAtlas::texture() const { return impl_->texture; }

} // namespace gview::sdl3_detail
