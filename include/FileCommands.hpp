#pragma once
#include <string>
#include <vector>

class Vfs;

namespace FileCommands {

void cat(Vfs& vfs, const std::vector<std::string>& args);
void echo(Vfs& vfs, const std::vector<std::string>& args);
void echoAppend(Vfs& vfs, const std::vector<std::string>& args);
void nano(Vfs& vfs, const std::vector<std::string>& args);
void read(Vfs& vfs, const std::vector<std::string>& args);      

}
