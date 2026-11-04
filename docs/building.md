# Building

Day-to-day builds and tests run **locally**. GitHub Actions only runs for `v*`
release tags (publishing a GitHub Release), or on demand ("Run workflow").

## Host: patched Sunshine + VM bundle (Docker)

```sh
bash host/sunshine-patches/build-local.sh     # -> out/host/clouddeck-host-*.tar.gz
CLEAN=1 bash host/sunshine-patches/build-local.sh   # from a fresh checkout
```

The script builds a `fedora:43` builder image once. The Sunshine checkout,
build tree and ccache live in Docker volumes, so after a patch change only the
touched files recompile. The first build takes about 10 minutes.

## Android (WSL / Linux, no root needed)

A user-level toolchain lives in `~/android-toolchain`: Temurin JDK 25 and
Android SDK with `platforms;android-37.0`, `ndk;29.0.14206865` and
platform-tools. The Android Gradle plugin downloads the matching build-tools
itself.

```sh
export JAVA_HOME=~/android-toolchain/jdk ANDROID_HOME=~/android-toolchain/sdk PATH=$JAVA_HOME/bin:$PATH
cd client-android
./gradlew --max-workers=6 testNonRootDebugUnitTest assembleNonRootDebug
```

**Memory:** Docker Desktop and WSL distros share one VM. Run the Docker host
build and the Gradle/qmake builds one at a time (the scripts cap themselves at
8 jobs, and the container at 10 GB).

## Protocol tests (WSL / Linux)

```sh
bash tests/mic-protocol/run.sh    # builds common/ with CMake, runs the mic packet tests + vector
```

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

## Desktop: Windows (local)

One-time setup (what this machine has):

| Tool | How |
|---|---|
| Visual Studio 2022 Build Tools, C++ workload + VC++ redist | `winget install Microsoft.VisualStudio.2022.BuildTools` (with `--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended`) |
| Qt 6.12.0 `win64_msvc2022_64` in `C:\Qt` | `aqtinstall`, pinned to the same commit as CI (in a venv under `%LOCALAPPDATA%\clouddeck-build\venv`) |
| jom | `C:\Qt\Tools\jom\jom.exe` from download.qt.io (otherwise the build falls back to nmake) |
| 7-Zip | `C:\Program Files\7-Zip` |
| WiX | restored by msbuild automatically |

Build:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-windows.ps1     # -Clean for a full rebuild
```

The script fetches the prebuilt dependencies once (`setup-deps.ps1`), runs
upstream's `scripts\build-arch.bat Release x64`, and copies
`CloudDeckClient-<version>-dev.<sha>-windows-x64.msi` and `-portable.zip`
into `out\windows\`.

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
