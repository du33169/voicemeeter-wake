# Use the MinGW-w64 UCRT toolchain provided by the Pixi/conda-forge environment.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

# Disable CMake's implicit compiler checks that can trip over the toolchain.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Runtime DLLs for UCRT toolchain live next to the compiler inside the env;
# let CMake locate headers/libs there first.
