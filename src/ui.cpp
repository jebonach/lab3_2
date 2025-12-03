#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#include "Errors.hpp"
#include "Vfs.hpp"
#include "FileCommands.hpp"
#include "Compression.hpp"

// -----------------------------
// Утилиты парсинга
// -----------------------------
static std::vector<std::string> splitTokens(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

static std::string joinFrom(const std::vector<std::string>& v, std::size_t start) {
    std::string res;
    for (std::size_t i = start; i < v.size(); ++i) {
        if (!res.empty()) res.push_back(' ');
        res += v[i];
    }
    return res;
}

// -----------------------------
// Справка
// -----------------------------
static void printHelp() {
    std::cout
        << "Доступные команды:\n"
        << "  help                                  - показать эту справку\n"
        << "  pwd                                   - показать текущую директорию\n"
        << "  ls [path]                             - список содержимого\n"
        << "  tree                                  - вывести дерево\n"
        << "  cd <path>                             - перейти в директорию\n"
        << "  mkdir <path>                          - создать директорию\n"
        << "  touch <path>                          - создать пустой файл\n"
        << "  rm <path>                             - удалить файл/директорию (рекурсивно для директорий)\n"
        << "  mv <src> <dstDir>                     - переместить узел в директорию\n"
        << "  rename <path> <newName>               - переименовать файл/директорию\n"
        << "  write <file> <text> [--append]        - записать текст в файл (добавить при --append)\n"
        << "  cat <file>                            - вывести содержимое файла\n"
        << "  echo <text...> >  <file>              - перезаписать файл текстом\n"
        << "  echo <text...> >> <file>              - дописать текст в конец файла\n"
        << "  nano <file>                           - ввести текст построчно (завершение: строка '.')\n"
        << "  read <file> [offset] [count]          - прочитать байты как hex\n"
        << "  savejson <path.json>                  - сохранить VFS в JSON\n"
        << "  loadjson <path.json>                  - загрузить VFS из JSON\n"
        << "  compress <path> [--algo all|alpha]    - сжать файл/директорию (LZW var-width)\n"
        << "  uncompress <file>                     - разжать файл (по заголовку)\n"
        << "  exit | quit                           - выход\n";
}

static void printPrompt(const Vfs& vfs) {
    std::cout << vfs.pwd() << " $ ";
}

static bool parseAlgoArg(const std::vector<std::string>& args, std::size_t startIdx, CompAlgo& outAlgo) {
    outAlgo = CompAlgo::LZW_VAR_ALL;
    if (args.size() <= startIdx) return true;

    const std::string& a0 = args[startIdx];

    if (a0.rfind("--algo=", 0) == 0) {
        std::string v = a0.substr(7);
        if (v == "all")   { outAlgo = CompAlgo::LZW_VAR_ALL;   return true; }
        if (v == "alpha") { outAlgo = CompAlgo::LZW_VAR_ALPHA; return true; }
        return false;
    }

    if (a0 == "--algo") {
        if (args.size() <= startIdx + 1) return false;
        const std::string& v = args[startIdx + 1];
        if (v == "all")   { outAlgo = CompAlgo::LZW_VAR_ALL;   return true; }
        if (v == "alpha") { outAlgo = CompAlgo::LZW_VAR_ALPHA; return true; }
        return false;
    }

    return true;
}

void runVfsCLI() {
    Vfs vfs;
    printHelp();

    std::string line;
    while (true) {
        printPrompt(vfs);
        if (!std::getline(std::cin, line)) break;

auto tokens = splitTokens(line);
        if (tokens.empty()) continue;

        const std::string& cmd = tokens[0];
        std::vector<std::string> args(tokens.begin() + 1, tokens.end());

        try {
            if (cmd == "help") {
                printHelp();
            }
            else if (cmd == "exit" || cmd == "quit") {
                break;
            }
            else if (cmd == "pwd") {
                std::cout << vfs.pwd() << "\n";
            }
            else if (cmd == "ls") {
                if (args.size() > 1) { std::cout << "usage: ls [path]\n"; continue; }
                if (args.empty()) vfs.ls();
                else              vfs.ls(args[0]);
            }
            else if (cmd == "tree") {
                vfs.printTree();
            }
            else if (cmd == "cd") {
                if (args.size() != 1) { std::cout << "usage: cd <path>\n"; continue; }
                vfs.cd(args[0]);
            }
            else if (cmd == "mkdir") {
                if (args.size() != 1) { std::cout << "usage: mkdir <path>\n"; continue; }
                vfs.mkdir(args[0]);
            }
            else if (cmd == "touch") {
                if (args.size() != 1) { std::cout << "usage: touch <path>\n"; continue; }
                vfs.createFile(args[0]);
            }
            else if (cmd == "rm") {
                if (args.size() != 1) { std::cout << "usage: rm <path>\n"; continue; }
                vfs.rm(args[0]);
            }
            else if (cmd == "mv") {
                if (args.size() != 2) { std::cout << "usage: mv <src> <dstDir>\n"; continue; }
                vfs.mv(args[0], args[1]);
            }
            else if (cmd == "rename") {
                if (args.size() != 2) { std::cout << "usage: rename <path> <newName>\n"; continue; }
                vfs.renameNode(args[0], args[1]);
            }
            else if (cmd == "write") {

                if (args.size() < 2) { std::cout << "usage: write <file> <text> [--append]\n"; continue; }
                const std::string& path = args[0];

                bool append = false;
                std::vector<std::string> textParts;
                textParts.reserve(args.size() - 1);

                for (std::size_t i = 1; i < args.size(); ++i) {
                    if (args[i] == "--append" && i == args.size() - 1) {
                        append = true;
                    } else {
                        textParts.push_back(args[i]);
                    }
                }

                std::string text = joinFrom(textParts, 0);
                vfs.writeFile(path, text, append);
            }
            else if (cmd == "cat") {
                if (args.size() != 1) { std::cout << "usage: cat <file>\n"; continue; }
                FileCommands::cat(vfs, args);
            }
            else if (cmd == "echo") {
                // echo <text...> > <file> | echo <text...> >> <file>
                if (args.size() < 3) { std::cout << "usage: echo <text> >|>> <file>\n"; continue; }
                const std::string& redir = args[args.size() - 2];
                if (redir == ">") {
                    FileCommands::echo(vfs, args);
                } else if (redir == ">>") {
                    FileCommands::echoAppend(vfs, args);
                } else {
                    std::cout << "usage: echo <text> >|>> <file>\n";
                }
            }
            else if (cmd == "nano") {
                if (args.size() != 1) { std::cout << "usage: nano <file>\n"; continue; }
                FileCommands::nano(vfs, args);
            }
            else if (cmd == "read") {
                if (args.size() < 1 || args.size() > 3) { std::cout << "usage: read <file> [offset] [count]\n"; continue; }

FileCommands::read(vfs, args);
            }
            else if (cmd == "savejson") {
                if (args.size() != 1) { std::cout << "usage: savejson <path.json>\n"; continue; }
                vfs.saveJson(args[0]);
                std::cout << "saved to " << args[0] << "\n";
            }
            else if (cmd == "loadjson") {
                if (args.size() != 1) { std::cout << "usage: loadjson <path.json>\n"; continue; }
                vfs.loadJson(args[0]);
                std::cout << "loaded from " << args[0] << "\n";
            }
            else if (cmd == "compress") {
                if (args.size() != 1) { std::cout << "usage: compress <path>\n"; continue; }
                vfs.compress(args[0]);
                std::cout << "compressed: " << args[0] << "\n";
            }
            else if (cmd == "decompress") {
                if (args.size() != 1) { std::cout << "usage: uncompress <file>\n"; continue; }
                vfs.decompress(args[0]);
                std::cout << "decompressed: " << args[0] << "\n";
            }
            else {
                std::cout << "unknown command: " << cmd << "\n";
            }
        } catch (const VfsException& ex) {
            handleException(ex);
        } catch (const std::exception& ex) {
            std::cerr << "Ошибка: " << ex.what() << "\n";
        }
    }
}