TERMUX_PKG_HOMEPAGE=https://github.com/yourname/hello-fmt
TERMUX_PKG_DESCRIPTION="A proot tools"
TERMUX_PKG_LICENSE="GPL-2.0"
TERMUX_PKG_MAINTAINER="fengchenkeji 399233159@qq.com"
TERMUX_PKG_VERSION=1.0.0
TERMUX_PKG_SRCURL=file:///storage/emulated/0/Pbox/hello_fmt/hello_fmt-${TERMUX_PKG_VERSION}.tar.gz
TERMUX_PKG_SHA256=0000000000000000000000000000000000000000000000000000000000000000
TERMUX_PKG_DEPENDS="fmt, libspdlog"
TERMUX_PKG_BUILD_IN_SRC=true

termux_step_make() {
    export FMT_ROOT="${TERMUX_PREFIX}"
    export SPDLOG_ROOT="${TERMUX_PREFIX}"

    cmake -S . -B build \
        -Wno-unused-cli \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${TERMUX_PREFIX}" \
        -DFMT_ROOT="${FMT_ROOT}" \
        -DSPDLOG_ROOT="${SPDLOG_ROOT}"

    make -C build
    
}

termux_step_make_install() {
    local INSTALL_ROOT="${TERMUX_PREFIX}/opt/Pbox"
    mkdir -p "${INSTALL_ROOT}/lib"
    # 使用绝对路径，避免工作目录漂移
    cp "${TERMUX_PKG_SRCDIR}/build/main" "${INSTALL_ROOT}/main"
    cp "${TERMUX_PKG_SRCDIR}/build/libhello.so" "${INSTALL_ROOT}/lib/libhello.so"
}
