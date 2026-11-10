# Ubuntu-hosted clang-cl cross-compile targeting the Windows x64 MSVC ABI.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Let vcpkg infer VCPKG_TARGET_IS_WINDOWS from this triplet name. The chainloaded
# toolchain provides the Windows target and xwin SDK/CRT sysroot.
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../toolchain-linux-clangcl.cmake")
