# Standalone POSIX protocol tests: cmake -S tests/sync2 -B build/sync2-tests
# Then: cmake --build build/sync2-tests && ctest --test-dir build/sync2-tests
cmake_minimum_required(VERSION 3.25)
project(sync2_entertainment_tests LANGUAGES CXX)
enable_testing()
find_package(Threads REQUIRED)
get_filename_component(core ../.. ABSOLUTE)
set(board "${core}/src/boards/ford_sync_gen2")
add_executable(sync2_entertainment_test
    entertainment_test.cpp support.cpp
    "${board}/ford_sync_gen2_console.cpp"
    "${board}/ford_sync_gen2_entertainment.cpp"
    "${board}/ford_sync_gen2_media_status.cpp"
    "${board}/ford_sync_gen2_ilp_channel.cpp"
    "${board}/ford_sync_gen2_ilp_signals.cpp"
    "${core}/src/state/state_stream.cpp"
    "${core}/src/platform/host_file_posix.cpp"
    "${core}/src/platform/host_error_posix.cpp"
    "${core}/src/platform/host_path_open_posix.cpp"
    "${core}/src/platform/host_local_path_open_posix.cpp")
target_compile_features(sync2_entertainment_test PRIVATE cxx_std_20)
target_compile_options(sync2_entertainment_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(sync2_entertainment_test PRIVATE
    "${core}/include" "${core}/libCerfEngine/include"
    "${core}/libCerfEngine/libservicelocator/include")
target_link_libraries(sync2_entertainment_test PRIVATE Threads::Threads)
add_test(NAME sync2_entertainment COMMAND sync2_entertainment_test)
add_executable(imx51_ipu_blend_test ../imx51_ipu_blend_test.cpp)
target_compile_features(imx51_ipu_blend_test PRIVATE cxx_std_20)
target_compile_options(imx51_ipu_blend_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(imx51_ipu_blend_test PRIVATE "${core}/include" "${core}/libCerfEngine/include")
add_test(NAME imx51_ipu_blend COMMAND imx51_ipu_blend_test)
add_executable(audio_pacing_test audio_pacing_test.cpp
    "${core}/src/host/paced_wave_out.cpp" "${core}/src/host/dynamic_rate_resampler.cpp"
    "${core}/src/platform/wave_pcm_format.cpp")
target_compile_features(audio_pacing_test PRIVATE cxx_std_20)
target_compile_options(audio_pacing_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(audio_pacing_test PRIVATE "${core}/include"
    "${core}/libCerfEngine/include" "${core}/libCerfEngine/libservicelocator/include")
target_link_libraries(audio_pacing_test PRIVATE Threads::Threads)
add_test(NAME audio_pacing COMMAND audio_pacing_test)
