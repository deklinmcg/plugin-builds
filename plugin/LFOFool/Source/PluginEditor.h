#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * LFOFool Plugin Editor — WebView UI
 *
 * CRITICAL: Member declaration order MUST be:
 *   1. Parameter relays  (destroyed last)
 *   2. WebBrowserComponent (destroyed middle)
 *   3. Parameter attachments (destroyed first)
 *
 * C++ destroys in REVERSE order — this prevents a DAW crash on plugin unload.
 */
class LFOFoolAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    LFOFoolAudioProcessorEditor (LFOFoolAudioProcessor&);
    ~LFOFoolAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized ()                override;

private:
    LFOFoolAudioProcessor& audioProcessor;

    // ═══════════════════════════════════════════════════════════════════════════
    // 1. PARAMETER RELAYS — destroyed last (no WebView dependencies)
    // ═══════════════════════════════════════════════════════════════════════════

    // LFO 1
    juce::WebSliderRelay       lfo1_rateRelay        { "lfo1_rate"         };
    juce::WebSliderRelay       lfo1_rateSyncRelay    { "lfo1_rate_sync"    };
    juce::WebToggleButtonRelay lfo1_syncEnabledRelay { "lfo1_sync_enabled" };
    juce::WebSliderRelay       lfo1_depthRelay       { "lfo1_depth"        };
    juce::WebSliderRelay       lfo1_shapeRelay       { "lfo1_shape"        };
    juce::WebSliderRelay       lfo1_phaseRelay       { "lfo1_phase"        };
    juce::WebSliderRelay       lfo1_gritRelay        { "lfo1_grit"         };
    juce::WebSliderRelay       lfo1_targetRelay      { "lfo1_target"       };
    juce::WebToggleButtonRelay lfo1_enabledRelay     { "lfo1_enabled"      };

    // LFO 2
    juce::WebSliderRelay       lfo2_rateRelay        { "lfo2_rate"         };
    juce::WebSliderRelay       lfo2_rateSyncRelay    { "lfo2_rate_sync"    };
    juce::WebToggleButtonRelay lfo2_syncEnabledRelay { "lfo2_sync_enabled" };
    juce::WebSliderRelay       lfo2_depthRelay       { "lfo2_depth"        };
    juce::WebSliderRelay       lfo2_shapeRelay       { "lfo2_shape"        };
    juce::WebSliderRelay       lfo2_phaseRelay       { "lfo2_phase"        };
    juce::WebSliderRelay       lfo2_gritRelay        { "lfo2_grit"         };
    juce::WebSliderRelay       lfo2_targetRelay      { "lfo2_target"       };
    juce::WebToggleButtonRelay lfo2_enabledRelay     { "lfo2_enabled"      };

    // LFO 3
    juce::WebSliderRelay       lfo3_rateRelay        { "lfo3_rate"         };
    juce::WebSliderRelay       lfo3_rateSyncRelay    { "lfo3_rate_sync"    };
    juce::WebToggleButtonRelay lfo3_syncEnabledRelay { "lfo3_sync_enabled" };
    juce::WebSliderRelay       lfo3_depthRelay       { "lfo3_depth"        };
    juce::WebSliderRelay       lfo3_shapeRelay       { "lfo3_shape"        };
    juce::WebSliderRelay       lfo3_phaseRelay       { "lfo3_phase"        };
    juce::WebSliderRelay       lfo3_gritRelay        { "lfo3_grit"         };
    juce::WebSliderRelay       lfo3_targetRelay      { "lfo3_target"       };
    juce::WebToggleButtonRelay lfo3_enabledRelay     { "lfo3_enabled"      };

    // LFO 4
    juce::WebSliderRelay       lfo4_rateRelay        { "lfo4_rate"         };
    juce::WebSliderRelay       lfo4_rateSyncRelay    { "lfo4_rate_sync"    };
    juce::WebToggleButtonRelay lfo4_syncEnabledRelay { "lfo4_sync_enabled" };
    juce::WebSliderRelay       lfo4_depthRelay       { "lfo4_depth"        };
    juce::WebSliderRelay       lfo4_shapeRelay       { "lfo4_shape"        };
    juce::WebSliderRelay       lfo4_phaseRelay       { "lfo4_phase"        };
    juce::WebSliderRelay       lfo4_gritRelay        { "lfo4_grit"         };
    juce::WebSliderRelay       lfo4_targetRelay      { "lfo4_target"       };
    juce::WebToggleButtonRelay lfo4_enabledRelay     { "lfo4_enabled"      };

    // Global
    juce::WebSliderRelay       chaosRelay            { "chaos"             };
    juce::WebSliderRelay       masterDepthRelay      { "master_depth"      };
    juce::WebToggleButtonRelay bpmSyncRelay          { "bpm_sync"          };
    juce::WebSliderRelay       filterCutoffRelay     { "filter_cutoff_base"};
    juce::WebSliderRelay       filterResRelay        { "filter_resonance"  };
    juce::WebSliderRelay       reverbBaseRelay       { "reverb_base"       };
    juce::WebSliderRelay       delayBaseRelay        { "delay_base"        };

    // ═══════════════════════════════════════════════════════════════════════════
    // 2. WEBVIEW — destroyed after attachments, before relays
    // ═══════════════════════════════════════════════════════════════════════════
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // ═══════════════════════════════════════════════════════════════════════════
    // 3. PARAMETER ATTACHMENTS — destroyed first
    // ═══════════════════════════════════════════════════════════════════════════

    // LFO 1
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo1_rateAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo1_rateSyncAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> lfo1_syncEnabledAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo1_depthAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo1_shapeAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo1_phaseAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo1_gritAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo1_targetAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> lfo1_enabledAtt;

    // LFO 2
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo2_rateAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo2_rateSyncAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> lfo2_syncEnabledAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo2_depthAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo2_shapeAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo2_phaseAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo2_gritAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo2_targetAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> lfo2_enabledAtt;

    // LFO 3
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo3_rateAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo3_rateSyncAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> lfo3_syncEnabledAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo3_depthAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo3_shapeAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo3_phaseAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo3_gritAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo3_targetAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> lfo3_enabledAtt;

    // LFO 4
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo4_rateAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo4_rateSyncAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> lfo4_syncEnabledAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo4_depthAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo4_shapeAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo4_phaseAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo4_gritAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lfo4_targetAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> lfo4_enabledAtt;

    // Global
    std::unique_ptr<juce::WebSliderParameterAttachment>       chaosAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       masterDepthAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bpmSyncAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       filterCutoffAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       filterResAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       reverbBaseAtt;
    std::unique_ptr<juce::WebSliderParameterAttachment>       delayBaseAtt;

    // Resource provider
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    std::unique_ptr<juce::ZipFile> getZipFile();

    static const char*  getMimeForExtension (const juce::String& extension);
    static juce::String getExtension        (juce::String filename);
    static auto         streamToVector      (juce::InputStream& stream);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LFOFoolAudioProcessorEditor)
};
