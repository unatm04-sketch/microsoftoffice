set(app_icon_windows "${PROJECT_SOURCE_DIR}/resources/app/omnioffice.rc")

qt_add_executable(OmniOffice WIN32
    Automation.cpp
    Automation.h
    Backstage.cpp
    Backstage.h
    CommandSearch.cpp
    CommandSearch.h
    MainWindow.cpp
    MainWindow.h
    NavigationRail.cpp
    NavigationRail.h
    SettingsDialog.cpp
    SettingsDialog.h
    SingleInstance.cpp
    SingleInstance.h
    StartPage.cpp
    StartPage.h
    main.cpp
    $<$<PLATFORM_ID:Windows>:${app_icon_windows}>
)

target_link_libraries(OmniOffice
    PRIVATE
        omni_build_options
        omni_core
        omni_writer
        Qt6::Network
        Qt6::PrintSupport
        Qt6::Widgets
)

set_target_properties(OmniOffice PROPERTIES
    OUTPUT_NAME "OmniOffice"
)

# Copy the Qt runtime next to the executable so it can be started from the build tree.
if(WIN32 AND OMNI_DEPLOY_ON_BUILD)
    find_program(OMNI_WINDEPLOYQT windeployqt HINTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
    if(OMNI_WINDEPLOYQT)
        # Translations are copied directly: windeployqt would need lconvert (Qt Tools) to merge them.
        set(qt_translations_dir "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}")
        add_custom_command(TARGET OmniOffice POST_BUILD
            COMMAND "${OMNI_WINDEPLOYQT}"
                --no-compiler-runtime
                --no-system-d3d-compiler
                --no-opengl-sw
                --no-translations
                --verbose 0
                "$<TARGET_FILE:OmniOffice>"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:OmniOffice>/translations"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${qt_translations_dir}/qtbase_tr.qm"
                "$<TARGET_FILE_DIR:OmniOffice>/translations/qtbase_tr.qm"
            COMMENT "Qt calisma zamani kopyalaniyor (windeployqt)"
            VERBATIM
        )
    endif()
endif()

install(TARGETS OmniOffice
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET OmniOffice
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
