#pragma once

#include <string>

namespace rhenium {

const std::string& get_runtime_script();
std::string build_runtime_script(bool frameless, bool resizable);

}
