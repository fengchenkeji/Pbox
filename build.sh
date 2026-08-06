TERMUX_PKG_HOMEPAGE=https://github.com/yourname/hello-fmt
TERMUX_PKG_DESCRIPTION="A proot tools"
TERMUX_PKG_LICENSE="GPL-3.0"
TERMUX_PKG_MAINTAINER="fengchenkeji 399233159@qq.com"
TERMUX_PKG_VERSION=1.0.0
# 本地源码压缩包
TERMUX_PKG_SRCURL=https://gitee.com/xianyugongzuoshi/Pbox.git
# 务必执行 sha256sum ~/Pbox_1.0.0.tar.gz 替换这里64位哈希
TERMUX_PKG_SHA256=填写你tar包真实的sha256值
TERMUX_PKG_DEPENDS="fmt, libspdlog"
TERMUX_PKG_BUILD_IN_SRC=true

termux_step_make() {
    # termux的库默认安装在TERMUX_PREFIX，CMake find_package自动识别，不需要手动ROOT变量
    cmake -S . -B build \
        -Wno-unused-cli \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${TERMUX_PREFIX}" \

    make -C build -j"$(nproc)"
}

termux_step_make_install() {
    local INSTALL_ROOT="${TERMUX_PREFIX}/opt/Pbox"
    mkdir -p "${INSTALL_ROOT}/lib"
    mkdir -p "${TERMUX_PREFIX}/bin"

    # 复制主程序、动态库、资源文件
    cp -f "${TERMUX_PKG_SRCDIR}/build/main" "${INSTALL_ROOT}/"
    cp -f "${TERMUX_PKG_SRCDIR}/build/lib/"*.so "${INSTALL_ROOT}/lib/"
    cp -r "${TERMUX_PKG_SRCDIR}/res" "${INSTALL_ROOT}/"
}
