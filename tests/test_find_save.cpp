#include "Vfs.hpp"
#include "Errors.hpp"
#include <cassert>
#include <iostream>

int main() {
    // find
    {
        Vfs v;
        v.createFile("/f1.txt");
        auto f = v.findFileByName("f1.txt");
        assert(f && f->isFile);
        auto no = v.findFileByName("nope.txt");
        assert(no == nullptr);
    }
    // save OK и save ошибка (не существующая папка)
    {
        Vfs v;
        v.mkdir("/dir");
        v.createFile("/dir/a.txt");
        v.saveJson("state.json"); // должно быть ок
        try { v.saveJson("no_such_dir/state.json"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::IOError && ex.code()==6);
        }
    }
    std::cout << "[OK] test_find_save\n";
}
