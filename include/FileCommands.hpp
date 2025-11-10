#pragma once
#include <string>
#include <vector>

class Vfs;

namespace FileCommands {

void cat(Vfs& vfs, const std::vector<std::string>& args);
void echo(Vfs& vfs, const std::vector<std::string>& args);       // echo text > file
void echoAppend(Vfs& vfs, const std::vector<std::string>& args); // echo text >> file
void nano(Vfs& vfs, const std::vector<std::string>& args);       // редактирование вручную
void read(Vfs& vfs, const std::vector<std::string>& args);       // побайтовое чтение

}
