#include "Vfs.hpp"
#include "Errors.hpp"
#include <cassert>
#include <iostream>

int main() {
    {
        Vfs v; 
        assert(v.pwd() == "/");
    }
    
    {
        Vfs v;
        v.mkdir("/a");
        v.cd("/a");
        assert(v.pwd() == "/a");
        v.mkdir("b");
        v.cd("b");
        assert(v.pwd() == "/a/b");
    }
    
    {
        Vfs v;
        try { v.cd("/nope"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type() == ErrorType::PathError && ex.code() == 0);
        }
    }
    
    {
        Vfs v;
        v.createFile("/f.txt");
        try { v.cd("/f.txt"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type() == ErrorType::InvalidArg && ex.code() == 1);
        }
    }
    std::cout << "[OK] test_nav\n";
}
