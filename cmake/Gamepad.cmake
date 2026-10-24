# Call in the directory that creates the executable (CMake's POST_BUILD rule).
function(brn_configure_gamepad target)
    find_package(SDL3 3.4.18 CONFIG REQUIRED COMPONENTS SDL3-shared
        HINTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../vendor/sdl3/cmake")
    get_filename_component(sdl3_package_root "${SDL3_DIR}/.." ABSOLUTE)
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl3_package_root}/LICENSE.txt"
            "$<TARGET_FILE_DIR:${target}>/SDL3-LICENSE.txt")
endfunction()
