target_sources(${PROJECT_NAME}
    PRIVATE
        BrnPostEventStates_embed_check.cpp

    PUBLIC

)

add_subdirectory(Offline)
add_subdirectory(Online)
add_subdirectory(Showtime)
