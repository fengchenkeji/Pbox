TERMUX_PKG_HOMEPAGE=https://github.com/fengchenkeji/Pbox
TERMUX_PKG_DESCRIPTION="A proot container management tool for Termux (bundled proot)"
TERMUX_PKG_LICENSE="GPL-3.0"
TERMUX_PKG_LICENSE_FILE="COPYING"
TERMUX_PKG_MAINTAINER="fengchenkeji <399233159@qq.com>"
TERMUX_PKG_VERSION=0.0.2
TERMUX_PKG_DEPENDS="libspdlog, libcurl"
TERMUX_PKG_BUILD_IN_SRC=true

# 从 termux 仓库下载指定包的 .deb 并解压到目标目录
# 用法: _pbox_fetch_deb <包名> <目标解压目录>
_pbox_fetch_deb() {
    local pkg="$1"
    local dest="$2"
    local repo_arch="$TERMUX_ARCH"
    local repo="https://packages.termux.dev/apt/termux-main"

    echo "[pbox] Fetching ${pkg}.deb for ${repo_arch}..."

    # 下载包索引
    curl -fL "${repo}/dists/stable/main/binary-${repo_arch}/Packages" -o /tmp/pbox_packages 2>/dev/null || {
        echo "[pbox] WARN: failed to download Packages index"
        return 1
    }

    local deb_path
    deb_path=$(grep -A30 "^Package: ${pkg}$" /tmp/pbox_packages | grep "^Filename:" | head -1 | awk '{print $2}')
    if [ -z "$deb_path" ]; then
        echo "[pbox] ERROR: package ${pkg} not found in repo"
        return 1
    fi

    local deb_url="${repo}/${deb_path}"
    local deb_file="/tmp/pbox_${pkg}.deb"
    curl -fL "$deb_url" -o "$deb_file" || return 1

    mkdir -p "$dest"
    dpkg-deb -x "$deb_file" "$dest"
    echo "[pbox] Extracted ${pkg} to ${dest}"
    return 0
}

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
    mkdir -p "${INSTALL_ROOT}/bin"
    mkdir -p "${INSTALL_ROOT}/res"
    mkdir -p "${TERMUX_PREFIX}/bin"
    mkdir -p "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}"

    # 1. 复制 Pbox 主程序
    cp -f "${TERMUX_PKG_SRCDIR}/build/main" "${INSTALL_ROOT}/pbox"
    chmod +x "${INSTALL_ROOT}/pbox"

    # 2. 复制 libinitialization.so
    if [ -d "${TERMUX_PKG_SRCDIR}/build/lib" ]; then
        cp -f "${TERMUX_PKG_SRCDIR}/build/lib/"*.so "${INSTALL_ROOT}/lib/" 2>/dev/null || true
    fi

    # 3. 下载并打包自带 proot（不依赖系统 proot 包）
    local PROOT_EXTRACT="/tmp/pbox-proot-extract"
    rm -rf "$PROOT_EXTRACT"
    if _pbox_fetch_deb "proot" "$PROOT_EXTRACT"; then
        local PROOT_BIN="${PROOT_EXTRACT}${TERMUX_PREFIX}/bin/proot"
        if [ -f "$PROOT_BIN" ]; then
            cp -f "$PROOT_BIN" "${INSTALL_ROOT}/bin/proot"
            chmod +x "${INSTALL_ROOT}/bin/proot"
            echo "[pbox] Installed bundled proot: ${INSTALL_ROOT}/bin/proot"
        fi

        # 复制 proot loader 文件（Android 版 proot 需要）
        local PROOT_LIBEXEC="${PROOT_EXTRACT}${TERMUX_PREFIX}/libexec/proot"
        if [ -d "$PROOT_LIBEXEC" ]; then
            cp -f "$PROOT_LIBEXEC"/* "${INSTALL_ROOT}/bin/" 2>/dev/null || true
        fi
    else
        echo "[pbox] ERROR: failed to fetch proot deb"
        exit 1
    fi

    # 4. 下载并打包 libtalloc（proot 运行时依赖）
    local TALLOC_EXTRACT="/tmp/pbox-talloc-extract"
    rm -rf "$TALLOC_EXTRACT"
    if _pbox_fetch_deb "libtalloc" "$TALLOC_EXTRACT"; then
        cp -f "${TALLOC_EXTRACT}${TERMUX_PREFIX}/lib/"libtalloc.so* "${INSTALL_ROOT}/lib/" 2>/dev/null || true
        echo "[pbox] Installed bundled libtalloc"
    fi

    # 5. 复制资源目录
    if [ -d "${TERMUX_PKG_SRCDIR}/res" ]; then
        cp -r "${TERMUX_PKG_SRCDIR}/res/"* "${INSTALL_ROOT}/res/" 2>/dev/null || true
    fi

    # 6. 启动脚本：设置库搜索路径为 Pbox 自带目录
    cat > "${TERMUX_PREFIX}/bin/pbox" << 'PBLAUNCHER'
#!/data/data/com.termux/files/usr/bin/sh
# Pbox 启动脚本
PBOX_HOME="${PREFIX}/opt/Pbox"
export LD_LIBRARY_PATH="${PBOX_HOME}/lib:${LD_LIBRARY_PATH}"
exec "${PBOX_HOME}/pbox" "$@"
PBLAUNCHER
    chmod +x "${TERMUX_PREFIX}/bin/pbox"

    # 7. 许可证文件
    install -Dm644 "${TERMUX_PKG_SRCDIR}/COPYING" "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}/COPYING"
    install -Dm644 "${TERMUX_PKG_SRCDIR}/THIRD_PARTY_NOTICES.md" "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}/THIRD_PARTY_NOTICES.md"
    install -Dm644 "${TERMUX_PKG_SRCDIR}/third_party/proot/LICENSE" "${TERMUX_PREFIX}/share/doc/${TERMUX_PKG_NAME}/proot-COPYING"
}
