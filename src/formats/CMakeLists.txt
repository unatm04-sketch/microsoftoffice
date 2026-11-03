qt_add_library(omni_formats STATIC
    Docx.h
    DocxReader.cpp
    DocxWriter.cpp
    Ooxml.cpp
    Ooxml.h
    PageSetup.h
    TextCodec.cpp
    TextCodec.h
    Zip.cpp
    Zip.h
)

target_include_directories(omni_formats
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(omni_formats
    PUBLIC
        Qt6::Gui
    PRIVATE
        omni_build_options
)
