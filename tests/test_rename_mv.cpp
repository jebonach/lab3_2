#include "Vfs.hpp"
#include "Errors.hpp"
#include <cassert>
#include <iostream>

int main() {
    // rename файла, затем найти по новому имени
    {
        Vfs v;
        v.mkdir("/a");
        v.createFile("/a/f.txt");
        v.renameNode("/a/f.txt", "g.txt");
        auto fOld = v.findFileByName("f.txt");
        auto fNew = v.findFileByName("g.txt");
        assert(!fOld);
        assert(fNew && fNew->name=="g.txt");
    }
    // rename конфликт по имени
    {
        Vfs v;
        v.mkdir("/a");
        v.createFile("/a/f");
        v.createFile("/a/g");
        try { v.renameNode("/a/g","f"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::InvalidArg && ex.code()==2);
        }
    }
    // rename корня
    {
        Vfs v;
        try { v.renameNode("/","newroot"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::RootError && ex.code()==4);
        }
    }
    // mv директории и файл
    {
        Vfs v;
        v.mkdir("/a");
        v.mkdir("/b");
        v.createFile("/a/f");
        v.mv("/a/f","/b");
        // теперь файл в /b
        v.cd("/b");
        // проверим, что найти можно по имени
        auto f = v.findFileByName("f");
        assert(f && f->parent.lock()->name=="b");
    }
    // mv в поддерево самого себя
    {
        Vfs v;
        v.mkdir("/a");
        v.mkdir("/a/b");
        try { v.mv("/a","/a/b"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::Conflict && ex.code()==5);
        }
    }
    // mv: dest not found / dest not dir
    {
        Vfs v;
        v.mkdir("/a"); v.createFile("/a/f");
        try { v.mv("/a/f","/nope"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::PathError && ex.code()==8);
        }
        v.createFile("/x");
        try { v.mv("/a","/x"); assert(!"must throw"); }
        catch (const VfsException& ex) {
            assert(ex.type()==ErrorType::InvalidArg && ex.code()==9);
        }
    }
    std::cout << "[OK] test_rename_mv\n";
}
