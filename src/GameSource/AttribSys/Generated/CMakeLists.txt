target_sources(${PROJECT_NAME}
    PRIVATE
        codegen.cpp

    PUBLIC
        attrib_findcollection.h
        attrib_private.h
)

add_subdirectory(classes)
