TERMUX_PKG_HOMEPAGE=https://gitee.com/xianyugongzuoshi/Pbox
TERMUX_PKG_DESCRIPTION="A proot container management tool for Termux"
TERMUX_PKG_LICENSE="GPL-3.0"
TERMUX_PKG_LICENSE_FILE="COPYING"
TERMUX_PKG_MAINTAINER="fengchenkeji <399233159@qq.com>"
TERMUX_PKG_VERSION=0.0.1
TERMUX_PKG_SRCURL=https://gitee.com/xianyugongzuoshi/Pbox/archive/refs/tags/v${TERMUX_PKG_VERSION}.tar.gz
TERMUX_PKG_SHA256=598291455a8cea29e96f50c5da4f47fdfb1261f981cb81583809d1b815fb0b46
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
    # 文档目录
    mkdir -p "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}"

    cp -f "${TERMUX_PKG_SRCDIR}/build/main" "${INSTALL_ROOT}/pbox"
    chmod +x "${INSTALL_ROOT}/pbox"

    cp -f "${TERMUX_PKG_SRCDIR}/build/lib/"*.so "${INSTALL_ROOT}/lib/" 2>/dev/null || true
    # 注意：cmake POST_BUILD已经把对应ABI的libproot.so复制到build/lib，上面一行已经复制，此处可以删掉单独复制libproot.so的代码

    if [ -d "${TERMUX_PKG_SRCDIR}/res" ]; then
        cp -r "${TERMUX_PKG_SRCDIR}/res/"* "${INSTALL_ROOT}/res/" 2>/dev/null || true
    fi

    # 启动包装脚本
    cat > "${TERMUX_PREFIX}/bin/pbox" << 'PBLAUNCHER'
#!/data/data/com.termux/files/usr/bin/sh
exec "${PREFIX}/opt/Pbox/pbox" "$@"
PBLAUNCHER
    chmod +x "${TERMUX_PREFIX}/bin/pbox"

    ##############################
    # ====== 安装所有声明文件 ======
    ##############################
    # Pbox主许可证 GPL‑3.0
    install -Dm644 "${TERMUX_PKG_SRCDIR}/COPYING" "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}/COPYING"
    # 第三方说明文档
    install -Dm644 "${TERMUX_PKG_SRCDIR}/THIRD_PARTY_NOTICES.md" "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}/THIRD_PARTY_NOTICES.md"
    # libproot.so 上游 GPL‑2.0许可证
    install -Dm644 "${TERMUX_PKG_SRCDIR}/third_party/proot/LICENSE" "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}/proot‑COPYING"
}
