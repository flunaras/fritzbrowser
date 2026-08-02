# Fritz!Box Browser

A minimal Qt/KDE browser dedicated to your Fritz!Box web interface that logs
you back in automatically before the box's ~20 minute inactivity timeout
kicks in, so you never see the login screen again.

## How it works

Fritz!Box's login page is a client-side rendered SPA. Its actual session ID
(SID) lives only inside the page's own JavaScript state (not in a cookie),
set via a client-side PBKDF2/MD5 challenge-response computed by the box's
own JavaScript when the login form is submitted. Because of that, this app
does **not** try to reimplement the SID protocol out-of-band; instead it
drives the real login form directly:

1. Stores your Fritz!Box address, username and password (in **KWallet** or
   libsecret, whichever is available).
2. On every page load, injects JavaScript that looks for the login form's
   password field. On modern FRITZ!OS this field is nested inside a Shadow
   DOM custom element (`<password-input id="uiPass">`), so a plain
   `document.querySelector()` can never find it — the injected script
   recursively descends into every `shadowRoot` to locate it.
3. Fills in the username/password using the native input value setter plus
   `input`/`change` events (so the page's own validation logic fires), then
   clicks the real submit button (`#submitLoginBtn`) — so the box's own
   client-side challenge-response JavaScript runs exactly as it would for a
   human typing the credentials.
4. Periodically reloads the currently displayed page (default every 10
   minutes, configurable) to reset the box's ~20 minute inactivity timer.
   Reloading resends an authenticated request through the browser's actual
   session — unlike an out-of-band "keep-alive" request, this genuinely
   touches the same session the visible page is using. If the session has
   expired by the time of reload, the resulting login form is detected and
   auto-filled per step 2/3.

This has been verified end-to-end against a real FRITZ!Box 6591 Cable.

## Features

- **Tabs**: open multiple tabs (`Ctrl+T` / the `+` button), each with its
  own independent auto-login. Fritz!Box's session id isn't shared via a
  cookie (see above), so each tab that isn't already authenticated logs
  itself in independently, automatically.
- **Editable Favorites**: bookmark pages you visit often via the
  *Favorites* menu/toolbar button → *Add Current Page...*, or manage the
  whole list (add/remove/reorder/rename) via *Favorites → Manage
  Favorites...*. Favorites are always stored as a **path relative to your
  configured Fritz!Box address** (e.g. `/#/net/home_network`), never a full
  absolute URL — this keeps them valid if you ever change the Fritz!Box
  address/IP, and prevents accidentally bookmarking something unrelated.
  When a tab's current page matches a Favorite, its tab label (and the
  window title) shows the Favorite's name instead of the generic page
  title Fritz!Box's single-page-app always reports (e.g. "FRITZ!Box 6591
  Cable" regardless of which page is actually shown).
- **Start page**: in *Settings*, pick a Favorite to automatically navigate
  to right after the very first login of the session (e.g. jump straight
  to "Home Network" or "Smart Home" instead of the generic overview page).
- **HTTPS support**: enter an `https://` address in Settings to connect
  securely. Fritz!Box's default HTTPS certificate is self-signed, which
  browsers normally reject; enable *"Ignore certificate errors"* in
  Settings to bypass that check (verified end-to-end against a real box's
  HTTPS interface, including all its subresource requests, not just the
  initial page). Leave it disabled if you've installed a proper trusted
  certificate on your Fritz!Box. **This setting only takes effect after
  restarting the app** - it's implemented via Chromium's
  `--ignore-certificate-errors` startup switch (see Notes below for why),
  which can't be toggled on an already-running engine.

## Building (Docker, like `fritzhome`)

As with [fritzhome](https://github.com/flunaras/fritzhome), all official
builds happen inside Docker containers, so you don't need any Qt/KDE
development packages installed on your host — only Docker.

```sh
./docker/build.sh --distro all
# or a single target:
./docker/build.sh --distro opensuse-tumbleweed-x86_64
```

Available `--distro` aliases:

| Alias                          | Qt/KF stack              | Credential backend | Package |
|---------------------------------|---------------------------|---------------------|---------|
| `opensuse-leap-15.6-x86_64`     | Qt5, no KDE Frameworks     | libsecret            | RPM     |
| `opensuse-leap-16.0-x86_64`     | Qt6 + KF6                  | KWallet              | RPM     |
| `opensuse-tumbleweed-x86_64`    | Qt6 + KF6                  | KWallet              | RPM     |
| `opensuse-tumbleweed-aarch64`   | Qt6 + KF6 (cross-compiled) | KWallet              | RPM     |
| `ubuntu-24.04-x86_64`           | Qt6, no KDE Frameworks     | libsecret            | DEB     |

All five targets have been built and verified in CI-style Docker runs.

Resulting binaries and packages are copied to:

```
out/<family>/<distro>/<arch>/fritzbrowser
out/<family>/<distro>/<arch>/fritzbrowser-<version>-<release>.<arch>.rpm   # or .deb
```

### About the aarch64 target

Unlike a "rebuild Chromium itself for aarch64" approach (genuinely
unsupported by QtWebEngine's build system without a native or QEMU-based
target toolchain), `opensuse-tumbleweed-aarch64` doesn't need to compile
Chromium at all — it only links our own small application against
*already-compiled* aarch64 Qt6/KF6/QtWebEngine shared libraries. openSUSE
already publishes prebuilt aarch64 packages for `qt6-webenginewidgets` (and
everything it depends on), so the same "fetch prebuilt aarch64 RPMs into a
sysroot" technique `fritzhome`'s `tumbleweed-aarch64` target uses for KF6
works here too — it's just a much longer list of packages, since
QtWebEngineWidgets pulls in Qt6Quick, Qt6Qml (and its many declarative
submodules, several of which have their CMake configs in oddly-named
`*-private-devel` packages), Qt6WebChannel, Qt6Positioning, etc.

The resulting binary has been verified to be a genuine `ELF ... ARM aarch64`
executable that configures, compiles, links, and packages successfully end
to end via `cmake`/`ninja`/`cpack`. It has **not** been runtime-tested on
actual aarch64 hardware (this sandbox has no aarch64 machine or QEMU
user-mode emulation available) — the same trust model `fritzhome`'s
existing aarch64 target already relies on.

## Building manually (without Docker)

Dependencies:
- CMake >= 3.20
- Qt6 (or Qt5) with `Widgets`, `WebEngineWidgets`, `Network`
- Optional but recommended: KF6Wallet or KF5Wallet (`kwallet` KDE Frameworks
  module) for secure credential storage via KWallet. Without it, `libsecret`
  is used if available (GNOME Keyring / Secret Service), otherwise a
  plaintext `QSettings` fallback.

```sh
cmake -B build -S . -DUSE_KF=6      # or 5, 0, qt6, or omit for auto-detect
cmake --build build -j
./build/fritzbrowser
```

## Usage

On first launch you'll be prompted for:
- Fritz!Box address (e.g. `http://fritz.box` or `http://192.168.178.1`)
- Username and password (the same ones you use in the web UI)
- Page reload interval (default 600s / 10 minutes - keep this comfortably
  below the box's ~1200s/20 minute timeout)
- Optionally, a Favorite to jump to right after the initial login

Settings can be changed later via *File > Settings* or the toolbar button.

Favorites are managed via the *Favorites* menu or toolbar button:
- **Add Current Page...** bookmarks the page currently shown in the active
  tab.
- **Manage Favorites...** opens an editor to rename, reorder, add or
  remove entries.
- Clicking a Favorite in the menu navigates the *current* tab to it.

Tabs behave like a normal browser: `Ctrl+T` / the `+` button opens a new
tab (starting at your configured Fritz!Box home page), `Ctrl+W` closes the
current tab (the last remaining tab can't be closed - it's sent back to
the home page instead), and `Ctrl+L` focuses the address bar.

The window remembers its size, position, and maximized state across
restarts.

## Notes / limitations

- Verified against a FRITZ!Box 6591 Cable's login form structure
  (`#uiViewUser`, `#uiPassInput` inside a Shadow DOM, `#submitLoginBtn`).
  Older firmware without Shadow DOM / Web Components should still work,
  since the injected script falls back to a plain `input[type=password]`
  search when no Shadow DOM is present.
- If auto-login stops working after a firmware update, open the login
  page's DevTools (`QTWEBENGINE_REMOTE_DEBUGGING=<port>` env var + a
  Chromium-based browser pointed at `http://127.0.0.1:<port>`) and check
  the actual `id`/`name` of the password/submit elements; adjust the
  selectors in `buildAutoLoginScript()`/`buildDetectLoginFormScript()` (in
  `browsertab.cpp`) accordingly.
- To avoid tripping the box's brute-force block-time throttle, auto-login
  attempts are capped at 3 in a row with an 8 second cooldown between
  attempts. If it fails 3 times, you'll need to log in manually / fix your
  credentials in Settings.
- The periodic reload keep-alive reloads *every* open tab, since each tab
  has its own independent Fritz!Box session. If you keep many tabs open,
  be aware Fritz!Box boxes have a limited number of concurrent sessions.
- "Ignore certificate errors" is a single global toggle (not scoped to a
  specific host), and applies to every tab and every site you navigate to,
  not just your configured Fritz!Box address. Only enable it if you trust
  your local network. It's implemented via Chromium's
  `--ignore-certificate-errors` command-line switch (set in `main.cpp`
  before `QApplication` is constructed, based on the persisted setting),
  because `QWebEngineCertificateError` is only overridable for main-frame
  navigation - subresource requests (the page's own JS/CSS, XHR polling
  like Fritz!Box's `data.lua`, etc.) get non-overridable certificate
  errors that a per-page signal handler alone cannot bypass, and Chromium
  simply blocks them, causing a flood of SSL handshake failures and a
  partially-broken page. This is a Chromium startup-time switch, so
  toggling the setting only takes effect after restarting the app.
- Credentials are stored locally only; this app makes no external network
  calls beyond your configured Fritz!Box address.
