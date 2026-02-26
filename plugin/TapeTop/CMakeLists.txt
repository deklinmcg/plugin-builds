juce_add_plugin(TapeTop
    COMPANY_NAME "APC"
    PLUGIN_MANUFACTURER_CODE Apcc
    PLUGIN_CODE Tape
    FORMATS VST3 AU
    PRODUCT_NAME "TapeTop"
    VST3_CATEGORIES Fx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

juce_generate_juce_header(TapeTop)

target_sources(TapeTop PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(TapeTop PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(TapeTop PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
