#pragma once
#include <string>
#include <vector>

inline std::vector<std::string> splitPath(const std::string& p) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c: p) {
        if (c=='/') {
            if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
        } else cur.push_back(c);
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}
