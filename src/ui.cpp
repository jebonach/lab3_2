#include "ui.hpp"
#include "Vfs.hpp"
#include "Errors.hpp"
#include <iostream>
#include <sstream>
#include <vector>

static void printHelp() {
    std::cout
      << "Commands:\n"
      << "  pwd\n  ls [path]\n  cd <path>\n"
      << "  mkdir <path>\n  create <path>\n"
      << "  rm <path>\n  rename <path> <newname>\n  mv <src> <dst_dir>\n"
      << "  find <filename>\n  tree\n  save <file.json>\n  help\n  exit\n";
}

void runVfsCLI() {
    Vfs vfs;
    //printHelp();

    std::string line;
    while (true) {
        std::cout << vfs.pwd() << " $ ";
        if (!std::getline(std::cin, line)) break;
        std::istringstream iss(line);
        std::string cmd; iss >> cmd;
        if (cmd.empty()) continue;

        try {
            if (cmd=="exit" || cmd=="quit") break;
            else if (cmd=="help") printHelp();
            else if (cmd=="pwd") std::cout << vfs.pwd() << "\n";
            else if (cmd=="ls") {
                std::string p; iss >> p; vfs.ls(p);
            }
            else if (cmd=="cd") {
                std::string p; iss >> p; vfs.cd(p);
            }
            else if (cmd=="mkdir") {
                std::string p; iss >> p; vfs.mkdir(p);
            }
            else if (cmd=="create") {
                std::string p; iss >> p; vfs.createFile(p);
            }
            else if (cmd=="rm") {
                std::string p; iss >> p; vfs.rm(p);
            }
            else if (cmd=="rename") {
                std::string p, nn; iss >> p >> nn; vfs.renameNode(p, nn);
            }
            else if (cmd=="mv") {
                std::string s,d; iss >> s >> d; vfs.mv(s,d);
            }
            else if (cmd=="find") {
                std::string name; iss >> name;
                auto n = vfs.findFileByName(name);
                if (!n) std::cout << "not found\n";
                else {
                    std::vector<std::string> parts;
                    auto cur = n;
                    while (cur) { parts.push_back(cur->name); cur = cur->parent.lock(); }
                    std::string path;
                    for (auto it = parts.rbegin(); it!=parts.rend(); ++it) {
                        if (it==parts.rbegin() && *it=="/") { path = "/"; continue; }
                        if (path.size()>1) path += "/";
                        path += *it;
                    }
                    std::cout << "found: " << path << "\n";
                }
            }
            else if (cmd=="tree") vfs.printTree();
            else if (cmd=="save") { std::string f; iss >> f; vfs.saveJson(f); }
            else std::cout << "unknown command (type 'help')\n";
        }
        catch (const VfsException& ex) {
            handleException(ex);
        }
        catch (const std::exception& e) {
            std::cout << "fatal: " << e.what() << "\n";
        }
    }
}
