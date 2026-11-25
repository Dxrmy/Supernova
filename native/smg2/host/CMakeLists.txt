# Standalone headless Nebula host (nebula_native_host.exe) built with MSVC
# to load build/smg2_game/smg_game.dll and build/smg2_content/game.pak (SB4E01).
cmake_minimum_required(VERSION 3.24)
project(NebulaSmg2Host LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

get_filename_component(nebula_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(NEBULA_ARCHIVE_SOURCES "${nebula_root}/third_party/archive" CACHE PATH "zlib, xz and libarchive sources")

include("${nebula_root}/cmake/archive_deps.cmake")
include("${nebula_root}/third_party/boringssl.cmake")
include("${nebula_root}/third_party/zlib_inflate.cmake")

add_executable(nebula_native_host
    "${nebula_root}/native/host/host_main.cpp"
    "${nebula_root}/native/host/luma_guard.cpp"
    "${nebula_root}/native/host/crash_watch.cpp"
    "${nebula_root}/runtime/src/whole_lines.cpp"
    "${nebula_root}/runtime/src/host_extension.cpp"
    "${nebula_root}/runtime/src/luma_services_host.cpp"
    "${nebula_root}/runtime/src/luma_system.cpp"
    "${nebula_root}/runtime/src/luma_mods.cpp"
    "${nebula_root}/runtime/src/luma_fs.cpp"
    "${nebula_root}/runtime/src/luma_net.cpp"
    "${nebula_root}/runtime/src/luma_net_reach.cpp"
    "${nebula_root}/runtime/src/luma_assets.cpp"
    "${nebula_root}/runtime/src/luma_audio.cpp"
    "${nebula_root}/runtime/src/luma_capture.cpp"
    "${nebula_root}/runtime/src/luma_input.cpp"
    "${nebula_root}/runtime/src/input/mod_actions.cpp"
    "${nebula_root}/runtime/src/input_bindings.cpp"
    "${nebula_root}/runtime/src/mod_log.cpp"
    "${nebula_root}/runtime/src/mod_check.cpp"
    "${nebula_root}/runtime/src/luma_game_store.cpp"
    "${nebula_root}/runtime/src/mod_screens.cpp"
    "${nebula_root}/runtime/src/mod_start_check.cpp"
    "${nebula_root}/runtime/src/mod_plugins.cpp"
    "${nebula_root}/runtime/src/game_module.cpp"
    "${nebula_root}/runtime/src/luma.cpp"
    "${nebula_root}/runtime/src/mods.cpp"
    "${nebula_root}/runtime/src/mod_consent.cpp"
    "${nebula_root}/runtime/src/texture_categories.cpp"
    "${nebula_root}/runtime/src/dolphin_pack.cpp"
    "${nebula_root}/runtime/src/mod_xml.cpp"
    "${nebula_root}/runtime/src/atomic_file.cpp"
    "${nebula_root}/runtime/src/save_files.cpp"
    "${nebula_root}/runtime/src/asset_store.cpp"
    "${nebula_root}/runtime/src/crash_report.cpp"
    "${nebula_root}/runtime/src/discord_presence.cpp"
    "${nebula_root}/runtime/src/mii_import.cpp"
    "${nebula_root}/runtime/src/mii_formats.cpp"
    "${nebula_root}/runtime/src/pointer_control.cpp"
    $<$<PLATFORM_ID:Windows>:${nebula_root}/runtime/src/render/mod_shader_compile_d3d12.cpp>
)

target_include_directories(nebula_native_host PRIVATE
    "${nebula_root}/native/include"
    "${nebula_root}/runtime/include"
)

target_compile_definitions(nebula_native_host PRIVATE
    NEBULA_VERSION="2.0.0"
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(nebula_native_host PRIVATE
    nebula_bssl_ssl
    nebula_zinflate
    nebula_archive
    $<$<PLATFORM_ID:Windows>:shell32;ole32;bcrypt;winmm;dbghelp;ws2_32>
)
