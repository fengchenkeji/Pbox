# Pbox
## 项目介绍
Pbox 是一款运行在 Termux 环境下的轻量 proot 工具箱，无需 Android 设备 Root 权限，即可快速创建、初始化并管理 proot 容器，轻松搭建隔离的 Linux 运行环境。

## 编译方式
项目基于 `termux-packages` 体系进行编译打包，通过内置 `build.sh` 脚本完成自动化编译、库文件部署与安装。

## 依赖
- `libspdlog`：日志组件，终端输出全等级日志，仅 ERROR 级别日志按日期写入本地日志文件
- `proot`：容器核心程序，实现无 root 容器虚拟化能力

## 核心功能
1. 极简创建 proot 容器，自动完成目录挂载、运行路径配置；
2. 自动适配动态链接库查找路径，解决 Termux 下自定义 so 库加载失败问题；
3. 完善的日志能力：控制台打印全部日志，错误日志持久化存储在 `log/` 目录，文件名格式为 `YYYY-MM-DD.log`；
4. 安装后全局命令调用，直接在 Termux 终端输入 `Pbox` 即可运行工具箱；
5. 统一管理容器资源文件、依赖动态库，开箱即用。

## 快速编译&安装
```bash
# 进入项目根目录执行编译脚本
git clone https://github.com/termux/termux-packages.git
git clone https://gitee.com/xianyugongzuoshi/Pbox.git
termux-packages/build-package.sh ~/Pbox
