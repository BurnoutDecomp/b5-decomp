# SDL3 is built from the vendor/SDL submodule (BurnoutDecomp/SDL: release 3.4.18
# plus the Steam Controller haptics backport), never from a system package.
# Call in the directory that creates the executable (CMake's POST_BUILD rule).
function(brn_configure_gamepad target)
    set(sdl_source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../vendor/SDL")
    if (NOT EXISTS "${sdl_source}/CMakeLists.txt")
        message(FATAL_ERROR "SDL submodule missing: git submodule update --init vendor/SDL")
    endif()
    if (NOT TARGET SDL3::SDL3-shared)
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory("${sdl_source}" "${CMAKE_BINARY_DIR}/vendor/SDL" EXCLUDE_FROM_ALL)
    endif()
    target_link_libraries(${target} PRIVATE SDL3::SDL3-shared)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl_source}/LICENSE.txt"
            "$<TARGET_FILE_DIR:${target}>/SDL3-LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../docs/licenses/DualSenseTriggerEffects.txt"
            "$<TARGET_FILE_DIR:${target}>/DualSenseTriggerEffects-LICENSE.txt")
endfunction()
