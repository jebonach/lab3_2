#include "ui.hpp"
#include "Vfs.hpp"
#include "Errors.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <string_view>

static void printHelp() {
    std::cout
      << "Commands:\n"
      << "  pwd\n  ls [path]\n  cd <path>\n"
      << "  mkdir <path>\n  create <path>\n"
      << "  rm <path>\n  rename <path> <newname>\n  mv <src> <dst_dir>\n"
      << "  find <filename>\n  tree\n  save <file.json>\n  help\n  exit\n";
}

enum class Cmd { Exit, Help, Pwd, Ls, Cd, Mkdir, Create, Rm, Rename, Mv, Find, Tree, Save, Unknown };

static Cmd parseCmd(std::string_view s) {
    if (s=="exit"||s=="quit") return Cmd::Exit;
    if (s=="help")   return Cmd::Help;
    if (s=="pwd")    return Cmd::Pwd;
    if (s=="ls")     return Cmd::Ls;
    if (s=="cd")     return Cmd::Cd;
    if (s=="mkdir")  return Cmd::Mkdir;
    if (s=="create") return Cmd::Create;
    if (s=="rm")     return Cmd::Rm;
    if (s=="rename") return Cmd::Rename;
    if (s=="mv")     return Cmd::Mv;
    if (s=="find")   return Cmd::Find;
    if (s=="tree")   return Cmd::Tree;
    if (s=="save")   return Cmd::Save;
    return Cmd::Unknown;
}

static std::vector<std::string> readArgs(std::istringstream& iss) {
    std::vector<std::string> a; std::string t;
    while (iss >> t) a.push_back(std::move(t));
    return a;
}

static void printUsage(std::string_view cmd, std::string_view u) {
    std::cout << "usage: " << cmd << " " << u << "\n";
}

static void doHelp(Vfs&, const std::vector<std::string>&) { printHelp(); }
static void doPwd (Vfs& v, const std::vector<std::string>&){ std::cout << v.pwd() << "\n"; }
static void doLs  (Vfs& v, const std::vector<std::string>& a){
    if (a.size() > 1) { printUsage("ls","[path]"); return; }
    v.ls(a.empty()? "" : a[0]);
}
static void doCd  (Vfs& v, const std::vector<std::string>& a){
    if (a.size() != 1) { printUsage("cd","<path>"); return; }
    v.cd(a[0]);
}
static void doMkdir(Vfs& v, const std::vector<std::string>& a){
    if (a.size() != 1) { printUsage("mkdir","<path>"); return; }
    v.mkdir(a[0]);
}
static void doCreate(Vfs& v, const std::vector<std::string>& a){
    if (a.size() != 1) { printUsage("create","<path>"); return; }
    v.createFile(a[0]);
}
static void doRm(Vfs& v, const std::vector<std::string>& a){
    if (a.size() != 1) { printUsage("rm","<path>"); return; }
    v.rm(a[0]);
}
static void doRename(Vfs& v, const std::vector<std::string>& a){
    if (a.size() != 2) { printUsage("rename","<path> <newname>"); return; }
    v.renameNode(a[0], a[1]);
}
static void doMv(Vfs& v, const std::vector<std::string>& a){
    if (a.size() != 2) { printUsage("mv","<src> <dst_dir>"); return; }
    v.mv(a[0], a[1]);
}

static std::string fullPathOfNode(const std::shared_ptr<FSNode>& n) {
    if (!n) return "/";
    std::vector<std::string> parts;
    auto cur = n;
    while (cur) { parts.push_back(cur->name); cur = cur->parent.lock(); }
    std::string path;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        if (it == parts.rbegin() && *it == "/") { path = "/"; continue; }
        if (path.size() > 1) path += "/";
        path += *it;
    }
    if (path.empty()) path = "/";
    return path;
}

static void doFind(Vfs& v, const std::vector<std::string>& a){
    if (a.size() != 1) { printUsage("find","<filename>"); return; }
    auto n = v.findFileByName(a[0]);
    if (!n) std::cout << "not found\n";
    else    std::cout << "found: " << fullPathOfNode(n) << "\n";
}
static void doTree(Vfs& v, const std::vector<std::string>&){ v.printTree(); }
static void doSave(Vfs& v, const std::vector<std::string>& a){
    if (a.size() != 1) { printUsage("save","<file.json>"); return; }
    v.saveJson(a[0]);
}

// ---------- Main loop ----------
void runVfsCLI() {
    Vfs vfs;
    // printHelp();

    std::string line;
    while (true) {
        std::cout << vfs.pwd() << " $ ";
        if (!std::getline(std::cin, line)) break;

        std::istringstream iss(line);
        std::string cmdStr; iss >> cmdStr;
        if (cmdStr.empty()) continue;

        auto args = readArgs(iss);

        try {
            switch (parseCmd(cmdStr)) {
                case Cmd::Exit:   return;
                case Cmd::Help:   doHelp(vfs, args);   break;
                case Cmd::Pwd:    doPwd(vfs, args);    break;
                case Cmd::Ls:     doLs(vfs, args);     break;
                case Cmd::Cd:     doCd(vfs, args);     break;
                case Cmd::Mkdir:  doMkdir(vfs, args);  break;
                case Cmd::Create: doCreate(vfs, args); break;
                case Cmd::Rm:     doRm(vfs, args);     break;
                case Cmd::Rename: doRename(vfs, args); break;
                case Cmd::Mv:     doMv(vfs, args);     break;
                case Cmd::Find:   doFind(vfs, args);   break;
                case Cmd::Tree:   doTree(vfs, args);   break;
                case Cmd::Save:   doSave(vfs, args);   break;
                case Cmd::Unknown:
                default: std::cout << "unknown command (type 'help')\n"; break;
            }
        }
        catch (const VfsException& ex) {
            handleException(ex);
        }
        catch (const std::exception& e) {
            std::cout << "fatal: " << e.what() << "\n";
        }
    }
}
