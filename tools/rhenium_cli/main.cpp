#include "process.hpp"
#include "templates.hpp"

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace rhenium::cli;

namespace {

void print_help() {
    std::cout
        << "Rhenium CLI v" << RHENIUM_VERSION << " — C++20 Host + JS Desktop Framework\n\n"
        << "Usage:\n"
        << "  rhenium init <name> [--dir <parent_dir>]   Create a new Rhenium project\n"
        << "  rhenium dev [project_dir]                  Build Debug and launch with live-reload\n"
        << "  rhenium build [project_dir]                Build Release with embedded assets into dist/\n"
        << "  rhenium --help                             Show this help message\n"
        << "  rhenium --version                          Show version\n";
}

int cmd_init(const std::string& raw_name, const fs::path& parent_dir) {
    const fs::path project_dir = parent_dir / utf8_to_path(raw_name);

    std::error_code ec;
    if (fs::exists(project_dir, ec)) {
        std::cerr << "Target directory already exists: " << path_to_utf8(project_dir) << "\n";
        return 1;
    }

    if (!scaffold_project(raw_name, project_dir, RHENIUM_DEFAULT_SOURCE_DIR)) {
        std::cerr << "Failed to write project files into: " << path_to_utf8(project_dir) << "\n";
        return 1;
    }

    std::cout << "Created Rhenium project at: " << path_to_utf8(project_dir) << "\n";
    std::cout << "Next steps:\n"
              << "  rhenium dev \"" << path_to_utf8(project_dir) << "\"\n"
              << "  rhenium build \"" << path_to_utf8(project_dir) << "\"\n";
    return 0;
}

int cmd_dev(const fs::path& project_dir) {
    const fs::path build_dir = project_dir / "build" / "dev";
    std::cout << "[rhenium dev] Configuring Debug build...\n";
    if (run_process({
            L"cmake", L"-S", project_dir.wstring(), L"-B", build_dir.wstring(),
            L"-G", L"Ninja", L"-DCMAKE_BUILD_TYPE=Debug"}, project_dir) != 0) {
        return 1;
    }

    std::cout << "[rhenium dev] Building...\n";
    if (run_process({L"cmake", L"--build", build_dir.wstring()}, project_dir) != 0) {
        return 1;
    }

    const auto exe = find_built_executable(build_dir);
    if (!exe.has_value()) {
        std::cerr << "[rhenium dev] Built executable not found in " << path_to_utf8(build_dir) << "\n";
        return 1;
    }

    std::cout << "[rhenium dev] Launching " << path_to_utf8(*exe) << "\n";
    return run_process({exe->wstring()}, project_dir);
}

int cmd_build(const fs::path& project_dir) {
    const fs::path build_dir = project_dir / "build" / "release";
    const fs::path dist_dir = project_dir / "dist";

    std::cout << "[rhenium build] Configuring Release build (embedded assets)...\n";
    if (run_process({
            L"cmake", L"-S", project_dir.wstring(), L"-B", build_dir.wstring(),
            L"-G", L"Ninja", L"-DCMAKE_BUILD_TYPE=Release", L"-DRHENIUM_EMBED_ASSETS=ON"}, project_dir) != 0) {
        return 1;
    }

    std::cout << "[rhenium build] Building...\n";
    if (run_process({L"cmake", L"--build", build_dir.wstring()}, project_dir) != 0) {
        return 1;
    }

    const auto exe = find_built_executable(build_dir);
    if (!exe.has_value()) {
        std::cerr << "[rhenium build] Built executable not found in " << path_to_utf8(build_dir) << "\n";
        return 1;
    }

    std::error_code ec;
    fs::create_directories(dist_dir, ec);
    fs::copy_file(*exe, dist_dir / exe->filename(), fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "[rhenium build] Failed copying executable to dist/: " << ec.message() << "\n";
        return 1;
    }

    const fs::path loader = build_dir / "WebView2Loader.dll";
    if (fs::exists(loader, ec)) {
        fs::copy_file(loader, dist_dir / "WebView2Loader.dll", fs::copy_options::overwrite_existing, ec);
    }

    std::cout << "[rhenium build] Release bundle ready in: " << path_to_utf8(dist_dir) << "\n";
    return 0;
}

}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    int argc = 0;
    LPWSTR* argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv_w) {
        std::cerr << "Failed to parse command line\n";
        return 1;
    }

    if (argc < 2) {
        LocalFree(argv_w);
        print_help();
        return 1;
    }

    const std::string sub = wide_to_utf8(argv_w[1]);
    if (sub == "--help" || sub == "-h") {
        LocalFree(argv_w);
        print_help();
        return 0;
    }
    if (sub == "--version" || sub == "-v") {
        LocalFree(argv_w);
        std::cout << "rhenium " << RHENIUM_VERSION << "\n";
        return 0;
    }
    if (sub == "init") {
        if (argc < 3) {
            LocalFree(argv_w);
            std::cerr << "Usage: rhenium init <name> [--dir <parent_dir>]\n";
            return 1;
        }
        const std::string name = wide_to_utf8(argv_w[2]);
        fs::path parent_dir = fs::current_path();
        for (int i = 3; i < argc; ++i) {
            const std::wstring opt = argv_w[i];
            if (opt == L"--dir" && i + 1 < argc) {
                parent_dir = fs::path(argv_w[++i]);
            }
        }
        LocalFree(argv_w);
        return cmd_init(name, parent_dir);
    }
    if (sub == "dev") {
        const fs::path project_dir = (argc >= 3) ? fs::path(argv_w[2]) : fs::current_path();
        LocalFree(argv_w);
        return cmd_dev(fs::absolute(project_dir));
    }
    if (sub == "build") {
        const fs::path project_dir = (argc >= 3) ? fs::path(argv_w[2]) : fs::current_path();
        LocalFree(argv_w);
        return cmd_build(fs::absolute(project_dir));
    }

    LocalFree(argv_w);
    std::cerr << "Unknown command: " << sub << "\n";
    print_help();
    return 1;
}
