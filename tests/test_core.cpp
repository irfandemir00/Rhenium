#include <rhenium/core/assets.hpp>
#include <rhenium/core/config.hpp>
#include <rhenium/core/ipc.hpp>
#include <rhenium/core/security.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool cond, const char* expr, int line) {
    if (!cond) {
        std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
        ++g_failures;
    }
}

#define CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

struct TempDirGuard {
    fs::path dir;
    explicit TempDirGuard(const std::string& name) {
        dir = fs::temp_directory_path() / name;
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
    }
    ~TempDirGuard() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

void test_config() {
    rhenium::Config def;
    CHECK(def.width == 1024);
    CHECK(def.height == 680);
    CHECK(def.start_path == "/index.html");

    const nlohmann::json j = {
        {"app_id", "org.example.demo"},
        {"window", {
            {"title", "Custom Title"},
            {"width", 1280},
            {"height", 720},
            {"frameless", true}
        }},
        {"security", {
            {"csp", "default-src 'self'"},
            {"devtools", false}
        }},
        {"start_path", "app.html"}
    };
    const auto cfg = rhenium::Config::from_json(j);
    CHECK(cfg.app_id == "org.example.demo");
    CHECK(cfg.title == "Custom Title");
    CHECK(cfg.width == 1280);
    CHECK(cfg.height == 720);
    CHECK(cfg.frameless == true);
    CHECK(cfg.csp == "default-src 'self'");
    CHECK(cfg.devtools == false);
    CHECK(cfg.start_path == "/app.html");

    bool threw = false;
    try {
        (void)rhenium::Config::from_json(nlohmann::json::array({1, 2, 3}));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);

    TempDirGuard guard("rhenium_test_cfg");
    const auto missing = rhenium::Config::from_file(guard.dir / "does_not_exist.json");
    CHECK(missing.title == "Rhenium App");

    {
        std::ofstream bad(guard.dir / "bad.json");
        bad << "{not-valid-json";
    }
    bool threw_bad_json = false;
    try {
        (void)rhenium::Config::from_file(guard.dir / "bad.json");
    } catch (const std::runtime_error&) {
        threw_bad_json = true;
    }
    CHECK(threw_bad_json);
}

void test_security() {
    using namespace rhenium::security;

    CHECK(is_trusted_origin("https://rhenium.localhost"));
    CHECK(is_trusted_origin("https://rhenium.localhost/"));
    CHECK(is_trusted_origin("https://rhenium.localhost/index.html?x=1#top"));
    CHECK(is_trusted_origin("https://RHENIUM.LOCALHOST:443/styles.css"));

    CHECK(!is_trusted_origin("http://rhenium.localhost/index.html"));
    CHECK(!is_trusted_origin("https://rhenium.localhost.evil.com/index.html"));
    CHECK(!is_trusted_origin("https://evil.com/https://rhenium.localhost"));
    CHECK(!is_trusted_origin("javascript:alert(1)"));
    CHECK(!is_trusted_origin("file:///C:/Windows/win.ini"));

    CHECK(!is_external_navigation("https://rhenium.localhost/index.html"));
    CHECK(is_external_navigation("https://github.com"));
    CHECK(is_external_navigation("http://example.org"));
    CHECK(is_external_navigation("mailto:user@example.com"));

    CHECK(normalize_asset_path("/") == "/index.html");
    CHECK(normalize_asset_path("https://rhenium.localhost/") == "/index.html");
    CHECK(normalize_asset_path("https://rhenium.localhost/styles.css?v=123#hash") == "/styles.css");
    CHECK(normalize_asset_path("/sub/my%20file.txt") == "/sub/my file.txt");
    CHECK(normalize_asset_path("/../secret.txt").empty());
    CHECK(normalize_asset_path("/%2e%2e/secret.txt").empty());
    CHECK(normalize_asset_path("/foo/%00bar").empty());
    CHECK(default_csp().find("script-src 'self' https://rhenium.localhost") != std::string::npos);
}

void test_assets() {
    CHECK(rhenium::mime_type_for_path("/index.html").starts_with("text/html"));
    CHECK(rhenium::mime_type_for_path("/styles.css").starts_with("text/css"));
    CHECK(rhenium::mime_type_for_path("/icon.svg") == "image/svg+xml");
    CHECK(rhenium::mime_type_for_path("/unknown.bin") == "application/octet-stream");

    auto& reg = rhenium::EmbeddedAssetRegistry::instance();
    reg.clear();
    static const uint8_t kHtml[] = "<h1>Embedded</h1>";
    reg.register_asset("/index.html", kHtml, sizeof(kHtml) - 1);
    CHECK(reg.count() == 1);

    rhenium::AssetResolver embedded_resolver(fs::path{}, true);
    auto found = embedded_resolver.resolve("https://rhenium.localhost/");
    CHECK(found.has_value());
    CHECK(found->mime.starts_with("text/html"));
    CHECK(std::string(found->bytes.begin(), found->bytes.end()) == "<h1>Embedded</h1>");
    reg.clear();

    TempDirGuard guard("rhenium_test_assets");
    const fs::path ui_dir = guard.dir / "ui";
    fs::create_directories(ui_dir);
    {
        std::ofstream f(ui_dir / "index.html");
        f << "hello disk";
    }
    {
        std::ofstream secret(guard.dir / "secret.txt");
        secret << "top secret";
    }

    rhenium::AssetResolver disk_resolver(ui_dir, false);
    auto disk_ok = disk_resolver.resolve("/");
    CHECK(disk_ok.has_value());
    CHECK(std::string(disk_ok->bytes.begin(), disk_ok->bytes.end()) == "hello disk");

    auto traversal = disk_resolver.resolve("/../secret.txt");
    CHECK(!traversal.has_value());

    rhenium::set_default_dev_ui_dir("C:/demo/ui");
    CHECK(rhenium::get_default_dev_ui_dir() == "C:/demo/ui");
    rhenium::set_default_dev_ui_dir("");
}

void test_ipc_router() {
    rhenium::IpcRouter router;
    router.register_command("ping", []() -> std::string { return "pong"; });
    router.register_command("add", [](int a, int b) -> int { return a + b; });
    router.register_command("form_echo", [](nlohmann::json obj) -> std::string {
        return obj.value("title", "");
    });
    router.register_command("void_cmd", []() {});
    router.register_command("heavy", []() -> int { return 99; }, true);
    router.register_command("boom", []() -> int {
        throw std::runtime_error("intentional failure");
    });

    CHECK(router.has_command("ping"));
    CHECK(!router.is_async("ping"));
    CHECK(router.is_async("heavy"));

    auto r1 = router.invoke("ping", nlohmann::json::array());
    CHECK(r1.ok && r1.value == "pong");

    auto r2 = router.invoke("add", nlohmann::json::array({10, 32}));
    CHECK(r2.ok && r2.value == 42);

    auto r4 = router.invoke("form_echo", nlohmann::json::array({{{"title", "Task 1"}}}));
    CHECK(r4.ok && r4.value == "Task 1");

    auto r5 = router.invoke("void_cmd", nlohmann::json::array());
    CHECK(r5.ok && r5.value.is_null());

    auto bad_arity = router.invoke("add", nlohmann::json::array({1}));
    CHECK(!bad_arity.ok);

    auto bad_type = router.invoke("add", nlohmann::json::array({"not-int", 2}));
    CHECK(!bad_type.ok);

    auto unknown = router.invoke("missing", nlohmann::json::array());
    CHECK(!unknown.ok);

    auto thrown = router.invoke("boom", nlohmann::json::array());
    CHECK(!thrown.ok && thrown.error.find("intentional failure") != std::string::npos);
}

}

int main() {
    test_config();
    test_security();
    test_assets();
    test_ipc_router();

    if (g_failures == 0) {
        std::cout << "All Rhenium core tests passed.\n";
        return 0;
    }
    std::cerr << g_failures << " test(s) failed.\n";
    return 1;
}
