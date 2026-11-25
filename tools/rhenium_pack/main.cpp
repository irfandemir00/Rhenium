#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string wide_to_utf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    const int len = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), len, nullptr, nullptr);
    return out;
}

std::string path_to_utf8(const fs::path& p) {
    return wide_to_utf8(p.wstring());
}

std::string escape_cpp_string(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

}

int main() {
    int argc = 0;
    LPWSTR* argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv_w) {
        std::cerr << "rhenium_pack: failed to parse command line\n";
        return 1;
    }

    fs::path input_dir;
    fs::path output_file;

    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv_w[i];
        if (arg == L"--input" && i + 1 < argc) {
            input_dir = fs::path(argv_w[++i]);
        } else if (arg == L"--output" && i + 1 < argc) {
            output_file = fs::path(argv_w[++i]);
        } else {
            LocalFree(argv_w);
            std::cerr << "Usage: rhenium_pack --input <ui_dir> --output <out.cpp>\n";
            return 1;
        }
    }
    LocalFree(argv_w);

    if (input_dir.empty() || output_file.empty()) {
        std::cerr << "Usage: rhenium_pack --input <ui_dir> --output <out.cpp>\n";
        return 1;
    }

    std::error_code ec;
    if (!fs::exists(input_dir, ec) || !fs::is_directory(input_dir, ec)) {
        std::cerr << "rhenium_pack: input directory does not exist: " << path_to_utf8(input_dir) << "\n";
        return 1;
    }

    struct AssetItem {
        std::string virtual_path;
        fs::path disk_path;
    };

    std::vector<AssetItem> items;
    for (const auto& entry : fs::recursive_directory_iterator(input_dir, ec)) {
        if (ec) {
            std::cerr << "rhenium_pack: directory walk failed: " << ec.message() << "\n";
            return 1;
        }
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        fs::path rel = fs::relative(entry.path(), input_dir, ec);
        if (ec) {
            continue;
        }
        std::string rel_str = path_to_utf8(rel);
        std::replace(rel_str.begin(), rel_str.end(), '\\', '/');
        if (rel_str.empty() || rel_str.front() != '/') {
            rel_str.insert(rel_str.begin(), '/');
        }
        items.push_back(AssetItem{std::move(rel_str), entry.path()});
    }

    std::sort(items.begin(), items.end(), [](const AssetItem& a, const AssetItem& b) {
        return a.virtual_path < b.virtual_path;
    });

    if (output_file.has_parent_path()) {
        fs::create_directories(output_file.parent_path(), ec);
    }

    std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "rhenium_pack: cannot open output file: " << path_to_utf8(output_file) << "\n";
        return 1;
    }

    out << "#include <cstddef>\n";
    out << "#include <cstdint>\n";
    out << "#include \"rhenium/core/assets.hpp\"\n\n";
    out << "namespace {\n\n";

    std::vector<size_t> sizes;
    sizes.reserve(items.size());

    for (size_t idx = 0; idx < items.size(); ++idx) {
        std::ifstream in(items[idx].disk_path, std::ios::binary | std::ios::ate);
        if (!in.is_open()) {
            std::cerr << "rhenium_pack: cannot read asset: " << path_to_utf8(items[idx].disk_path) << "\n";
            return 1;
        }
        const std::streamsize raw_size = in.tellg();
        in.seekg(0, std::ios::beg);

        std::vector<uint8_t> bytes;
        if (raw_size > 0) {
            bytes.resize(static_cast<size_t>(raw_size));
            if (!in.read(reinterpret_cast<char*>(bytes.data()), raw_size)) {
                std::cerr << "rhenium_pack: read error on: " << path_to_utf8(items[idx].disk_path) << "\n";
                return 1;
            }
        }
        sizes.push_back(bytes.size());

        out << "const uint8_t kAsset_" << idx << "[] = {";
        if (bytes.empty()) {
            out << "0";
        } else {
            out << std::hex << std::setfill('0');
            for (size_t b = 0; b < bytes.size(); ++b) {
                if (b % 16 == 0) {
                    out << "\n    ";
                }
                out << "0x" << std::setw(2) << static_cast<unsigned>(bytes[b]);
                if (b + 1 < bytes.size()) {
                    out << ", ";
                }
            }
            out << std::dec;
            out << "\n";
        }
        out << "};\n\n";
    }

    out << "struct RheniumEmbeddedRegistrar {\n";
    out << "    RheniumEmbeddedRegistrar() {\n";
    out << "        auto& reg = rhenium::EmbeddedAssetRegistry::instance();\n";
    for (size_t idx = 0; idx < items.size(); ++idx) {
        out << "        reg.register_asset(\"" << escape_cpp_string(items[idx].virtual_path)
            << "\", kAsset_" << idx << ", " << sizes[idx] << ");\n";
    }
    out << "    }\n";
    out << "} g_rhenium_embedded_registrar;\n\n";
    out << "}\n";

    if (!out.good()) {
        std::cerr << "rhenium_pack: failed writing output file\n";
        return 1;
    }

    return 0;
}
