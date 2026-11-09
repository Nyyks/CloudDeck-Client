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
./gradlew assembleNonRootDebug         # debug APK: armeabi-v7a, arm64-v8a, x86, x86_64
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

## Linux packages (.deb)

| Package | Built by | What's in it |
|---|---|---|
| `CloudDeckClient-<v>-raspberrypi-trixie-arm64.deb` | `tools/build-deb-rpi.sh` (locally) · `.github/workflows/linux-arm64.yml` (releases) | Native build against Raspberry Pi OS Trixie's own Qt 6, SDL2, Mesa and **Raspberry Pi's FFmpeg** |
| `CloudDeckClient-<v>-linux-amd64.deb` | `tools/deb/appimage-to-deb.sh <AppImage> <v> <dir>` · `build.yml` (after the AppImage) | The x86_64 AppImage's contents under `/opt/clouddeck-client`, so one package fits Debian, Ubuntu, Mint… |

Both install a `clouddeck-client` command and a menu entry, and use the shared
`tools/deb/package.sh` for the control files. The binary isn't installed as `/usr/bin/moonlight`,
because stock Moonlight's package owns that path; both can be installed at once.

**Raspberry Pi .deb:**
- `tools/build-deb-rpi.sh` runs in WSL or on Linux with Docker and arm64 binfmt (QEMU). It runs
  `tools/deb/rpi-trixie-build.sh` in an arm64 `debian:trixie` container with Raspberry Pi's
  archive added, as on a Pi.
  - Under emulation it takes about an hour.
  - The archive key comes from Raspberry Pi's keyring package, pinned by SHA-256. The older
    `raspberrypi.gpg.key` has a SHA-1 binding that Trixie's apt rejects.
  - The build fails unless `libavcodec-dev` is Raspberry Pi's `+rpt` build.
  - `dpkg-shlibdeps` works out the library dependencies. The QML modules and Qt plugins are
    listed by hand, because they load at run time.
- **Why it's native:** Raspberry Pi's FFmpeg has the V4L2 *request* decoders.
  - They drive the Pi 5's only video decoder, HEVC.
  - Upstream FFmpeg only has V4L2 M2M: the Pi 4's H.264 decoder, and nothing on a Pi 5.
  - The arm64 AppImage (1.0–1.1) bundled upstream FFmpeg, so a Pi 5 decoded in software.
- "Run workflow" on *Raspberry Pi* with `release_tag` attaches a build to an existing release.

**Generic .deb:** the dependency scan maps every system library the bundle uses to its
package. Libraries that aren't installed on the build machine are looked up in a table in
the script, and the build stops on any it doesn't know.
