#include "gview/authoring.hpp"

namespace gview {

// Preserves the broad simulator coverage used by Gubsy's mature video tool.
const std::vector<PreviewPreset>& preview_presets() {
    using F = glayout::FormFactor;
    static const std::vector<PreviewPreset> presets{
        {"16:9 Desktop / TV", "1280x720 (720p)", 1280, 720, 1.0f, F::Desktop},
        {"16:9 Desktop / TV", "1366x768 (16:9)", 1366, 768, 1.0f, F::Desktop},
        {"16:9 Desktop / TV", "1920x1080 (1080p)", 1920, 1080, 1.0f, F::Desktop},
        {"16:9 Desktop / TV", "2560x1440 (1440p)", 2560, 1440, 1.0f, F::Desktop},
        {"16:9 Desktop / TV", "3840x2160 (4K)", 3840, 2160, 2.0f, F::Desktop},
        {"16:9 Desktop / TV", "7680x4320 (8K)", 7680, 4320, 4.0f, F::Desktop},
        {"16:10 Laptops", "1280x800 (16:10)", 1280, 800, 1.0f, F::Tablet},
        {"16:10 Laptops", "1440x900 (16:10)", 1440, 900, 1.0f, F::Desktop},
        {"16:10 Laptops", "1680x1050 (16:10)", 1680, 1050, 1.0f, F::Desktop},
        {"16:10 Laptops", "1920x1200 (WUXGA)", 1920, 1200, 1.0f, F::Desktop},
        {"16:10 Laptops", "2560x1600 (WQXGA)", 2560, 1600, 2.0f, F::Desktop},
        {"16:10 Laptops", "2880x1800 (Retina)", 2880, 1800, 2.0f, F::Desktop},
        {"Ultrawide 21:9", "2560x1080 (21:9)", 2560, 1080, 1.0f, F::Desktop},
        {"Ultrawide 21:9", "3440x1440 (UWQHD)", 3440, 1440, 1.0f, F::Desktop},
        {"Ultrawide 21:9", "5120x2160 (5K2K)", 5120, 2160, 2.0f, F::Desktop},
        {"Super Ultrawide 32:9", "3840x1080 (32:9)", 3840, 1080, 1.0f, F::Desktop},
        {"Super Ultrawide 32:9", "5120x1440 (DQHD)", 5120, 1440, 1.0f, F::Desktop},
        {"Console & Retro", "1280x720 (Switch HD)", 1280, 720, 1.0f, F::Tablet},
        {"Console & Retro", "640x480 (SD Consoles)", 640, 480, 1.0f, F::Desktop},
        {"Console & Retro", "480x272 (PSP)", 480, 272, 1.0f, F::Tablet},
        {"Console & Retro", "480x270 (PSP Crop)", 480, 270, 1.0f, F::Tablet},
        {"Console & Retro", "320x240 (N64/PS1)", 320, 240, 1.0f, F::Tablet},
        {"Console & Retro", "256x192 (Nintendo DS)", 256, 192, 1.0f, F::Tablet},
        {"Console & Retro", "240x160 (GBA)", 240, 160, 1.0f, F::Tablet},
        {"Console & Retro", "160x144 (Game Boy)", 160, 144, 1.0f, F::Phone},
        {"Phone Portrait", "720x1280 (HD Phone)", 720, 1280, 2.0f, F::Phone},
        {"Phone Portrait", "1080x1920 (FHD Phone)", 1080, 1920, 3.0f, F::Phone},
        {"Phone Portrait", "1080x2340 (19.5:9)", 1080, 2340, 3.0f, F::Phone},
        {"Phone Portrait", "1080x2400 (20:9)", 1080, 2400, 3.0f, F::Phone},
        {"Phone Portrait", "1170x2532 (iPhone 12/13)", 1170, 2532, 3.0f, F::Phone,
         {0.0f, 47.0f, 0.0f, 34.0f}},
        {"Phone Portrait", "1179x2556 (iPhone 14/15)", 1179, 2556, 3.0f, F::Phone,
         {0.0f, 59.0f, 0.0f, 34.0f}},
        {"Phone Portrait", "1440x3200 (QHD+ Phone)", 1440, 3200, 4.0f, F::Phone},
        {"Tablet Portrait", "1536x2048 (iPad)", 1536, 2048, 2.0f, F::Tablet},
        {"Tablet Portrait", "1668x2388 (iPad Pro 11\")", 1668, 2388, 2.0f, F::Tablet},
        {"Tablet Portrait", "2048x2732 (iPad Pro 12.9\")", 2048, 2732, 2.0f, F::Tablet},
        {"Tablet Portrait", "1600x2560 (Android Tablet)", 1600, 2560, 2.0f, F::Tablet},
    };
    return presets;
}

} // namespace gview
