#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace gview {

using NodeIndex = std::uint32_t;
constexpr NodeIndex invalid_node = 0xffffffffu;

struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

enum class ContentKind { None, Text, Image, Sprite, Progress, CustomSurface };
enum class ControlKind { None, Button, Toggle, Slider, Select, TextInput, ScrollArea };
enum class ActivationPolicy { Manual, OnFocus };
enum class PaintStratum { Background, Content, Overlay, Modal, Tooltip, Prompt };
enum class TextAlign { Start, Center, End };
enum class NavAction { Up, Down, Left, Right, Confirm, Back, TabPrevious, TabNext };

using Value = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

struct TextStyle {
    std::string font = "default";
    float size = 16.0f;
    float line_height = 0.0f;
    TextAlign horizontal = TextAlign::Start;
    TextAlign vertical = TextAlign::Start;
    bool wrap = false;
};

struct BoxStyle {
    Color fill{0, 0, 0, 0};
    Color border{0, 0, 0, 0};
    Color text{255, 255, 255, 255};
    float border_width = 0.0f;
    float corner_radius = 0.0f;
    float opacity = 1.0f;
};

struct VisualStates {
    BoxStyle normal;
    BoxStyle selected;
    BoxStyle hovered;
    BoxStyle focused;
    BoxStyle selected_focused;
    BoxStyle pressed;
    BoxStyle disabled;
};

struct SelectOption {
    std::string id;
    std::string label;
    Value value;
};

struct NodeSpec {
    std::string layout_id;
    ContentKind content = ContentKind::None;
    ControlKind control = ControlKind::None;
    ActivationPolicy activation = ActivationPolicy::Manual;
    PaintStratum stratum = PaintStratum::Content;
    VisualStates style;
    TextStyle text_style;
    std::string text;
    std::string asset;
    std::string binding;
    std::string action;
    std::string focus_group;
    std::string condition;
    std::vector<SelectOption> options;
    double minimum = 0.0;
    double maximum = 1.0;
    double step = 0.1;
    bool selected = false;
    bool focusable = false;
    bool enabled = true;
};

std::string_view to_string(ContentKind value);
std::string_view to_string(ControlKind value);
std::string_view to_string(NavAction value);

} // namespace gview
