qt_add_library(omni_core STATIC
    ActionManager.cpp
    ActionManager.h
    AppInfo.h
    AppSettings.cpp
    AppSettings.h
    DocumentEditor.cpp
    DocumentEditor.h
    Icons.cpp
    Icons.h
    Module.h
    ModuleRegistry.cpp
    ModuleRegistry.h
    SettingsPage.h
    TextOps.cpp
    TextOps.h
    ThemeManager.cpp
    ThemeManager.h
    Utils.cpp
    Utils.h
    widgets/ColorButton.cpp
    widgets/ColorButton.h
    widgets/FindReplaceBar.cpp
    widgets/FindReplaceBar.h
    widgets/Ribbon.cpp
    widgets/Ribbon.h
    widgets/ZoomControl.cpp
    widgets/ZoomControl.h
)

target_include_directories(omni_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(omni_core
    PUBLIC
        Qt6::Widgets
    PRIVATE
        Qt6::Svg
        omni_build_options
        $<$<PLATFORM_ID:Windows>:dwmapi>
)

qt_add_resources(omni_core "omni_core_resources"
    PREFIX "/omni"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        "${PROJECT_SOURCE_DIR}/resources/ui/arrow-down-dark.svg"
        "${PROJECT_SOURCE_DIR}/resources/ui/arrow-down-light.svg"
)
