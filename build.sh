TERMUX_PKG_HOMEPAGE=shturl.cc/hNkhBpazWCVhWllviVfnKgUfFDTln
TERMUX_PKG_DESCRIPTION="A proot container management tool for Termux"
TERMUX_PKG_LICENSE="GPL-3.0"
TERMUX_PKG_LICENSE_FILE="COPYING"
TERMUX_PKG_MAINTAINER="fengchenkeji <399233159@qq.com>"
TERMUX_PKG_VERSION=0.0.1
TERMUX_PKG_SRCURL=file:///storage/emulated/0/Pbox/Pbox.tar.gz
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
    mkdir -p "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}"

    # 复制主程序
    cp -f "${TERMUX_PKG_SRCDIR}/build/main" "${INSTALL_ROOT}/pbox"
    chmod +x "${INSTALL_ROOT}/pbox"

    # 直接从源码lib目录复制对应架构的so文件（不依赖cmake POST_BUILD）
    # 目录名使用全角破折号，与源码一致
    if [ "${TERMUX_ARCH}" = "aarch64" ]; then
        SO_ARCH_DIR="arm64‑v8a"
    else
        SO_ARCH_DIR="armeabi‑v7a"
    fi
    local SO_SRC_DIR="${TERMUX_PKG_SRCDIR}/lib/${SO_ARCH_DIR}"
    echo "Copying prebuilt so from: ${SO_SRC_DIR}"
    ls -la "${SO_SRC_DIR}/"
    cp -f "${SO_SRC_DIR}/"*.so* "${INSTALL_ROOT}/lib/" 2>/dev/null || true
    cp -f "${SO_SRC_DIR}/"*.so "${INSTALL_ROOT}/lib/" 2>/dev/null || true

    # 同时也复制cmake构建输出的so（initialization.so等）
    if [ -d "${TERMUX_PKG_SRCDIR}/build/lib" ]; then
        cp -f "${TERMUX_PKG_SRCDIR}/build/lib/"*.so "${INSTALL_ROOT}/lib/" 2>/dev/null || true
    fi

    # 复制资源目录
    if [ -d "${TERMUX_PKG_SRCDIR}/res" ]; then
        cp -r "${TERMUX_PKG_SRCDIR}/res/"* "${INSTALL_ROOT}/res/" 2>/dev/null || true
    fi

    # 启动脚本：优先加载自带lib目录，确保使用自带的libtalloc.so.2
    cat > "${TERMUX_PREFIX}/bin/pbox" << 'PBLAUNCHER'
#!/data/data/com.termux/files/usr/bin/sh
export LD_LIBRARY_PATH="${PREFIX}/opt/Pbox/lib:${LD_LIBRARY_PATH}"
exec "${PREFIX}/opt/Pbox/pbox" "$@"
PBLAUNCHER
    chmod +x "${TERMUX_PREFIX}/bin/pbox"

    # 许可证文件安装
    install -Dm644 "${TERMUX_PKG_SRCDIR}/COPYING" "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}/COPYING"
    install -Dm644 "${TERMUX_PKG_SRCDIR}/THIRD_PARTY_NOTICES.md" "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}/THIRD_PARTY_NOTICES.md"
    install -Dm644 "${TERMUX_PKG_SRCDIR}/third_party/proot/LICENSE" "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}/proot-COPYING"
}
