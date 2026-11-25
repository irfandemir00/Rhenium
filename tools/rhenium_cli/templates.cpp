#include "templates.hpp"

#include "process.hpp"

#include <cctype>
#include <nlohmann/json.hpp>

namespace rhenium::cli {

std::string sanitize_identifier(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (c == '-' || c == ' ') {
            out.push_back('_');
        }
    }
    if (out.empty() || std::isdigit(static_cast<unsigned char>(out.front()))) {
        out = "app_" + out;
    }
    return out;
}

bool scaffold_project(
    const std::string& raw_name,
    const std::filesystem::path& project_dir,
    const std::string& default_rhenium_dir) {
    const std::string target_name = sanitize_identifier(raw_name);

    const std::string cmake_content =
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(" + target_name + " LANGUAGES CXX)\n\n"
        "set(CMAKE_CXX_STANDARD 20)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        "set(CMAKE_CXX_EXTENSIONS OFF)\n\n"
        "set(RHENIUM_DIR \"$ENV{RHENIUM_DIR}\" CACHE PATH \"Path to Rhenium repository\")\n"
        "if(NOT RHENIUM_DIR)\n"
        "    set(RHENIUM_DIR \"" + default_rhenium_dir + "\")\n"
        "endif()\n\n"
        "set(RHENIUM_BUILD_EXAMPLES OFF CACHE BOOL \"\" FORCE)\n"
        "set(RHENIUM_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)\n"
        "set(RHENIUM_BUILD_CLI OFF CACHE BOOL \"\" FORCE)\n\n"
        "add_subdirectory(\"${RHENIUM_DIR}\" \"${CMAKE_BINARY_DIR}/_rhenium\")\n\n"
        "add_executable(" + target_name + " WIN32\n"
        "    src/main.cpp\n"
        ")\n\n"
        "target_link_libraries(" + target_name + " PRIVATE rhenium::rhenium)\n"
        "rhenium_embed_assets(TARGET " + target_name + " DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/ui\")\n"
        "rhenium_copy_webview2_loader(" + target_name + ")\n";

    const nlohmann::json config_json = {
        {"app_id", "com.rhenium." + target_name},
        {"title", raw_name},
        {"width", 900},
        {"height", 620},
        {"min_width", 520},
        {"min_height", 380},
        {"resizable", true},
        {"frameless", false},
        {"dark_titlebar", true},
        {"ui_dir", "ui"}
    };

    const std::string main_cpp =
        "#include <rhenium/rhenium.hpp>\n\n"
        "int main() {\n"
        "    return rhenium::App::from_config_file(\"rhenium.json\").run();\n"
        "}\n";

    const std::string index_html =
        "<!DOCTYPE html>\n"
        "<html lang=\"tr\">\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "  <title>" + raw_name + "</title>\n"
        "  <link rel=\"stylesheet\" href=\"/styles.css\">\n"
        "  <script type=\"module\" src=\"/app.js\"></script>\n"
        "</head>\n"
        "<body>\n"
        "  <main class=\"panel\">\n"
        "    <header class=\"panel-header\">\n"
        "      <div>\n"
        "        <h1>" + raw_name + "</h1>\n"
        "        <p id=\"status\" class=\"subtitle\" aria-live=\"polite\">JavaScript ile calisiyor</p>\n"
        "      </div>\n"
        "      <span class=\"pill\">JS + C++ Kabuk</span>\n"
        "    </header>\n"
        "    <div id=\"counter\" class=\"counter\" aria-live=\"polite\">0</div>\n"
        "    <div class=\"actions\">\n"
        "      <button id=\"btn-alert\" type=\"button\" class=\"btn btn-secondary\">Native Mesaj</button>\n"
        "      <button id=\"btn-reset\" type=\"button\" class=\"btn btn-secondary\">Sifirla</button>\n"
        "      <button id=\"btn-inc\" type=\"button\" class=\"btn btn-primary\">+1 Artir (JS)</button>\n"
        "    </div>\n"
        "  </main>\n"
        "</body>\n"
        "</html>\n";

    const std::string styles_css =
        ":root {\n"
        "  color-scheme: dark;\n"
        "  --color-primary: #0c5cab;\n"
        "  --color-primary-hover: #136ec9;\n"
        "  --color-surface: #09090b;\n"
        "  --color-glass: rgba(17, 17, 22, 0.8);\n"
        "  --color-border: rgba(255, 255, 255, 0.12);\n"
        "  --color-text: #fafafa;\n"
        "  --color-muted: #a1a1aa;\n"
        "  --color-focus: #60a5fa;\n"
        "}\n"
        "* { box-sizing: border-box; margin: 0; padding: 0; }\n"
        ":focus-visible { outline: 2px solid var(--color-focus); outline-offset: 2px; }\n"
        "body {\n"
        "  min-height: 100vh;\n"
        "  display: grid;\n"
        "  place-items: center;\n"
        "  background: radial-gradient(700px 360px at 50% 0%, rgba(12, 92, 171, 0.2), transparent 65%), var(--color-surface);\n"
        "  color: var(--color-text);\n"
        "  font-family: 'IBM Plex Sans', 'Segoe UI', system-ui, sans-serif;\n"
        "}\n"
        ".panel {\n"
        "  width: min(480px, 92vw);\n"
        "  padding: 24px;\n"
        "  border-radius: 14px;\n"
        "  background: var(--color-glass);\n"
        "  backdrop-filter: blur(12px);\n"
        "  border: 1px solid var(--color-border);\n"
        "  box-shadow: 0 12px 32px rgba(0, 0, 0, 0.4);\n"
        "  display: flex;\n"
        "  flex-direction: column;\n"
        "  gap: 24px;\n"
        "}\n"
        ".panel-header { display: flex; justify-content: space-between; align-items: flex-start; gap: 16px; }\n"
        "h1 { font-size: 20px; font-weight: 600; }\n"
        ".subtitle { color: var(--color-muted); font-size: 14px; margin-top: 4px; }\n"
        ".pill { padding: 3px 10px; border-radius: 999px; font-size: 12px; font-weight: 500; color: #6ee7b7; background: rgba(16, 185, 129, 0.14); border: 1px solid rgba(16, 185, 129, 0.35); }\n"
        ".counter { font-size: 40px; font-weight: 700; text-align: center; padding: 16px; border-radius: 10px; background: #0d0d12; border: 1px solid var(--color-border); }\n"
        ".actions { display: flex; gap: 8px; justify-content: flex-end; }\n"
        ".btn { min-height: 36px; padding: 0 16px; border-radius: 6px; border: 1px solid transparent; font-size: 14px; font-weight: 500; cursor: pointer; }\n"
        ".btn-primary { background: var(--color-primary); color: #fff; }\n"
        ".btn-primary:hover { background: var(--color-primary-hover); }\n"
        ".btn-secondary { background: #18181f; color: var(--color-text); border-color: var(--color-border); }\n"
        ".btn-secondary:hover { background: #22222c; }\n"
        "@media (prefers-reduced-motion: reduce) { * { transition-duration: 0.01ms !important; } }\n";

    const std::string app_js =
        "const rh = window.rhenium;\n"
        "let count = 0;\n\n"
        "const counterEl = document.getElementById('counter');\n"
        "const statusEl = document.getElementById('status');\n\n"
        "function render() {\n"
        "  counterEl.textContent = String(count);\n"
        "  statusEl.textContent = `JS sayaci: ${count}`;\n"
        "}\n\n"
        "document.getElementById('btn-inc').addEventListener('click', () => {\n"
        "  count += 1;\n"
        "  render();\n"
        "});\n\n"
        "document.getElementById('btn-reset').addEventListener('click', () => {\n"
        "  count = 0;\n"
        "  render();\n"
        "});\n\n"
        "document.getElementById('btn-alert').addEventListener('click', async () => {\n"
        "  await rh.dialog.messageBox({\n"
        "    title: '" + raw_name + "',\n"
        "    message: `JavaScript'ten C++ native dialog cagrildi! Sayac: ${count}`\n"
        "  });\n"
        "});\n\n"
        "render();\n";

    return write_text_file(project_dir / "CMakeLists.txt", cmake_content) &&
           write_text_file(project_dir / "rhenium.json", config_json.dump(2) + "\n") &&
           write_text_file(project_dir / "src" / "main.cpp", main_cpp) &&
           write_text_file(project_dir / "ui" / "index.html", index_html) &&
           write_text_file(project_dir / "ui" / "styles.css", styles_css) &&
           write_text_file(project_dir / "ui" / "app.js", app_js);
}

}
