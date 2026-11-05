cmake_minimum_required(VERSION 3.25)
project(usb_hotplug_tests LANGUAGES CXX)
enable_testing()
find_package(Threads REQUIRED)
get_filename_component(core ../.. ABSOLUTE)

function(usb_test name)
    add_executable(${name} ${name}.cpp support.cpp ${ARGN}
        "${core}/src/peripherals/usb/usb_device.cpp"
        "${core}/src/peripherals/usb/usb_hub.cpp"
        "${core}/src/peripherals/usb/usb_host_port.cpp"
        "${core}/src/state/state_stream.cpp"
        "${core}/src/platform/host_file_posix.cpp"
        "${core}/src/platform/host_error_posix.cpp"
        "${core}/src/platform/host_path_open_posix.cpp"
        "${core}/src/platform/host_local_path_open_posix.cpp")
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
    target_include_directories(${name} PRIVATE "${core}/include"
        "${core}/libCerfEngine/include" "${core}/libCerfEngine/libservicelocator/include")
    target_link_libraries(${name} PRIVATE Threads::Threads)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

usb_test(usb_hub_hotplug_test)
usb_test(imx51_usb_resume_test usb_resume_clock.cpp
    "${core}/src/peripherals/peripheral_base.cpp"
    "${core}/src/core/tick_scale.cpp"
    "${core}/src/socs/cycle_anchored_counter.cpp"
    "${core}/src/socs/imx51/imx51_usboh3.cpp"
    "${core}/src/socs/imx51/imx51_usboh3_host.cpp"
    "${core}/src/socs/freescale_usb_frame_index.cpp")
target_compile_options(imx51_usb_resume_test PRIVATE -ffunction-sections -fdata-sections)
if(APPLE)
    target_link_options(imx51_usb_resume_test PRIVATE -Wl,-dead_strip)
else()
    target_link_options(imx51_usb_resume_test PRIVATE -Wl,--gc-sections)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json CONFIG REQUIRED)
endif()
target_link_libraries(imx51_usb_resume_test PRIVATE nlohmann_json::nlohmann_json)
