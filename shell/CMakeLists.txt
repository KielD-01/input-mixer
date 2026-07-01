if(APPLE)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/macos/InfoPlistAdditions.plist" _input_mixer_plist_merge)
else()
    set(_input_mixer_plist_merge "")
endif()

juce_add_gui_app(InputMixer
    PRODUCT_NAME "Input Mixer"
    COMPANY_NAME "InputMixer"
    BUNDLE_ID "com.inputmixer.app"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/AppIcon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Input Mixer needs microphone access to capture hardware audio sources."
    CAMERA_PERMISSION_ENABLED FALSE
    PLIST_TO_MERGE "${_input_mixer_plist_merge}"
)

target_sources(InputMixer
    PRIVATE
        Main.cpp
        app/InputMixerApplication.cpp
        app/MainWindow.cpp
        app/FirstRunWizard.cpp
        app/DriverInstaller.cpp
        app/SettingsStore.cpp
        bridge/RustBridge.cpp
        bridge/SourceMatching.cpp
        ui/DesignModePanel.cpp
        ui/LevelMeterComponent.cpp
        ui/MixerLookAndFeel.cpp
        ui/MixerTheme.cpp
        ui/FontAudioIcons.cpp
        ui/ChannelStripComponent.cpp
        ui/MasterStripComponent.cpp
        ui/MonitorStripComponent.cpp
)

target_include_directories(InputMixer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/target/cxxbridge
        ${CMAKE_SOURCE_DIR}/target/cxxbridge/input-mixer-core/src
)

target_compile_definitions(InputMixer
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:InputMixer,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(InputMixer
    PRIVATE
        input_mixer_core
        fontaudio
        juce::juce_gui_extra
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

collect_rust_native_libs(RUST_EXTRA_LIBS ${RUST_PROFILE})
if(RUST_EXTRA_LIBS)
    target_link_libraries(InputMixer PRIVATE ${RUST_EXTRA_LIBS})
endif()

if(APPLE)
    target_link_libraries(InputMixer PRIVATE
        "-framework Foundation"
        "-framework CoreAudio"
        "-framework CoreMedia"
        "-framework ScreenCaptureKit"
        "-framework AVFoundation"
        "-framework Security"
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(InputMixer PRIVATE pthread dl m)
endif()

juce_generate_juce_header(InputMixer)

