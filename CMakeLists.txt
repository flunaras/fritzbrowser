# ── KDE Frameworks version selection ─────────────────────────────────────────
# Pass -DUSE_KF=5   → Qt5 + KF5   (e.g. openSUSE Leap 15.6, requires KDE:Frameworks5 OBS repo)
# Pass -DUSE_KF=6   → Qt6 + KF6   (e.g. openSUSE Leap 16.0 / Tumbleweed)
# Pass -DUSE_KF=0   → Qt5, NO KF  (libsecret fallback for credential storage)
# Pass -DUSE_KF=qt6 → Qt6, NO KF  (e.g. Ubuntu 24.04 LTS)
# Default: auto-detect (KF6 > KF5 > Qt-only)
cmake_minimum_required(VERSION 3.20)

set(USE_KF "" CACHE STRING "KDE Frameworks major version to use (5, 6, 0=Qt5 no KF, qt6=Qt6 no KF, empty=auto-detect)")

project(fritzbrowser
    VERSION 1.0.0
    DESCRIPTION "Browser for the Fritz!Box web UI with automatic re-login"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Determine Qt/KF version ───────────────────────────────────────────────────
if(USE_KF STREQUAL "5")
    set(_KF_MAJOR 5)
    set(_QT_MAJOR 5)
    set(_USE_KF ON)
elseif(USE_KF STREQUAL "6")
    set(_KF_MAJOR 6)
    set(_QT_MAJOR 6)
    set(_USE_KF ON)
elseif(USE_KF STREQUAL "0")
    set(_KF_MAJOR 0)
    set(_QT_MAJOR 5)
    set(_USE_KF OFF)
elseif(USE_KF STREQUAL "qt6")
    set(_KF_MAJOR 0)
    set(_QT_MAJOR 6)
    set(_USE_KF OFF)
elseif(USE_KF STREQUAL "")
    find_package(KF6 QUIET COMPONENTS CoreAddons)
    if(KF6_FOUND)
        set(_KF_MAJOR 6)
        set(_QT_MAJOR 6)
        set(_USE_KF ON)
        message(STATUS "Auto-detected KDE Frameworks 6")
    else()
        find_package(ECM QUIET NO_MODULE)
        if(ECM_FOUND)
            find_package(KF5 QUIET COMPONENTS CoreAddons)
        endif()
        if(KF5_FOUND)
            set(_KF_MAJOR 5)
            set(_QT_MAJOR 5)
            set(_USE_KF ON)
            message(STATUS "Auto-detected KDE Frameworks 5")
        else()
            set(_KF_MAJOR 0)
            set(_QT_MAJOR 6)
            set(_USE_KF OFF)
            message(STATUS "No KDE Frameworks found — building with Qt6 only")
        endif()
    endif()
else()
    message(FATAL_ERROR "USE_KF must be '5', '6', '0', 'qt6', or empty (auto-detect). Got: '${USE_KF}'")
endif()

message(STATUS "Building with Qt${_QT_MAJOR} + KF${_KF_MAJOR}")
if(_USE_KF)
    message(STATUS "  KDE Frameworks ${_KF_MAJOR} enabled (HAVE_KF=1)")
else()
    message(STATUS "  KDE Frameworks disabled (HAVE_KF=0)")
endif()

# ── ECM (only when KF is used) ────────────────────────────────────────────────
if(_USE_KF)
    find_package(ECM REQUIRED NO_MODULE)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
    include(KDEInstallDirs)
    include(KDECMakeSettings)
    include(KDECompilerSettings NO_POLICY_SCOPE)
endif()

include(FeatureSummary)

# ── Find Qt ───────────────────────────────────────────────────────────────────
find_package(Qt${_QT_MAJOR} REQUIRED COMPONENTS
    Core
    Widgets
    WebEngineWidgets
)

# ── Find KDE Frameworks (only when enabled) ───────────────────────────────────
if(_USE_KF)
    find_package(KF${_KF_MAJOR} REQUIRED COMPONENTS
        CoreAddons
        Wallet
    )
endif()

# ── Find libsecret (only when KF is NOT used) ─────────────────────────────────
# libsecret provides the freedesktop Secret Service API (GNOME keyring / KWallet
# in Secret Service mode) as a secure password backend when KDE Frameworks
# (and thus KWallet) are unavailable.
if(NOT _USE_KF)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
    endif()
    if(LIBSECRET_FOUND)
        message(STATUS "libsecret found — passwords stored in Secret Service keyring")
    else()
        message(STATUS "libsecret NOT found — falling back to QSettings (plaintext)")
    endif()
endif()

# ── Source files ──────────────────────────────────────────────────────────────
set(fritzbrowser_SRCS
    src/main.cpp
    src/mainwindow.cpp
    src/settingsdialog.cpp
    src/credentialstore.cpp
    src/secretstore.cpp
    src/browsertab.cpp
    src/favoritesstore.cpp
    src/favoritesdialog.cpp
    src/webenginepage.cpp
)

set(fritzbrowser_HDRS
    src/mainwindow.h
    src/settingsdialog.h
    src/credentialstore.h
    src/secretstore.h
    src/browsertab.h
    src/favoritesstore.h
    src/favoritesdialog.h
    src/webenginepage.h
)

add_executable(fritzbrowser
    ${fritzbrowser_SRCS}
    ${fritzbrowser_HDRS}
)

# ── Compile-time feature flags ────────────────────────────────────────────────
if(_USE_KF)
    target_compile_definitions(fritzbrowser PRIVATE HAVE_KF=1)
else()
    target_compile_definitions(fritzbrowser PRIVATE HAVE_KF=0)
endif()

if(LIBSECRET_FOUND)
    target_compile_definitions(fritzbrowser PRIVATE HAVE_LIBSECRET=1)
else()
    target_compile_definitions(fritzbrowser PRIVATE HAVE_LIBSECRET=0)
endif()

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(fritzbrowser PRIVATE
    Qt${_QT_MAJOR}::Core
    Qt${_QT_MAJOR}::Widgets
    Qt${_QT_MAJOR}::WebEngineWidgets
)

if(LIBSECRET_FOUND)
    target_link_libraries(fritzbrowser PRIVATE PkgConfig::LIBSECRET)
endif()

if(_USE_KF)
    target_link_libraries(fritzbrowser PRIVATE
        KF${_KF_MAJOR}::CoreAddons
        KF${_KF_MAJOR}::Wallet
    )
endif()

# ── Install ───────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS fritzbrowser RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES data/io.github.flunaras.fritzbrowser.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# ── CPack — RPM/DEB packaging ─────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME        "fritzbrowser")
set(CPACK_PACKAGE_VERSION     "${PROJECT_VERSION}")
set(CPACK_PACKAGE_RELEASE     "1")
set(CPACK_PACKAGE_VENDOR      "Ulf Saran")
set(CPACK_PACKAGE_CONTACT     "Ulf Saran <ulf.saran@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION
"fritzbrowser is a minimal Qt/KDE web browser dedicated to the Fritz!Box
web interface. It performs AVM's SID challenge-response login in the
background and keeps the session alive automatically, so the ~20 minute
inactivity logout never interrupts you.")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

if(NOT CPACK_GENERATOR)
    set(CPACK_GENERATOR "RPM")
endif()

# ── RPM-specific settings ─────────────────────────────────────────────────────
set(CPACK_RPM_PACKAGE_LICENSE       "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP         "Productivity/Networking/Web/Browsers")
set(CPACK_RPM_PACKAGE_URL           "https://github.com/flunaras/fritzbrowser")
set(CPACK_RPM_PACKAGE_RELEASE       "${CPACK_PACKAGE_RELEASE}")
set(CPACK_RPM_PACKAGE_ARCHITECTURE  "${CMAKE_SYSTEM_PROCESSOR}")

set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

# RPM runtime dependencies — openSUSE package names.
if(_USE_KF)
    if(_KF_MAJOR EQUAL 6)
        set(CPACK_RPM_PACKAGE_REQUIRES
            "libQt6Core6, libQt6WebEngineWidgets6, libKF6Wallet6")
    else()
        set(CPACK_RPM_PACKAGE_REQUIRES
            "libQt5Core5 >= 5.15, libQt5WebEngineWidgets5, libkwalletbackend5-5")
    endif()
else()
    set(CPACK_RPM_PACKAGE_REQUIRES
        "libQt6Core6, libQt6WebEngineWidgets6, libsecret-1-0")
endif()

set(CPACK_RPM_SPEC_MORE_DEFINE "
%post
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :
%postun
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
fi
")

# ── DEB-specific settings ─────────────────────────────────────────────────────
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ulf Saran <ulf.saran@users.noreply.github.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION    "web")
set(CPACK_DEBIAN_PACKAGE_PRIORITY   "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE   "https://github.com/flunaras/fritzbrowser")

if(_USE_KF)
    if(_KF_MAJOR EQUAL 6)
        set(CPACK_DEBIAN_PACKAGE_DEPENDS
            "libqt6core6t64, libqt6webenginewidgets6, libqt6webenginecore6")
    else()
        set(CPACK_DEBIAN_PACKAGE_DEPENDS
            "libqt5core5t64 (>= 5.15), libqt5webenginewidgets5, libqt5webenginecore5")
    endif()
elseif(_QT_MAJOR EQUAL 6)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6core6t64, libqt6webenginewidgets6, libqt6webenginecore6, libsecret-1-0")
else()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt5core5t64 (>= 5.15), libqt5webenginewidgets5, libqt5webenginecore5, libsecret-1-0")
endif()

set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

include(CPack)
