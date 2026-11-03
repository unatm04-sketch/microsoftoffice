qt_add_library(omni_writer STATIC
    DocumentPrinter.cpp
    DocumentPrinter.h
    PageView.cpp
    PageView.h
    WriterDialogs.cpp
    WriterDialogs.h
    WriterEditor.cpp
    WriterEditor.h
    WriterModule.cpp
    WriterModule.h
    WriterTemplates.cpp
    WriterTemplates.h
)

target_link_libraries(omni_writer
    PUBLIC
        omni_core
    PRIVATE
        omni_formats
        omni_build_options
        Qt6::PrintSupport
)
