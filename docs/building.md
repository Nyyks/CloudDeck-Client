# Building

CI (`.github/workflows/build.yml`) is the reference build for every platform.
Pushing to `main`, `feat/**` or `fix/**` builds everything. Pushing a `v*` tag
also publishes a GitHub Release (prerelease) with all artifacts.

## Clone

```sh
git clone --recurse-submodules https://github.com/Nyyks/CloudDeck-Client.git
git config core.hooksPath .githooks      # gitleaks pre-commit hook; needs gitleaks on PATH
```

The clients don't carry their own moonlight-common-c. Both build `common/`
(ADR-0001).

## Desktop: Linux (quickest local loop)

Any distro with Qt 6.5+, FFmpeg, SDL2 (or sdl2-compat), SDL2_ttf, Opus and
OpenSSL dev packages works. Ubuntu 26.04 / WSL2 is known to work:

```sh
mkdir build && cd build
qmake6 ../client-desktop/moonlight-qt.pro CONFIG+=debug
make -j$(nproc)
./app/moonlight
```

AppImage: CI runs `client-desktop/scripts/build-appimage.sh` after building the
pinned dependency stack with `.github/scripts/build-linux-deps.sh` (cached).

## Desktop: Windows

Requires Visual Studio 2022+, Qt 6.12 (msvc2022_64), 7-Zip and the WiX toolset
(restored by msbuild).

```bat
cd client-desktop
powershell .\setup-deps.ps1
set PATH=C:\Qt\6.12.0\msvc2022_64\bin;%PATH%
scripts\build-arch.bat Release x64
```

Outputs: `build\build-x64-release\*.msi` and
`build\installer-x64-release\MoonlightPortable-x64-*.zip`. CI renames them to
`CloudDeckClient-<version>-windows-x64*` (ADR-0005).

## Android

JDK 25 and NDK 29.0.14206865 (from `app/build.gradle`).

```sh
cd client-android
./gradlew testNonRootDebugUnitTest     # JVM unit tests (shared fixtures)
./gradlew assembleNonRootDebug         # debug APK, arm64-v8a + x86_64
```

The APK is debug-signed. Release signing will use a keystore that lives only in
GitHub secrets.

## Unit tests

| Suite | Command |
|---|---|
| Desktop CloudDeck helpers | `qmake6 client-desktop/tests/clouddeck/clouddeck-tests.pro && make && ./tst_clouddeckapi` |
| Android CloudDeck helpers | `./gradlew testNonRootDebugUnitTest` (in `client-android/`) |

Both suites replay `shared/fixtures/clouddeck/*.json`. Change a case there, and
both implementations must agree.
