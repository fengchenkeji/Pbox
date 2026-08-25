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
#include "write_log.h"

// 加载动态库并获取导出函数指针
// so_path: 动态库路径 | func_name: 函数名 | out_handle: 返回dlopen句柄
// 返回函数指针，失败返回nullptr
template<typename FuncPtr>
FuncPtr load_dynamic_lib(const std::string& so_path, const std::string& func_name, void** out_handle)
{
    *out_handle = dlopen(so_path.c_str(), RTLD_LAZY);
    if (!(*out_handle))
    {
        return nullptr;
    }
    void* func = dlsym(*out_handle, func_name.c_str());
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
