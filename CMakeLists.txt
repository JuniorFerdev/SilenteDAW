cmake_minimum_required(VERSION 3.22)
project(Silente VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)
# LAME 3.100 MP3 encoder (LGPL), built from source as a static library for MP3 export.
file(GLOB SILENTE_LAME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lame-3.100/libmp3lame/*.c")
add_library(silente_lame STATIC ${SILENTE_LAME_SOURCES})
target_include_directories(silente_lame PRIVATE third_party/lame-config third_party/lame-3.100/libmp3lame PUBLIC third_party/lame-3.100/include)
target_compile_definitions(silente_lame PRIVATE HAVE_CONFIG_H)
target_compile_options(silente_lame PRIVATE -w)
juce_add_binary_data(SilenteLocales HEADER_NAME LocalisationData.h NAMESPACE SilenteLocales
    SOURCES resources/locales/es.json resources/locales/en.json)
juce_add_gui_app(Silente
    PRODUCT_NAME "Silente"
    COMPANY_NAME "Silente Audio"
    BUNDLE_ID "audio.silente.studio"
    VERSION "${PROJECT_VERSION}"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Silente usa el micrófono o la interfaz de audio para grabar las pistas que armes."
    DOCUMENT_EXTENSIONS silente
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS com.apple.security.cs.disable-library-validation com.apple.security.device.audio-input
)
target_sources(Silente PRIVATE src/Main.cpp src/AudioEngine.cpp src/MainComponent.cpp src/PluginManager.cpp tests/EngineTests.cpp tests/LocalisationUiTests.cpp tests/WorkflowUiTests.cpp tests/DeviceTests.cpp tests/IoTests.cpp
    src/ui/theme/Drawing.cpp src/ui/theme/SilenteLookAndFeel.cpp
    src/ui/theme/ToolIcons.cpp src/ui/theme/TrackColourPicker.cpp
    src/i18n/Translations.cpp src/i18n/UiTranslation.cpp
    src/ui/plugins/PluginSelector.cpp src/audio/DeviceSettings.cpp src/ui/audio/AudioSettingsPanel.cpp
    src/audio/Metronome.cpp src/audio/RecordingModes.cpp src/audio/EventEdit.cpp src/audio/RangeEdit.cpp src/ui/metronome/MetronomePanel.cpp src/ui/metronome/TapTempoPanel.cpp src/ui/mixconsole/MixConsole.cpp src/ui/project/ChannelTab.cpp src/ui/project/FadeEditor.cpp src/ui/plugins/PluginEditorWindow.cpp src/app/KeyCommands.cpp src/ui/keys/KeyCommandsPanel.cpp
    src/io/Mp3Encoder.cpp src/io/AudioFileFormats.cpp src/io/VideoAudio.mm src/diagnostics/NativeWindows.mm src/io/TrackExport.cpp
    src/ui/io/FormatSelector.cpp src/ui/io/ExportDialog.cpp src/ui/io/ConversionPanel.cpp
    src/app/Preferences.cpp src/ui/settings/SettingsWorkspace.cpp tests/SettingsUiTests.cpp
    src/ui/project/ProjectToolbox.cpp tests/ToolboxUiTests.cpp
    src/domain/TrackData.cpp src/persistence/TrackDataJson.cpp
    src/ui/project/TimelineTracks.cpp src/ui/project/MarkerInspector.cpp tests/MarkerUiTests.cpp src/ui/plugins/InsertRack.cpp
    src/app/TrackCreation.cpp src/ui/project/AddTrackDialog.cpp tests/AddTrackUiTests.cpp
    src/ui/midi/PianoRoll.cpp)
target_compile_definitions(Silente PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Silente,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_VST=0
    JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)
target_link_libraries(Silente PRIVATE SilenteLocales silente_lame "-framework AVFoundation" "-framework CoreMedia" juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/Silente.icns")
    set_source_files_properties(assets/Silente.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(Silente PRIVATE assets/Silente.icns)
    set_target_properties(Silente PROPERTIES MACOSX_BUNDLE_ICON_FILE Silente.icns)
endif()
enable_testing()
add_test(NAME SilenteEngineTests COMMAND Silente --self-test "${CMAKE_CURRENT_BINARY_DIR}/test-results")

foreach(locale es en)
    set_source_files_properties(resources/macos/${locale}.lproj/InfoPlist.strings PROPERTIES MACOSX_PACKAGE_LOCATION Resources/${locale}.lproj)
    target_sources(Silente PRIVATE resources/macos/${locale}.lproj/InfoPlist.strings)
endforeach()
