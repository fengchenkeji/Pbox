TERMUX_PKG_HOMEPAGE=https://gitee.com/xianyugongzuoshi/Pbox
TERMUX_PKG_DESCRIPTION="A proot container management tool for Termux"
TERMUX_PKG_LICENSE="GPL-3.0"
TERMUX_PKG_MAINTAINER="fengchenkeji <399233159@qq.com>"
TERMUX_PKG_VERSION=0.0.1
TERMUX_PKG_SRCURL=https://gitee.com/xianyugongzuoshi/Pbox/archive/refs/tags/v${TERMUX_PKG_VERSION}.tar.gz
TERMUX_PKG_SHA256=SKIP
TERMUX_PKG_DEPENDS="libspdlog, libcurl"
TERMUX_PKG_BUILD_IN_SRC=true

termux_step_make() {
    cmake -S . -B build \
        -Wno-unused-cli \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${TERMUX_PREFIX}" \
        -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
        -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}"

    make -C build -j"$(nproc)"
}

termux_step_make_install() {
    local INSTALL_ROOT="${TERMUX_PREFIX}/opt/Pbox"
    mkdir -p "${INSTALL_ROOT}/lib"
    mkdir -p "${INSTALL_ROOT}/res"
    mkdir -p "${TERMUX_PREFIX}/bin"

    cp -f "${TERMUX_PKG_SRCDIR}/build/main" "${INSTALL_ROOT}/pbox"
    chmod +x "${INSTALL_ROOT}/pbox"

    cp -f "${TERMUX_PKG_SRCDIR}/build/lib/"*.so "${INSTALL_ROOT}/lib/" 2>/dev/null || true
    if [ -f "${TERMUX_PKG_SRCDIR}/lib/libproot.so" ]; then
        cp -f "${TERMUX_PKG_SRCDIR}/lib/libproot.so" "${INSTALL_ROOT}/lib/"
    fi

    if [ -d "${TERMUX_PKG_SRCDIR}/res" ]; then
        cp -r "${TERMUX_PKG_SRCDIR}/res/"* "${INSTALL_ROOT}/res/" 2>/dev/null || true
    fi

    cat > "${TERMUX_PREFIX}/bin/pbox" << 'PBLAUNCHER'
#!/data/data/com.termux/files/usr/bin/sh
exec "${PREFIX}/opt/Pbox/pbox" "$@"
PBLAUNCHER
    chmod +x "${TERMUX_PREFIX}/bin/pbox"
}
