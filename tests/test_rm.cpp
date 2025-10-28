#include "Vfs.hpp"
#include "Errors.hpp"
#include <cassert>
#include <iostream>

int main() {
    {
        Vfs v;
        v.mkdir("/a");
        v.createFile("/a/f");
        v.rm("/a/f");

        try { v.rm("/a/f"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::PathError && ex.code()==0);
        }
        v.mkdir("/a/b");
        v.createFile("/a/b/c");
        v.rm("/a/b"); // удаляет рекурсивно
        try { v.cd("/a/b"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::PathError && ex.code()==0);
        }
    }

    {
        Vfs v;
        try { v.rm("/"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::RootError && ex.code()==4);
        }
    }
    std::cout << "[OK] test_rm\n";
}
