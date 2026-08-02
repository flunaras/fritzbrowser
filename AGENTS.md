# AGENTS.md — Fritz!Box Browser Agent Guidance

## Project Overview

**fritzbrowser** is a minimal Qt/KDE web browser (C++17, CMake, `QtWebEngineWidgets`) dedicated to a Fritz!Box's web UI. It automatically logs in and keeps the session alive so the ~20 minute inactivity timeout never interrupts the user, and adds tabs, editable Favorites, and a configurable post-login start page on top of that.

Key quirks that make this project non-obvious:
- **Fritz!Box has no session cookie.** The SID lives only in each page's own JS/render state (see "Fritz!Box login mechanics" below). Every browser tab therefore needs its own independent auto-login logic, and "keep-alive" means reloading the page, not refreshing a cookie.
- **Modern FRITZ!OS renders its login form via Shadow DOM custom elements** (`<password-input>`). Plain `document.querySelector('input[type=password]')` cannot find it — every injected script must recursively pierce `shadowRoot`s.
- **Multi-target cross-platform build** — Qt5 (Leap 15.6) or Qt6 (Leap 16.0+, Tumbleweed, Ubuntu 24.04, Tumbleweed aarch64 cross-compiled) with optional KDE Frameworks (KF5/KF6), same `USE_KF` pattern as the sibling project `fritzhome`.
- **Docker-based build workflow** — recommended approach; all official binaries are built this way, no host Qt/KDE install needed.
- **`--ignore-certificate-errors` is a Chromium startup switch**, not a per-page setting — it can only be toggled by restarting the app.

## Build System

### Quick Build Commands

**Always use Docker (recommended — no local Qt/KDE required):**

```bash
# Build for a single distro (Tumbleweed x86_64, Release)
./docker/build.sh --distro opensuse-tumbleweed-x86_64 --build-type Release

# Build for all five supported distros
./docker/build.sh --distro all --build-type Release

# Qt5-only build (Leap 15.6, libsecret password backend)
./docker/build.sh --distro opensuse-leap-15.6-x86_64

# Qt6 + KF6 build (Leap 16.0, KWallet)
./docker/build.sh --distro opensuse-leap-16.0-x86_64

# Cross-compiled aarch64 binary (see "aarch64 cross-compile" section below)
./docker/build.sh --distro opensuse-tumbleweed-aarch64

# Ubuntu 24.04 Qt6-only build (libsecret)
./docker/build.sh --distro ubuntu-24.04-x86_64
```

Output goes to `out/<family>/<distro>/<arch>/` (binary + `.rpm`/`.deb` package). `build/<family>/<distro>/<arch>/` is the intermediate cmake/ninja work tree. Both are gitignored/regenerable — safe to `rm -rf` at any time.

### Manual (non-Docker) build

```bash
cmake -B build -S . -DUSE_KF=6      # or 5, 0, qt6, or omit for auto-detect
cmake --build build -j
./build/fritzbrowser
```

### `USE_KF` values

| Value | Qt | KDE Frameworks | Password Backend | Used by |
|-------|-----|---|---|---|
| `0` | Qt5 | None | libsecret / QSettings fallback | Leap 15.6 |
| `6` | Qt6 | KF6 | KWallet | Leap 16.0, Tumbleweed (x86_64 + aarch64) |
| `qt6` | Qt6 | None | libsecret | Ubuntu 24.04 |
| (empty) | auto | auto | auto | Default: tries KF6 → KF5 → Qt6-only |

## Architecture & Code Organization

### Source files

- `src/main.cpp` — entry point. Sets `QCoreApplication::setOrganizationName/setApplicationName` *before* constructing `QApplication`, then reads `fritzbox/ignoreCertificateErrors` from a bare `QSettings` and, if set, exports `QTWEBENGINE_CHROMIUM_FLAGS=--ignore-certificate-errors` — this **must** happen before `QApplication` is constructed (see "HTTPS / certificate errors" below).
- `src/mainwindow.h/.cpp` — top-level `QMainWindow`. Owns the `QTabWidget`, toolbar (back/forward/reload/URL bar/Favorites menu/Settings), menu bar, status bar, keep-alive `QTimer`, window geometry persistence. Delegates all per-tab browsing/login logic to `BrowserTab`.
- `src/browsertab.h/.cpp` — one `QWebEngineView` + its own independent auto-login state machine. Contains `buildDetectLoginFormScript()` (cheap, always runs) and `buildAutoLoginScript()` (fills + submits, cooldown-gated). See "Fritz!Box login mechanics" below for why these are split.
- `src/webenginepage.h/.cpp` — custom `QWebEnginePage` subclass handling TLS certificate errors, with `#if QT_VERSION_MAJOR >= 6` / `< 6` branches (Qt5: virtual `certificateError()` returning bool; Qt6: `certificateError` *signal*, not `certificateErrorRequested` — verify against the actual installed headers if touching this, the naming/API shape has changed across Qt6 minors).
- `src/credentialstore.h/.cpp` — Fritz!Box address/username/password/keep-alive interval/start-page path/ignore-cert-errors flag. Password delegated to `SecretStore`; everything else via plain `QSettings`.
- `src/secretstore.h/.cpp` — cross-backend password storage: KWallet (`HAVE_KF=1`) → libsecret/Secret Service (`HAVE_LIBSECRET=1`) → `QSettings` plaintext fallback.
- `src/favoritesstore.h/.cpp` — editable bookmark list. **Favorites store only a path relative to the configured Fritz!Box address** (e.g. `/#/net/home_network`), never an absolute URL — see "Favorites are relative paths" below.
- `src/favoritesdialog.h/.cpp` — add/remove/reorder/rename editor (`QTableWidget`).
- `src/settingsdialog.h/.cpp` — address/username/password/keep-alive interval/start-page combo/ignore-cert-errors checkbox.

### Fritz!Box login mechanics (read this before touching auto-login code)

Verified against a real FRITZ!Box 6591 Cable (FRITZ!OS 8.25):

1. **No session cookie.** `document.cookie` is empty even when logged in. The SID is embedded in the server-rendered page's own JS state after a successful `POST /index.lua`, and gets included by the page's own JS in subsequent `POST /data.lua` calls (`sid=<hex>` in the body). Opening a second tab to the same address is therefore **not already logged in** — it needs its own independent login, exactly like `BrowserTab` does.
2. **The login form is Shadow DOM.** `<password-input id="uiPass">` is a custom element; the real `<input id="uiPassInput" type="password">` lives inside its `shadowRoot`. Both `buildDetectLoginFormScript()` and `buildAutoLoginScript()` use a recursive `deepQuerySelector()` that descends into every `shadowRoot` it finds — don't replace this with a plain `document.querySelector()`, it will silently find nothing on modern firmware.
3. **We drive the real form, not a network side-channel.** An earlier implementation tried to reimplement the SID challenge-response (PBKDF2/MD5) via `QNetworkAccessManager` and inject the result as a cookie — this doesn't work at all (see point 1). The current approach fills `#uiViewUser`/`#uiPassInput` and clicks `#submitLoginBtn` via native input value setters + `input`/`change` events + a real `MouseEvent('click')`, so the box's own client-side challenge-response JS runs exactly as it would for a human. Never call `form.submit()` directly — that bypasses the hashing and POSTs the plaintext password, which the box rejects.
4. **Detection and submission are two separate scripts, on purpose.** `checkLoginState()` always runs the cheap `buildDetectLoginFormScript()` on every page load, *regardless of any cooldown*. Only if it finds a password field does `onLoginFormDetected()` check the cooldown/attempt-cap and possibly call `submitLoginForm()`. A previous bug conflated these: gating the *detection* call behind the same cooldown as the *submission* meant the app could never confirm a fresh login actually succeeded (the confirming "no password field" check got silently skipped), so `authenticated()` never fired and start-page navigation never happened. Keep these two phases separate.
5. **`authenticated()` fires only when no login form is found**, not right after clicking submit. Emitting it on "submitted successfully" races against the login's own in-flight page navigation (a start-page redirect issued at that point gets clobbered). Wait for the *next* load cycle's detection to confirm.
6. **Keep-alive = reload, not a cookie/SID refresh.** `MainWindow::onKeepAliveTimer()` just calls `tab->reload()` on every open tab. This resends an authenticated request through the browser's actual session (there's nothing else to refresh, see point 1). Reloading a POST-derived document without a window manager present doesn't show a "confirm resubmission" dialog in QtWebEngine (verified) — it just works.
7. **Auto-login attempts are capped** (3 in a row, 8s cooldown) to avoid tripping the box's brute-force block-time throttle.

### Favorites are relative paths, not absolute URLs

`Favorite::path` (and `CredentialStore::startPagePath`) store only `path + query + fragment` (`QUrl::toString(QUrl::RemoveScheme | QUrl::RemoveAuthority)`), e.g. `/#/net/home_network`. `FavoritesStore` sanitizes on save — pasting a full `http://...` URL into the editor strips the scheme/host automatically. Navigation resolves the stored path against the current `fritzBoxUrl` via `QUrl::resolved()` (`MainWindow::resolveFavoritePath()`), which correctly implements RFC 3986 reference resolution for both absolute-path (`/foo`) and fragment-only (`#/foo`) references. This means Favorites (and the start page) keep working if the user changes the Fritz!Box address/IP later — don't reintroduce absolute-URL storage.

Tab labels also use this: `MainWindow::displayLabelFor()` compares a tab's current URL (normalized the same way) against stored Favorite paths, since Fritz!Box's SPA always reports the same generic page title (e.g. "FRITZ!Box 6591 Cable") regardless of which page is actually shown.

### HTTPS / certificate errors

`QWebEngineCertificateError` is only *overridable* for main-frame navigation. Subresource requests (the page's own JS/CSS, `data.lua` XHR polling, etc.) get **non-overridable** certificate errors that a per-page `certificateError` signal/virtual handler can never accept — Chromium just blocks them outright, flooding the log with `ssl_client_socket_impl.cc handshake failed` errors while the page loads only partially. The only fix is Chromium's own `--ignore-certificate-errors` command-line switch (set via `QTWEBENGINE_CHROMIUM_FLAGS` in `main.cpp`, before `QApplication` is constructed). This is why toggling "Ignore certificate errors" in Settings requires an app restart — it cannot take effect on an already-running engine. `WebEnginePage`'s per-signal handler is kept as a harmless defense-in-depth layer for the main frame only; don't rely on it alone for full-page HTTPS support.

### aarch64 cross-compile (`docker/Dockerfile.tumbleweed-aarch64`)

We do **not** cross-compile Chromium — we only link our own small app against openSUSE's already-published prebuilt aarch64 Qt6/QtWebEngine shared libraries, fetched as RPMs into a sysroot (`fetch-pkg.sh` helper in the Dockerfile), same technique `fritzhome`'s aarch64 target uses for KF6. Toolchain file: `cmake/toolchain-aarch64.cmake` (copied from `fritzhome`, generic/reusable).

Gotchas hit while building this (check these first if the aarch64 build breaks after a Qt version bump):
- `qt6-declarative-devel` **does not exist** as a package — its content is already covered by `qt6-qml-devel`/`qt6-quick-devel`.
- Several Qt6 declarative submodules' CMake configs (`Qt6QmlModelsConfig.cmake`, `Qt6QmlMetaConfig.cmake`, `Qt6QmlWorkerScriptConfig.cmake`, `Qt6QmlCoreConfig.cmake`) live in oddly-named **`qt6-qml*-private-devel`** packages, not plain `-devel` packages, despite also containing the *public* (non-private) CMake config. If cmake reports `Could NOT find Qt6QmlModels (missing: Qt6QmlModels_DIR)`, download the candidate RPM and `rpm2cpio ... | cpio -t` to find which package actually ships the missing `usr/lib64/cmake/Qt6<X>/Qt6<X>Config.cmake` — don't guess package names, verify by inspecting real RPM contents from `https://download.opensuse.org/ports/aarch64/tumbleweed/repo/oss/{aarch64,noarch}/`.
- The resulting binary has only been confirmed to be a genuine `ELF ... ARM aarch64` executable that builds/links/packages successfully — it has not been runtime-tested on real aarch64 hardware (none available here, nor QEMU user-mode emulation).

## Testing Methodology (no aarch64/real-device CI available)

This project has **no unit test suite** — it's a thin GUI/browser-automation wrapper around a real Fritz!Box's web UI, and the risky logic (JS selectors, login timing, HTTPS behavior) can only meaningfully be validated against a real box. When changing anything in `browsertab.cpp`, `webenginepage.cpp`, or the Favorites/start-page resolution logic, verify against a real box rather than reasoning abstractly — this codebase has a history of "looks correct but isn't" bugs (Shadow DOM piercing, the detection/cooldown race, the cert-error subresource limitation) that were only caught this way:

1. Rebuild via `./docker/build.sh --distro ubuntu-24.04-x86_64` (fastest iteration target).
2. Run the binary under `Xvfb` with `QT_QPA_PLATFORM=xcb` and `QTWEBENGINE_REMOTE_DEBUGGING=<port>`.
3. Inspect the *web content* via the Chrome DevTools Protocol (`chrome-remote-interface` npm package; Playwright's `connectOverCDP` does **not** work against QtWebEngine's CDP implementation — it lacks full browser-context support). `journalctl` shows the app's own `qDebug()` output (Qt/Ubuntu routes it to the systemd journal, not the redirected stdout/stderr of a backgrounded process — don't waste time debugging "why is my log file empty").
4. Inspect *native Qt widgets* (tab bar labels, window title, dialog contents) via a screenshot: `import -window root screenshot.png` (ImageMagick) — CDP only sees the web content, not the surrounding Qt chrome.
5. To simulate real user interaction on a bare Xvfb display with no window manager (`wmctrl`/`xdotool` need one), use `python-xlib` directly: `win.configure(x=, y=, width=, height=)` to move/resize, and a `ClientMessage` with `WM_DELETE_WINDOW` to trigger a graceful `closeEvent()` (a raw `SIGTERM`/`kill` skips Qt's close-event machinery entirely).
6. For credentials during testing, temporarily add an env-var override to `CredentialStore::load()`, test, then **remove it again** before finishing — never leave test-only credential bypasses in shipped code.

Real Playwright (`chromium.launch()`, a real headless Chromium, not QtWebEngine) is useful for a *different* purpose: reverse-engineering the actual Fritz!Box page structure/JS before writing selectors (e.g. `page.evaluate()` to dump shadow-DOM-piercing DOM trees, or capturing network requests to understand the login flow) — this is how the Shadow DOM selectors and the "no cookie" session model were originally discovered.

## Code Quality Standards

- **C++17**, member variables `m_camelCase`, constants `kCamelCase`, Qt signal/slot connections in constructors.
- Keep `BrowserTab` self-contained: it should never need to know about `MainWindow`/tabs/Favorites — all cross-tab coordination (start-page navigation, Favorites menu, tab labels) belongs in `MainWindow`, driven by `BrowserTab`'s signals (`authenticated`, `titleChanged`, `urlChanged`, `statusMessage`).
- When adding a new persisted setting, follow the existing pattern: add to `CredentialStore` (getter + `save()` parameter + `QSettings` key), thread it through `SettingsDialog`, and update `README.md`'s Settings description.
- Don't reintroduce a network-level SID/cookie side-channel for auth — see "Fritz!Box login mechanics" point 3 for why that approach was abandoned.
