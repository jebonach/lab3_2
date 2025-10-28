#include "Vfs.hpp"
#include "Errors.hpp"
#include <cassert>
#include <iostream>

int main() {
    {
        Vfs v;
        v.mkdir("/dir");
        v.createFile("/dir/file.txt");
        auto f = v.findFileByName("file.txt");
        assert(f && f->name=="file.txt" && f->isFile);
    }
    
    {
        Vfs v;
        v.mkdir("/x");
        try { v.mkdir("/x"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::InvalidArg && ex.code()==2);
        }
        v.createFile("/x/f");
        try { v.createFile("/x/f"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::InvalidArg && ex.code()==2);
        }
    }
    
    {
        Vfs v;
        try { v.mkdir(""); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::InvalidArg && ex.code()==3);
        }
        try { v.createFile("/x/.."); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::InvalidArg && ex.code()==3);
        }
        try { v.createFile("/x/inv/name"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::InvalidArg && ex.code()==3);
        }
    }
    std::cout << "[OK] test_create\n";
}
