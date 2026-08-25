# Third‑Party Notices

This document describes third‑party components used by Pbox.

## libproot
- Project: proot
- Upstream: https://github.com/proot-me/proot
- License: GPL‑2.0‑or‑later
- Copyright: Proot developers
> NOTE:
> `libproot.so` is **NOT available in Termux official package repository**.
> This pre‑compiled shared library is bundled inside Pbox deb package,
> will be installed to `${PREFIX}/opt/Pbox/lib/libproot.so`.
> The full GPL‑2.0 license text is installed at `${PREFIX}/share/doc/pbox/proot‑COPYING`.

## spdlog
- Project: spdlog
- Upstream: https://github.com/gabime/spdlog
- License: MIT
- Copyright: Gabi Melman
> Note: Provided by Termux system package `libspdlog`.

## libcurl
- Project: curl
- Upstream: https://curl.se
- License: MIT
- Copyright: Daniel Stenberg and contributors
> Note: Provided by Termux system package `libcurl`.

---
Pbox itself is licensed under **GPL‑3.0‑or‑later**, see COPYING file in `${PREFIX}/share/doc/pbox/COPYING`.
Full license texts of system libraries can be found in Termux `$PREFIX/share/doc` directory.
