add_subdirectory(Enums)
add_subdirectory(Generated)

# Original embedded X360 bytes; the selected native LE schema loader is separate.
target_sources(${PROJECT_NAME} PRIVATE
    Schema/X360/BrnAttribSchemaBinX360.cpp
    Schema/X360/BrnAttribSchemaVltX360.cpp
)
