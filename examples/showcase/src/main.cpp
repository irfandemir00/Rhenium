#include <rhenium/rhenium.hpp>

#include <windows.h>

#include <cstdio>
#include <string_view>

int main(int argc, char** argv) {
    bool check_embedded = false;
    bool check_assets = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--check-embedded") check_embedded = true;
        else if (arg == "--check-assets") check_assets = true;
    }

    if (check_embedded) {
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
        }
        const size_t count = rhenium::EmbeddedAssetRegistry::instance().count();
        std::printf("Embedded assets: %zu\n", count);
        return count >= 8 ? 0 : 1;
    }

    auto app = rhenium::App::from_config_file("rhenium.json");

    if (check_assets) {
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
        }
        const auto html = app.resolver().resolve("/index.html");
        const auto css = app.resolver().resolve("/styles/tokens.css");
        const auto js = app.resolver().resolve("/js/main.js");
        const bool ok = app.config().frameless && html.has_value() && css.has_value() && js.has_value();
        std::printf("Check assets: frameless=%d html=%d css=%d js=%d\n",
                    app.config().frameless ? 1 : 0,
                    html.has_value() ? 1 : 0,
                    css.has_value() ? 1 : 0,
                    js.has_value() ? 1 : 0);
        return ok ? 0 : 1;
    }

    return app.run();
}
