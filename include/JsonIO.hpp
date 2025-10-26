#pragma once
#include "FSNode.hpp"
#include <string>
#include <memory>

namespace JsonIO {
bool saveTreeToJsonFile(const std::shared_ptr<FSNode>& root,
                        const std::string& path);
}
