cmake_minimum_required(VERSION 3.22.1)
project(TONE3000 VERSION ${T3K_VERSION})
# This fork is a VST3 effect only. Audio/MIDI devices belong to the host.
# Keep the CMake target name for development scripts; use a distinct plugin ID
# and product name so the fork can coexist with the original TONE3000.
add_compile_definitions(HEADLESS=0)
set(NAM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/NeuralAmpModelerCore/NAM")
add_library(NAM STATIC
    ${NAM_SRC}/activations.cpp
    ${NAM_SRC}/conv1d.cpp
    ${NAM_SRC}/container.cpp
    ${NAM_SRC}/convnet.cpp
    ${NAM_SRC}/dsp.cpp
    ${NAM_SRC}/get_dsp.cpp
    ${NAM_SRC}/lstm.cpp
    ${NAM_SRC}/ring_buffer.cpp
    ${NAM_SRC}/util.cpp
    ${NAM_SRC}/wavenet/a2_fast.cpp
    ${NAM_SRC}/wavenet/model.cpp
    ${NAM_SRC}/wavenet/slimmable.cpp
)
target_include_directories(NAM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/NeuralAmpModelerCore
    ${CMAKE_CURRENT_SOURCE_DIR}/NeuralAmpModelerCore/Dependencies/eigen
    ${CMAKE_CURRENT_SOURCE_DIR}/NeuralAmpModelerCore/Dependencies/nlohmann
)
set_target_properties(NAM PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
# Hidden visibility, matching what JUCE sets on the plugin targets (NAM is a
# separate target and gets none of that). Left visible, GCC exports the
# header-inline ConfigParserRegistry singleton as STB_GNU_UNIQUE, which glibc
# resolves process-wide even across RTLD_LOCAL dlopens: a host with two
# formats installed (LV2 and VST3, say) shares one registry between both
# binaries, each still runs its own static parser registrations, and the
# duplicate-name throw escapes dlopen and aborts the host (issue #59).
set_target_properties(NAM PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN TRUE)
# Second guard for the same bug (GCC-only flag; Clang never emits unique
# symbols). It also keeps the .so dlclose'able: unique symbols pin a loaded
# library NODELETE.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(NAM PRIVATE -fno-gnu-unique)
endif()
# A2 full / A2 lite models use the hand-optimized path when config matches (see NeuralAmpModelerCore).
target_compile_definitions(NAM PRIVATE NAM_ENABLE_A2_FAST)


juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "Local Rig"
    BUNDLE_ID "local.rig.vst3"
    PLUGIN_MANUFACTURER_CODE Lrig
    PLUGIN_CODE Lr01
    PRODUCT_NAME "Local Rig"
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Distortion Delay Reverb Modulation
    VST3_AUTO_MANIFEST TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)
target_sources(${PROJECT_NAME}
    PRIVATE
        include/BlockEq.h
        src/BlockEq.cpp
        include/BlockSpectrum.h
        src/BlockSpectrum.cpp
        include/ChainBlock.h
        include/ChainHistory.h
        include/ImageDeck.h
        include/Spread.h
        src/Spread.cpp
        include/StereoOffset.h
        src/StereoOffset.cpp
        include/AutoOffset.h
        src/AutoOffset.cpp
        include/LegacyParamIds.h
        src/LegacyParamIds.cpp
        include/PresetFile.h
        src/PresetFile.cpp
        include/PresetManager.h
        src/PresetManager.cpp
        include/Processor.h
        src/Processor.cpp
        include/MidiMapper.h
        src/MidiMapper.cpp
        src/ProcessorChain.cpp
        src/ProcessorHistory.cpp
        src/ProcessorPresets.cpp
        src/ProcessorState.cpp
        src/ProcessorModelLoader.cpp
        include/ChainDomain.h
        include/NamEngine.h
        src/NamEngine.cpp
        include/TunerDetector.h
        src/TunerDetector.cpp
        include/PitchShift.h
        src/PitchShift.cpp
        include/OfflineEffects.h
        src/OfflineEffects.cpp
        include/OfflineEditor.h
        src/OfflineEditor.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/NeuralAmpModelerCore
        ${CMAKE_CURRENT_SOURCE_DIR}/NeuralAmpModelerCore/Dependencies/eigen
        ${CMAKE_CURRENT_SOURCE_DIR}/NeuralAmpModelerCore/Dependencies/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}/AudioDSPTools
        ${CMAKE_CURRENT_SOURCE_DIR}/AudioDSPTools/dsp/ResamplingContainer/Dependencies
)

# The define also gates the is_a2_shape declaration, which the plugin uses to
# restrict drop-loaded local NAM files to A2 (see ProcessorModelLoader.cpp).
# Defined here too because NAM is linked as a whole-archive flag on
# macOS/Linux, which doesn't propagate the NAM target's definitions.
target_compile_definitions(${PROJECT_NAME} PRIVATE NAM_ENABLE_A2_FAST)

# JUCE modules used
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
        juce::juce_audio_processors
        juce::juce_gui_basics
        # NAM: force_load/whole-archive so the static ConfigParserHelper
        # registrations are not stripped by the linker
        # PLATFORM_ID is "iOS" (not "Darwin") when cross-compiling for iOS, so
        # both have to be listed or the iOS link drops every model architecture
        # and NAM loads fail with "No config parser registered for ...".
        $<$<PLATFORM_ID:Darwin,iOS>:-Wl,-force_load,$<TARGET_FILE:NAM>>
        # Linux: flags and library must be separate link items. A single
        # "-Wl,--whole-archive,NAM,-Wl,..." item gets comma-split by the GCC
        # driver, passing a literal "-Wl" to ld ("unrecognized option '-Wl'").
        $<$<PLATFORM_ID:Linux>:-Wl,--whole-archive>
        $<$<PLATFORM_ID:Linux>:NAM>
        $<$<PLATFORM_ID:Linux>:-Wl,--no-whole-archive>
        # Windows (MSVC): /WHOLEARCHIVE is the equivalent; without it the
        # linker drops the objects holding the self-registering config parsers
        # ("No config parser registered for architecture ..." at NAM load).
        # Leading '-' so CMake treats the item as a flag (a leading '/' parses
        # as a path); MSVC's linker accepts either prefix.
        $<$<PLATFORM_ID:Windows>:NAM>
        $<$<PLATFORM_ID:Windows>:-WHOLEARCHIVE:$<TARGET_FILE:NAM>>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Ensure NAM is built before plugin (force_load path requires lib to exist)
add_dependencies(${PROJECT_NAME} NAM)


target_compile_definitions(${PROJECT_NAME} PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_WEBP=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
)
