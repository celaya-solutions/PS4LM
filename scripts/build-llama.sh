#!/usr/bin/env bash
# Cross-compiles llama.cpp's tools for the PS4.
#
#   scripts/build-llama.sh                 configure and build into build/llama-ps4
#   scripts/build-llama.sh llama-bench     build one target
#
# The tools land in build/llama-ps4/bin as <tool>.elf.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=env.sh
source "$root/scripts/env.sh"

llama="$root/third_party/llama.cpp"
out="$root/build/llama-ps4"
targets=("${@:-llama-server llama-bench llama-cli}")

if [ ! -f "$llama/CMakeLists.txt" ]; then
    echo "build-llama: run git submodule update --init first" >&2
    exit 1
fi

# The pinned llama.cpp. A shallow submodule counts one commit, so llama.cpp
# would call itself build 1 and fetch the "latest" web UI instead of the one
# made for this tag; the build number is passed in. Change both lines with
# the submodule.
llama_tag=b11608
llama_commit=04f7226a2c12c7f382c545a6db5a034266aa53cb
if [ "$(git -C "$llama" rev-parse HEAD)" != "$llama_commit" ]; then
    echo "build-llama: third_party/llama.cpp is not at $llama_tag ($llama_commit)" >&2
    exit 1
fi
if [ ! -x "$PS4_PAYLOAD_SDK/bin/orbis-clang" ]; then
    echo "build-llama: no SDK at $PS4_PAYLOAD_SDK, run scripts/setup-sdk.sh" >&2
    exit 1
fi

# The PS4 changes to llama.cpp live in patches/ and go onto the submodule's
# working tree; a patch that is already applied is skipped.
for p in "$root"/patches/*.patch; do
    [ -e "$p" ] || continue
    if git -C "$llama" apply --reverse --check "$p" 2>/dev/null; then
        continue
    fi
    git -C "$llama" apply "$p"
    echo "build-llama: applied $(basename "$p")"
done

# Compiler flags come from ps4/toolchain.cmake and ps4/CMakeLists.txt; setting
# CMAKE_C_FLAGS or CMAKE_CXX_FLAGS here would drop the toolchain's.
# Jaguar: SSE4.2, AVX, F16C; no AVX2, FMA, BMI2 or AVX-512 (-march comes from
# ps4/toolchain.cmake). No OpenMP in the SDK, so ggml's own thread pool.
# Static everything: a payload is one ELF. No TLS: models arrive by FTP.
# llamafile's sgemm.cpp calls AVX2 integer intrinsics in its AVX-only path
# (denibble, bittobyte, updot), so it is off (docs/PORTING.md 3).
opts=(
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=OFF
    -DGGML_NATIVE=OFF
    -DGGML_BACKEND_DL=OFF
    -DGGML_CPU_ALL_VARIANTS=OFF
    -DGGML_SSE42=ON
    -DGGML_AVX=ON
    -DGGML_F16C=ON
    -DGGML_AVX2=OFF
    -DGGML_FMA=OFF
    -DGGML_BMI2=OFF
    -DGGML_AVX512=OFF
    -DGGML_OPENMP=OFF
    -DGGML_METAL=OFF
    -DGGML_BLAS=OFF
    -DGGML_LLAMAFILE=OFF
    -DGGML_CCACHE=OFF
    -DLLAMA_OPENSSL=OFF
    -DLLAMA_BUILD_COMMON=ON
    -DLLAMA_BUILD_TESTS=OFF
    -DLLAMA_BUILD_EXAMPLES=OFF
    -DLLAMA_BUILD_TOOLS=ON
    -DLLAMA_BUILD_SERVER=ON
)

# Every option must be one the pinned llama.cpp declares; a renamed or removed
# one would otherwise be ignored without a word.
for o in "${opts[@]}"; do
    name="${o#-D}"
    name="${name%%=*}"
    case "$name" in CMAKE_*|BUILD_SHARED_LIBS) continue ;; esac
    if ! grep -qE "^[[:space:]]*option\($name[[:space:]]" \
            "$llama/CMakeLists.txt" "$llama/ggml/CMakeLists.txt"; then
        echo "build-llama: $name is not an option of the pinned llama.cpp" >&2
        exit 1
    fi
done

mkdir -p "$out"
cmake -S "$root/ps4" -B "$out" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$root/ps4/toolchain.cmake" \
    -DCMAKE_VERBOSE_MAKEFILE=OFF \
    -DPS4LM_LOG_HOST="$PS4LM_LOG_HOST" \
    -DPS4LM_LOG_PORT="$PS4LM_LOG_PORT" \
    -DLLAMA_BUILD_NUMBER="${llama_tag#b}" \
    "${opts[@]}" 2>&1 | tee "$out/configure.log"
if grep -q "Manually-specified variables were not used" "$out/configure.log"; then
    echo "build-llama: CMake ignored an option, see $out/configure.log" >&2
    exit 1
fi

# shellcheck disable=SC2068
cmake --build "$out" --target ${targets[@]}

# Every symbol a tool imports must be exported by one of the SDK stubs it
# names as NEEDED; anything else would only fail on the console. Weak imports
# (__cxa_thread_atexit_impl) may stay unresolved. The import list lands in
# build/llama-ps4/<tool>.imports (docs/PORTING.md).
llvm_bin="$("$LLVM_CONFIG" --bindir)"
for t in ${targets[@]}; do
    elf="$out/bin/$t.elf"
    [ -f "$elf" ] || continue
    exports=$(for lib in $("$llvm_bin/llvm-readelf" -d "$elf" | sed -n 's/.*Shared library: \[\(.*\)\.sprx\]/\1/p'); do
        "$llvm_bin/llvm-nm" -D --defined-only "$PS4_PAYLOAD_SDK/target/lib/$lib.so" | awk '{print $NF}'
    done | sort -u)
    "$llvm_bin/llvm-readelf" --dyn-syms "$elf" |
        awk '$7 == "UND" && $5 != "WEAK" && $8 != "" {print $8}' | sort -u > "$out/$t.imports"
    missing=$(comm -23 "$out/$t.imports" <(printf '%s\n' "$exports"))
    if [ -n "$missing" ]; then
        echo "build-llama: $t imports symbols no stub exports:" $missing >&2
        exit 1
    fi
    echo "build-llama: $t.elf links, $(wc -l < "$out/$t.imports" | tr -d ' ') imports, all exported"
done
