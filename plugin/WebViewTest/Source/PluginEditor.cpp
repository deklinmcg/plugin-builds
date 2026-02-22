#include "PluginEditor.h"
#include "PluginProcessor.h"

static juce::String getHTML()
{
    return R"HTMLEOF(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
  body { margin: 0; background: #1a1a2e; display: flex; align-items: center; justify-content: center; height: 100vh; font-family: monospace; }
  .box { text-align: center; color: #00ff88; }
  h1 { font-size: 32px; letter-spacing: 4px; margin: 0 0 12px; }
  p { color: #888; font-size: 13px; margin: 0; }
</style>
</head>
<body>
<div class="box">
  <h1>juce:// WORKS</h1>
  <p>WebView loaded successfully via juce:// scheme</p>
</div>
</body>
</html>)HTMLEOF";
}

WebViewTestAudioProcessorEditor::WebViewTestAudioProcessorEditor (WebViewTestAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      webView (juce::WebBrowserComponent::Options{}
          .withResourceProvider (
              [this] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
              {
                  auto html = getHTML();
                  std::vector<std::byte> bytes (html.getNumBytesAsUTF8());
                  std::memcpy (bytes.data(), html.toRawUTF8(), bytes.size());
                  return juce::WebBrowserComponent::Resource { std::move (bytes), "text/html" };
              },
              juce::String { "juce://plugin/" }))
{
    addAndMakeVisible (webView);
    webView.goToURL ("juce://plugin/");
    setSize (560, 320);
}

WebViewTestAudioProcessorEditor::~WebViewTestAudioProcessorEditor() {}
