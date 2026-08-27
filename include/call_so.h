/*
 * Pbox - Termux proot container manager
 * Copyright (C) 2026  fengchenkeji
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include <string>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <elf.h>
#include <unistd.h>
#include "write_log.h"

/* ========================================================================
 * ELF 静态符号表解析回退机制
 *
 * 问题背景：
 *   libproot.so 被编译为 PIE 可执行文件（Type=DYN PIE），而非标准共享库。
 *   其 main 符号仅存在于静态符号表 .symtab 中，不在动态符号表 .dynsym 中。
 *   dlsym() 只能查找 .dynsym，因此直接 dlsym(handle, "main") 必然失败。
 *
 * 解决方案：
 *   dlsym 失败时，手动解析 ELF 文件的 .symtab 节找到符号偏移，
 *   再结合 /proc/self/maps 中的库加载基址，计算出符号的实际运行地址。
 * ====================================================================== */

/**
 * 从 /proc/self/maps 获取已加载共享库/PIE 的基址
 * @param lib_path 库文件路径（支持部分匹配）
 * @return 加载基址指针，失败返回 nullptr
 */
static inline void* get_loaded_library_base(const char* lib_path)
{
    if (!lib_path || !*lib_path) return nullptr;

    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return nullptr;

    void* base = nullptr;
    char line[512];
    size_t path_len = strlen(lib_path);

    while (fgets(line, sizeof(line), fp))
    {
        // 每行格式: start-end perms offset dev inode pathname
        char* path = strstr(line, lib_path);
        if (path)
        {
            // 确保是完整路径匹配（前面是空格或行首）
            if (path == line || *(path - 1) == ' ')
            {
                unsigned long start = 0;
                if (sscanf(line, "%lx", &start) == 1)
                {
                    base = reinterpret_cast<void*>(start);
                    break;
                }
            }
        }
    }
    fclose(fp);
    return base;
}

/**
 * 从 ELF 文件的静态符号表 .symtab 中查找指定符号的偏移地址
 * 同时支持 Elf32 和 Elf64
 * @param elf_path ELF 文件路径
 * @param sym_name 要查找的符号名
 * @return 符号相对于加载基址的偏移（st_value），失败返回 0
 */
static inline uintptr_t find_symbol_offset_in_elf(const char* elf_path, const char* sym_name)
{
    if (!elf_path || !sym_name) return 0;

    FILE* fp = fopen(elf_path, "rb");
    if (!fp) return 0;

    // 读取 ELF 标识
    unsigned char e_ident[EI_NIDENT];
    if (fread(e_ident, 1, EI_NIDENT, fp) != EI_NIDENT)
    {
        fclose(fp);
        return 0;
    }

    // 校验 ELF magic
    if (memcmp(e_ident, ELFMAG, SELFMAG) != 0)
    {
        fclose(fp);
        return 0;
    }

    bool is64 = (e_ident[EI_CLASS] == ELFCLASS64);
    uintptr_t result = 0;

    if (is64)
    {
        // ===== 64 位 ELF 解析 =====
        Elf64_Ehdr ehdr;
        rewind(fp);
        if (fread(&ehdr, 1, sizeof(ehdr), fp) != sizeof(ehdr))
        {
            fclose(fp);
            return 0;
        }

        // 读取节头表
        if (ehdr.e_shoff == 0 || ehdr.e_shnum == 0)
        {
            fclose(fp);
            return 0;
        }

        Elf64_Shdr* shdrs = new Elf64_Shdr[ehdr.e_shnum];
        fseek(fp, ehdr.e_shoff, SEEK_SET);
        if (fread(shdrs, 1, sizeof(Elf64_Shdr) * ehdr.e_shnum, fp) != sizeof(Elf64_Shdr) * ehdr.e_shnum)
        {
            delete[] shdrs;
            fclose(fp);
            return 0;
        }

        // 找到 .symtab 节（SHT_SYMTAB）
        for (int i = 0; i < ehdr.e_shnum; i++)
        {
            if (shdrs[i].sh_type == SHT_SYMTAB && shdrs[i].sh_size > 0)
            {
                // 符号表对应的字符串表索引在 sh_link
                int strtab_idx = shdrs[i].sh_link;
                if (strtab_idx < 0 || strtab_idx >= ehdr.e_shnum) continue;

                Elf64_Shdr& strtab_sh = shdrs[strtab_idx];

                // 读取符号表
                size_t sym_count = shdrs[i].sh_size / sizeof(Elf64_Sym);
                Elf64_Sym* syms = new Elf64_Sym[sym_count];
                fseek(fp, shdrs[i].sh_offset, SEEK_SET);
                fread(syms, 1, shdrs[i].sh_size, fp);

                // 读取字符串表
                char* strtab = new char[strtab_sh.sh_size];
                fseek(fp, strtab_sh.sh_offset, SEEK_SET);
                fread(strtab, 1, strtab_sh.sh_size, fp);

                // 遍历符号表查找匹配的符号
                for (size_t j = 0; j < sym_count; j++)
                {
                    if (syms[j].st_name == 0 || syms[j].st_value == 0) continue;
                    // 只关心函数和对象符号（STT_FUNC / STT_OBJECT）
                    unsigned char type = ELF64_ST_TYPE(syms[j].st_info);
                    if (type != STT_FUNC && type != STT_OBJECT) continue;

                    const char* name = strtab + syms[j].st_name;
                    if (strcmp(name, sym_name) == 0)
                    {
                        result = static_cast<uintptr_t>(syms[j].st_value);
                        break;
                    }
                }

                delete[] syms;
                delete[] strtab;
                if (result != 0) break;
            }
        }
        delete[] shdrs;
    }
    else
    {
        // ===== 32 位 ELF 解析 =====
        Elf32_Ehdr ehdr;
        rewind(fp);
        if (fread(&ehdr, 1, sizeof(ehdr), fp) != sizeof(ehdr))
        {
            fclose(fp);
            return 0;
        }

        if (ehdr.e_shoff == 0 || ehdr.e_shnum == 0)
        {
            fclose(fp);
            return 0;
        }

        Elf32_Shdr* shdrs = new Elf32_Shdr[ehdr.e_shnum];
        fseek(fp, ehdr.e_shoff, SEEK_SET);
        if (fread(shdrs, 1, sizeof(Elf32_Shdr) * ehdr.e_shnum, fp) != sizeof(Elf32_Shdr) * ehdr.e_shnum)
        {
            delete[] shdrs;
            fclose(fp);
            return 0;
        }

        for (int i = 0; i < ehdr.e_shnum; i++)
        {
            if (shdrs[i].sh_type == SHT_SYMTAB && shdrs[i].sh_size > 0)
            {
                int strtab_idx = shdrs[i].sh_link;
                if (strtab_idx < 0 || strtab_idx >= ehdr.e_shnum) continue;

                Elf32_Shdr& strtab_sh = shdrs[strtab_idx];

                size_t sym_count = shdrs[i].sh_size / sizeof(Elf32_Sym);
                Elf32_Sym* syms = new Elf32_Sym[sym_count];
                fseek(fp, shdrs[i].sh_offset, SEEK_SET);
                fread(syms, 1, shdrs[i].sh_size, fp);

                char* strtab = new char[strtab_sh.sh_size];
                fseek(fp, strtab_sh.sh_offset, SEEK_SET);
                fread(strtab, 1, strtab_sh.sh_size, fp);

                for (size_t j = 0; j < sym_count; j++)
                {
                    if (syms[j].st_name == 0 || syms[j].st_value == 0) continue;
                    unsigned char type = ELF32_ST_TYPE(syms[j].st_info);
                    if (type != STT_FUNC && type != STT_OBJECT) continue;

                    const char* name = strtab + syms[j].st_name;
                    if (strcmp(name, sym_name) == 0)
                    {
                        result = static_cast<uintptr_t>(syms[j].st_value);
                        break;
                    }
                }

                delete[] syms;
                delete[] strtab;
                if (result != 0) break;
            }
        }
        delete[] shdrs;
    }

    fclose(fp);
    return result;
}

/**
 * dlsym 增强版：先尝试标准 dlsym（查 .dynsym），
 * 失败时回退到手动解析 ELF 静态符号表 .symtab
 * @param handle dlopen 返回的句柄
 * @param so_path 库文件路径（用于回退时解析 ELF 和查找基址）
 * @param symbol 符号名
 * @return 符号地址指针，失败返回 nullptr
 */
static inline void* dlsym_with_elf_fallback(void* handle, const char* so_path, const char* symbol)
{
    if (!handle || !so_path || !symbol) return nullptr;

    // 第一步：标准 dlsym（查找动态符号表 .dynsym）
    void* addr = dlsym(handle, symbol);
    if (addr) return addr;

    // 第二步：回退到 ELF 静态符号表解析
    // 1. 获取库在内存中的加载基址
    void* base = get_loaded_library_base(so_path);
    if (!base)
    {
        // /proc/self/maps 可能用的是不同路径表示，尝试用文件名匹配
        const char* basename = strrchr(so_path, '/');
        if (basename)
        {
            base = get_loaded_library_base(basename + 1);
        }
    }
    if (!base) return nullptr;

    // 2. 从 ELF 文件静态符号表找到符号偏移
    uintptr_t offset = find_symbol_offset_in_elf(so_path, symbol);
    if (offset == 0) return nullptr;

    // 3. 基址 + 偏移 = 实际运行地址
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(base) + offset);
}

// 加载动态库并获取导出函数指针
// so_path: 动态库路径 | func_name: 函数名 | out_handle: 返回dlopen句柄
// 返回函数指针，失败返回nullptr
// 增强：dlsym 失败时自动回退到 ELF 静态符号表解析（解决 PIE 库 main 符号不导出问题）
template<typename FuncPtr>
FuncPtr load_dynamic_lib(const std::string& so_path, const std::string& func_name, void** out_handle)
{
    *out_handle = dlopen(so_path.c_str(), RTLD_GLOBAL | RTLD_NOW);
    if (!(*out_handle))
    {
        return nullptr;
    }
    // 使用增强版 dlsym，支持从静态符号表回退查找
    void* func = dlsym_with_elf_fallback(*out_handle, so_path.c_str(), func_name.c_str());
    if (!func)
    {
        dlclose(*out_handle);
        *out_handle = nullptr;
    }
    return reinterpret_cast<FuncPtr>(func);
}

// 安全关闭动态库句柄
inline void close_lib_handle(void* handle)
{
    if (handle != nullptr)
    {
        dlclose(handle);
    }
}
