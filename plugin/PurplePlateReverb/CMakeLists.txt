juce_add_plugin(PurplePlateReverb
    COMPANY_NAME "APC"
    PLUGIN_MANUFACTURER_CODE Apcc
    PLUGIN_CODE PpRv
    FORMATS VST3 AU
    PRODUCT_NAME "PurplePlateReverb"
    VST3_CATEGORIES Fx Reverb
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

juce_generate_juce_header(PurplePlateReverb)

target_sources(PurplePlateReverb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(PurplePlateReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(PurplePlateReverb PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
