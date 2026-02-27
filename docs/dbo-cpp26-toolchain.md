# Wt::Dbo C++26 Reflection Toolchain Notes

Date: 2026-02-27  
Status: In progress

## Scope

`Wt::Dbo` runs in hard-cut reflection mode only:

- `WT_DBO_CPP26_HARD_CUT=ON` is required.
- `wtdbo` is compiled as C++26 (`cxx_std_26`).
- CMake configure performs a fail-fast probe for:
  - `__has_include(<meta>)`
  - reflection syntax (`^^int`)

## CMake Flags

Minimal configure flags for Dbo hard-cut validation:

```bash
cmake -S . -B build-dbo-reflection -G Ninja \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_LIBWTDBO=ON \
  -DWT_DBO_CPP26_HARD_CUT=ON \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_TESTS=OFF
cmake --build build-dbo-reflection --target wtdbo -j"$(nproc)"
```

## CI Matrix (Lot 1)

1. `jenkins/db.Jenkinsfile`
- Runs `tools/ci/check_wtdbo_throw_catch_budget.sh` to prevent new `throw/catch` debt in maintained Dbo code.

2. `jenkins/dbo-cpp26-reflection.Jenkinsfile`
- Dedicated reflection lane on Jenkins label `wt-cpp26-reflection`.
- Must pass configure/build with `WT_DBO_CPP26_HARD_CUT=ON`.

## Fail-Fast Baseline On Non-Reflection Compilers

Local probe results on 2026-02-27:

1. `clang++ 21.1.2 (2ubuntu6)` -> no `<meta>` header -> expected configure failure.
2. `g++ 15.2.0-4ubuntu4` -> no `<meta>` header -> expected configure failure.

These negative checks validate that Dbo now fails early and explicitly when reflection support is absent.
