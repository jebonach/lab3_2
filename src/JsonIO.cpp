#include "JsonIO.hpp"
#include <fstream>
#include <sstream>

namespace {
std::string esc(const std::string& s) {
    std::string o; o.reserve(s.size()+8);
    for (char c: s) {
        if (c=='"') o += "\\\"";
        else if (c=='\\') o += "\\\\";
        else if (c=='\n') o += "\\n";
        else o += c;
    }
    return o;
}

void toJsonRec(const std::shared_ptr<FSNode>& n, std::ostringstream& out, int indent) {
    auto ind = std::string(indent, ' ');
    out << ind << "{\n";
    out << ind << "  \"name\": \"" << esc(n->name) << "\",\n";
    out << ind << "  \"type\": \"" << (n->isFile? "file":"folder") << "\"";
    if (!n->isFile && !n->children.empty()) {
        out << ",\n" << ind << "  \"children\": [\n";
        bool first = true;
        for (auto& [_, bucket] : n->children) {
            if (bucket.dir) {
                if (!first) out << ",\n";
                first = false;
                toJsonRec(bucket.dir, out, indent+4);
            }
            if (bucket.file) {
                if (!first) out << ",\n";
                first = false;
                toJsonRec(bucket.file, out, indent+4);
            }
        }
        out << "\n" << ind << "  ]\n" << ind << "}";
    } else {
        out << "\n" << ind << "}";
    }
}
}

namespace JsonIO {
bool saveTreeToJsonFile(const std::shared_ptr<FSNode>& root,
                        const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    f << treeToJson(root);
    return true;
}

std::string treeToJson(const std::shared_ptr<FSNode>& root) {
    std::ostringstream out;
    toJsonRec(root, out, 0);
    out << "\n";
    return out.str();
}
}
