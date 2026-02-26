#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
LFOFoolAudioProcessorEditor::LFOFoolAudioProcessorEditor (LFOFoolAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (800, 500);

    // Build WebBrowserComponent — relays already declared in header
    webView = std::make_unique<juce::WebBrowserComponent> (
        juce::WebBrowserComponent::Options()
           #if JUCE_WINDOWS
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder (juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)))
           #endif
            .withNativeIntegrationEnabled()
            // LFO 1 relays
            .withOptionsFrom (lfo1_rateRelay)
            .withOptionsFrom (lfo1_rateSyncRelay)
            .withOptionsFrom (lfo1_syncEnabledRelay)
            .withOptionsFrom (lfo1_depthRelay)
            .withOptionsFrom (lfo1_shapeRelay)
            .withOptionsFrom (lfo1_phaseRelay)
            .withOptionsFrom (lfo1_gritRelay)
            .withOptionsFrom (lfo1_targetRelay)
            .withOptionsFrom (lfo1_enabledRelay)
            // LFO 2 relays
            .withOptionsFrom (lfo2_rateRelay)
            .withOptionsFrom (lfo2_rateSyncRelay)
            .withOptionsFrom (lfo2_syncEnabledRelay)
            .withOptionsFrom (lfo2_depthRelay)
            .withOptionsFrom (lfo2_shapeRelay)
            .withOptionsFrom (lfo2_phaseRelay)
            .withOptionsFrom (lfo2_gritRelay)
            .withOptionsFrom (lfo2_targetRelay)
            .withOptionsFrom (lfo2_enabledRelay)
            // LFO 3 relays
            .withOptionsFrom (lfo3_rateRelay)
            .withOptionsFrom (lfo3_rateSyncRelay)
            .withOptionsFrom (lfo3_syncEnabledRelay)
            .withOptionsFrom (lfo3_depthRelay)
            .withOptionsFrom (lfo3_shapeRelay)
            .withOptionsFrom (lfo3_phaseRelay)
            .withOptionsFrom (lfo3_gritRelay)
            .withOptionsFrom (lfo3_targetRelay)
            .withOptionsFrom (lfo3_enabledRelay)
            // LFO 4 relays
            .withOptionsFrom (lfo4_rateRelay)
            .withOptionsFrom (lfo4_rateSyncRelay)
            .withOptionsFrom (lfo4_syncEnabledRelay)
            .withOptionsFrom (lfo4_depthRelay)
            .withOptionsFrom (lfo4_shapeRelay)
            .withOptionsFrom (lfo4_phaseRelay)
            .withOptionsFrom (lfo4_gritRelay)
            .withOptionsFrom (lfo4_targetRelay)
            .withOptionsFrom (lfo4_enabledRelay)
            // Global relays
            .withOptionsFrom (chaosRelay)
            .withOptionsFrom (masterDepthRelay)
            .withOptionsFrom (bpmSyncRelay)
            .withOptionsFrom (filterCutoffRelay)
            .withOptionsFrom (filterResRelay)
            .withOptionsFrom (reverbBaseRelay)
            .withOptionsFrom (delayBaseRelay)
            .withResourceProvider ([this] (const auto& url) { return getResource (url); })
    );

    addAndMakeVisible (*webView);

    // ── Create parameter attachments AFTER WebBrowserComponent ────────────────
    auto& apvts = audioProcessor.apvts;

    // LFO 1
    lfo1_rateAtt        = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo1_rate"),         lfo1_rateRelay,        nullptr);
    lfo1_rateSyncAtt    = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo1_rate_sync"),     lfo1_rateSyncRelay,    nullptr);
    lfo1_syncEnabledAtt = std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter ("lfo1_sync_enabled"),  lfo1_syncEnabledRelay, nullptr);
    lfo1_depthAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo1_depth"),         lfo1_depthRelay,       nullptr);
    lfo1_shapeAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo1_shape"),         lfo1_shapeRelay,       nullptr);
    lfo1_phaseAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo1_phase"),         lfo1_phaseRelay,       nullptr);
    lfo1_gritAtt        = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo1_grit"),          lfo1_gritRelay,        nullptr);
    lfo1_targetAtt      = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo1_target"),        lfo1_targetRelay,      nullptr);
    lfo1_enabledAtt     = std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter ("lfo1_enabled"),       lfo1_enabledRelay,     nullptr);

    // LFO 2
    lfo2_rateAtt        = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo2_rate"),         lfo2_rateRelay,        nullptr);
    lfo2_rateSyncAtt    = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo2_rate_sync"),     lfo2_rateSyncRelay,    nullptr);
    lfo2_syncEnabledAtt = std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter ("lfo2_sync_enabled"),  lfo2_syncEnabledRelay, nullptr);
    lfo2_depthAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo2_depth"),         lfo2_depthRelay,       nullptr);
    lfo2_shapeAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo2_shape"),         lfo2_shapeRelay,       nullptr);
    lfo2_phaseAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo2_phase"),         lfo2_phaseRelay,       nullptr);
    lfo2_gritAtt        = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo2_grit"),          lfo2_gritRelay,        nullptr);
    lfo2_targetAtt      = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo2_target"),        lfo2_targetRelay,      nullptr);
    lfo2_enabledAtt     = std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter ("lfo2_enabled"),       lfo2_enabledRelay,     nullptr);

    // LFO 3
    lfo3_rateAtt        = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo3_rate"),         lfo3_rateRelay,        nullptr);
    lfo3_rateSyncAtt    = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo3_rate_sync"),     lfo3_rateSyncRelay,    nullptr);
    lfo3_syncEnabledAtt = std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter ("lfo3_sync_enabled"),  lfo3_syncEnabledRelay, nullptr);
    lfo3_depthAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo3_depth"),         lfo3_depthRelay,       nullptr);
    lfo3_shapeAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo3_shape"),         lfo3_shapeRelay,       nullptr);
    lfo3_phaseAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo3_phase"),         lfo3_phaseRelay,       nullptr);
    lfo3_gritAtt        = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo3_grit"),          lfo3_gritRelay,        nullptr);
    lfo3_targetAtt      = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo3_target"),        lfo3_targetRelay,      nullptr);
    lfo3_enabledAtt     = std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter ("lfo3_enabled"),       lfo3_enabledRelay,     nullptr);

    // LFO 4
    lfo4_rateAtt        = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo4_rate"),         lfo4_rateRelay,        nullptr);
    lfo4_rateSyncAtt    = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo4_rate_sync"),     lfo4_rateSyncRelay,    nullptr);
    lfo4_syncEnabledAtt = std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter ("lfo4_sync_enabled"),  lfo4_syncEnabledRelay, nullptr);
    lfo4_depthAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo4_depth"),         lfo4_depthRelay,       nullptr);
    lfo4_shapeAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo4_shape"),         lfo4_shapeRelay,       nullptr);
    lfo4_phaseAtt       = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo4_phase"),         lfo4_phaseRelay,       nullptr);
    lfo4_gritAtt        = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo4_grit"),          lfo4_gritRelay,        nullptr);
    lfo4_targetAtt      = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("lfo4_target"),        lfo4_targetRelay,      nullptr);
    lfo4_enabledAtt     = std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter ("lfo4_enabled"),       lfo4_enabledRelay,     nullptr);

    // Global
    chaosAtt        = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("chaos"),              chaosRelay,        nullptr);
    masterDepthAtt  = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("master_depth"),       masterDepthRelay,  nullptr);
    bpmSyncAtt      = std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter ("bpm_sync"),           bpmSyncRelay,      nullptr);
    filterCutoffAtt = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("filter_cutoff_base"), filterCutoffRelay, nullptr);
    filterResAtt    = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("filter_resonance"),   filterResRelay,    nullptr);
    reverbBaseAtt   = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("reverb_base"),        reverbBaseRelay,   nullptr);
    delayBaseAtt    = std::make_unique<juce::WebSliderParameterAttachment>       (*apvts.getParameter ("delay_base"),         delayBaseRelay,    nullptr);

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

LFOFoolAudioProcessorEditor::~LFOFoolAudioProcessorEditor() {}

//==============================================================================
void LFOFoolAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d14));
}

void LFOFoolAudioProcessorEditor::resized()
{
    webView->setBounds (getLocalBounds());
}

//==============================================================================
// Resource Provider

static juce::ZipFile* getLFOFoolZip()
{
    static auto stream = juce::createAssetInputStream ("lfofool_webui.zip",
                                                       juce::AssertAssetExists::no);
    if (stream == nullptr) return nullptr;
    static juce::ZipFile f { stream.get(), false };
    return &f;
}

static const char* getLFOFoolMime (const juce::String& ext)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { "htm",   "text/html"                },
        { "html",  "text/html"                },
        { "txt",   "text/plain"               },
        { "jpg",   "image/jpeg"               },
        { "jpeg",  "image/jpeg"               },
        { "svg",   "image/svg+xml"            },
        { "ico",   "image/vnd.microsoft.icon" },
        { "json",  "application/json"         },
        { "png",   "image/png"                },
        { "css",   "text/css"                 },
        { "map",   "application/json"         },
        { "js",    "text/javascript"          },
        { "woff2", "font/woff2"               }
    };
    if (const auto it = mimeMap.find (ext.toLowerCase()); it != mimeMap.end())
        return it->second;
    return "text/plain";
}

static juce::String getLFOFoolExt (const juce::String& filename)
{
    return filename.fromLastOccurrenceOf (".", false, false);
}

static auto streamToVec (juce::InputStream& stream)
{
    std::vector<std::byte> result ((size_t) stream.getTotalLength());
    stream.setPosition (0);
    [[maybe_unused]] const auto n = stream.read (result.data(), result.size());
    jassert (n == (ssize_t) result.size());
    return result;
}

std::optional<juce::WebBrowserComponent::Resource>
LFOFoolAudioProcessorEditor::getResource (const juce::String& url)
{
    const auto path = url == "/" ? juce::String { "index.html" }
                                 : url.fromFirstOccurrenceOf ("/", false, false);

    if (auto* zip = getLFOFoolZip())
    {
        if (auto* entry = zip->getEntry (path))
        {
            auto stream = juce::rawToUniquePtr (zip->createStreamForEntry (*entry));
            auto data   = streamToVec (*stream);
            auto mime   = getLFOFoolMime (getLFOFoolExt (entry->filename));
            return juce::WebBrowserComponent::Resource { std::move (data),
                                                         juce::String { mime } };
        }
    }

    return std::nullopt;
}
