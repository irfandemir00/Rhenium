#include "rhenium/core/security.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace rhenium::security {

namespace {

std::string to_lower_ascii(std::string_view sv) {
    std::string out(sv);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool percent_decode(std::string_view input, std::string& output) {
    output.clear();
    output.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%') {
            if (i + 2 >= input.size()) {
                return false;
            }
            const int hi = hex_value(input[i + 1]);
            const int lo = hex_value(input[i + 2]);
            if (hi < 0 || lo < 0) {
                return false;
            }
            const char decoded = static_cast<char>((hi << 4) | lo);
            if (decoded == '\0') {
                return false;
            }
            output.push_back(decoded);
            i += 2;
        } else {
            if (input[i] == '\0') {
                return false;
            }
            output.push_back(input[i]);
        }
    }
    return true;
}

}

bool is_trusted_origin(std::string_view url) {
    const std::string lower = to_lower_ascii(url);
    constexpr std::string_view kPrefix = "https://rhenium.localhost";
    if (!lower.starts_with(kPrefix)) {
        return false;
    }

    size_t pos = kPrefix.size();
    if (pos == lower.size()) {
        return true;
    }
    if (lower.substr(pos).starts_with(":443")) {
        pos += 4;
        if (pos == lower.size()) {
            return true;
        }
    }
    const char next = lower[pos];
    return next == '/' || next == '?' || next == '#';
}

bool is_external_navigation(std::string_view url) {
    if (is_trusted_origin(url)) {
        return false;
    }
    const std::string lower = to_lower_ascii(url);
    return lower.starts_with("http://") ||
           lower.starts_with("https://") ||
           lower.starts_with("mailto:");
}

std::string normalize_asset_path(std::string_view request_path) {
    std::string_view path = request_path;

    if (is_trusted_origin(path)) {
        const size_t scheme_end = path.find("://");
        const size_t slash = path.find('/', scheme_end + 3);
        path = (slash == std::string_view::npos) ? std::string_view("/") : path.substr(slash);
    }

    if (const size_t q = path.find_first_of("?#"); q != std::string_view::npos) {
        path = path.substr(0, q);
    }

    std::string decoded;
    if (!percent_decode(path, decoded)) {
        return {};
    }

    for (char& ch : decoded) {
        if (ch == '\\') {
            ch = '/';
        }
    }

    std::vector<std::string> segments;
    size_t start = 0;
    while (start <= decoded.size()) {
        const size_t slash = decoded.find('/', start);
        const std::string seg = (slash == std::string::npos)
            ? decoded.substr(start)
            : decoded.substr(start, slash - start);

        if (seg == ".." || seg == ".") {
            return {};
        }
        if (!seg.empty()) {
            segments.push_back(seg);
        }

        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }

    if (segments.empty()) {
        return "/index.html";
    }

    std::string normalized;
    for (const auto& seg : segments) {
        normalized.push_back('/');
        normalized += seg;
    }
    return normalized;
}

std::string default_csp() {
    return "default-src 'self' https://rhenium.localhost; "
           "script-src 'self' https://rhenium.localhost; "
           "style-src 'self' 'unsafe-inline'; "
           "img-src 'self' data: blob:; "
           "font-src 'self' data:;";
}

std::string format_response_headers(std::string_view mime, std::string_view csp, bool is_html) {
    std::string headers;
    headers += "Content-Type: ";
    headers += mime;
    headers += "\r\nCache-Control: no-cache\r\n";
    headers += "X-Content-Type-Options: nosniff\r\n";
    if (is_html) {
        headers += "Content-Security-Policy: ";
        headers += csp.empty() ? default_csp() : std::string(csp);
        headers += "\r\n";
    }
    return headers;
}

}
