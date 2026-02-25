#include "PluginEditor.h"
#include "PluginProcessor.h"

TapeBloomAudioProcessorEditor::TapeBloomAudioProcessorEditor (TapeBloomAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      webView (juce::WebBrowserComponent::Options{}
          .withResourceProvider (
              [this] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
              {
                  // Strip scheme+host, keep only the path
                  auto path = url.contains ("://")
                      ? url.fromFirstOccurrenceOf ("/", false, false)
                            .fromFirstOccurrenceOf ("/", false, false)
                      : url;

                  if (path.contains ("/set/"))
                  {
                      auto rest = path.fromFirstOccurrenceOf ("/set/", false, false);
                      int  idx  = rest.upToFirstOccurrenceOf ("/", false, false).getIntValue();
                      float val = rest.fromFirstOccurrenceOf ("/", false, false).getFloatValue();
                      auto& params = audioProcessor.getParameters();
                      if (idx >= 0 && idx < (int) params.size())
                          if (auto* param = dynamic_cast<juce::AudioParameterFloat*> (params[idx]))
                              *param = param->range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, val));
                      return juce::WebBrowserComponent::Resource { std::vector<std::byte>(), "text/plain" };
                  }

                  // Serve the HTML page for root and any unrecognised path
                  auto html = TapeBloomAudioProcessorEditor::getHTML();
                  std::vector<std::byte> bytes (html.getNumBytesAsUTF8());
                  std::memcpy (bytes.data(), html.toRawUTF8(), bytes.size());
                  return juce::WebBrowserComponent::Resource { std::move (bytes), "text/html" };
              },
              juce::String { "juce://plugin/" }))
{
    addAndMakeVisible (webView);
    webView.goToURL ("juce://plugin/");
    setSize (720, 400);
    startTimerHz (30);
}

TapeBloomAudioProcessorEditor::~TapeBloomAudioProcessorEditor()
{
    stopTimer();
}

void TapeBloomAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void TapeBloomAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void TapeBloomAudioProcessorEditor::timerCallback()
{
    auto& params = audioProcessor.getParameters();
    juce::String js = "if(window.updateParams)window.updateParams({";
    bool first = true;
    for (int i = 0; i < (int) params.size(); ++i)
    {
        if (auto* param = dynamic_cast<juce::AudioParameterFloat*> (params[i]))
        {
            if (! first) js += ",";
            js += "p" + juce::String (i) + ":" + juce::String (param->range.convertTo0to1 (param->get()), 4);
            first = false;
        }
    }
    js += "});";
    webView.evaluateJavascript (js, [] (juce::WebBrowserComponent::EvaluationResult) {});
}

juce::String TapeBloomAudioProcessorEditor::getHTML()
{
    return juce::String (R"HTMLEOF(
<!DOCTYPE html><html><head><meta charset="UTF-8"><style>
:root{--color-bg:#101014;--color-surface:#1A1A1F;--color-surface-hl:#292930;--color-primary:#1a1714;--color-text-primary:#F0F0F5;--color-text-secondary:#A0A0A5;--color-border:#3A3A45}
*{box-sizing:border-box;margin:0;padding:0}
html,body{width:100%;height:100%;overflow:hidden;background:var(--color-bg);color:var(--color-text-primary);font-family:-apple-system,BlinkMacSystemFont,'Helvetica Neue',sans-serif}
.plugin-container{width:100%;height:100%;display:flex;flex-direction:column;background:linear-gradient(160deg,var(--color-surface) 0%,var(--color-bg) 100%);border-top:3px solid var(--color-primary)}
.plugin-header{height:42px;border-bottom:1px solid var(--color-border);display:flex;justify-content:space-between;align-items:center;padding:0 20px;font-size:10px;font-weight:700;letter-spacing:3px;text-transform:uppercase}
.plugin-body{flex:1;display:flex;flex-direction:column;justify-content:center;align-items:center;gap:0px;padding:16px 24px}
.knob-row{display:flex;justify-content:center;align-items:flex-end;gap:52px}
.knob-container{display:flex;flex-direction:column;align-items:center;gap:5px;cursor:ns-resize;user-select:none}
.knob-container svg{filter:drop-shadow(0 2px 6px rgba(0,0,0,.5))}
.knob-label{color:var(--color-text-secondary);font-size:9px;font-weight:700;letter-spacing:2px;text-transform:uppercase;text-align:center}
.knob-value{color:var(--color-primary);font-size:10px;font-family:monospace;text-align:center}
.plugin-footer{height:26px;border-top:1px solid var(--color-border);display:flex;justify-content:space-between;align-items:center;padding:0 16px;font-size:8px;letter-spacing:2px;text-transform:uppercase;opacity:.3}
</style></head><body>
<div class="plugin-container">
<div class="plugin-header"><span>TAPEBLOOM</span><span style="opacity:.4;font-size:8px">APC</span></div>
<div class="plugin-body">
  <div class="knob-row"><div class="knob-container" id="p0"><svg width="80" height="80" viewBox="0 0 64 64" style="overflow:visible"><path d="M12.201 51.799 A 28 28 0 1 1 51.799 51.799" fill="none" stroke="var(--color-surface-hl)" stroke-width="6" stroke-linecap="round"/><circle class="value-arc" cx="32" cy="32" r="28" fill="none" stroke="var(--color-primary)" stroke-width="6" stroke-linecap="round" transform="rotate(135 32 32)"/></svg><div class="knob-value" id="v0">0.25</div><div class="knob-label">DRIVE</div></div><div class="knob-container" id="p1"><svg width="80" height="80" viewBox="0 0 64 64" style="overflow:visible"><path d="M12.201 51.799 A 28 28 0 1 1 51.799 51.799" fill="none" stroke="var(--color-surface-hl)" stroke-width="6" stroke-linecap="round"/><circle class="value-arc" cx="32" cy="32" r="28" fill="none" stroke="var(--color-primary)" stroke-width="6" stroke-linecap="round" transform="rotate(135 32 32)"/></svg><div class="knob-value" id="v1">0.50</div><div class="knob-label">WARMTH</div></div><div class="knob-container" id="p2"><svg width="80" height="80" viewBox="0 0 64 64" style="overflow:visible"><path d="M12.201 51.799 A 28 28 0 1 1 51.799 51.799" fill="none" stroke="var(--color-surface-hl)" stroke-width="6" stroke-linecap="round"/><circle class="value-arc" cx="32" cy="32" r="28" fill="none" stroke="var(--color-primary)" stroke-width="6" stroke-linecap="round" transform="rotate(135 32 32)"/></svg><div class="knob-value" id="v2">0.50</div><div class="knob-label">FLUX</div></div><div class="knob-container" id="p3"><svg width="80" height="80" viewBox="0 0 64 64" style="overflow:visible"><path d="M12.201 51.799 A 28 28 0 1 1 51.799 51.799" fill="none" stroke="var(--color-surface-hl)" stroke-width="6" stroke-linecap="round"/><circle class="value-arc" cx="32" cy="32" r="28" fill="none" stroke="var(--color-primary)" stroke-width="6" stroke-linecap="round" transform="rotate(135 32 32)"/></svg><div class="knob-value" id="v3">0.40</div><div class="knob-label">BLOOM</div></div></div>
</div>
<div class="plugin-footer"><span>AUDIO PLUGIN CODER</span><span>JUCE WEBVIEW 8</span></div>
</div>
<script>
var _ks={};
function _xhr(i,n){var x=new XMLHttpRequest();x.open('GET','juce://plugin/set/'+i+'/'+n.toFixed(4),true);x.send();}
function _arc(c,n){var r=parseFloat(c.getAttribute('r')),ci=2*Math.PI*r,a=ci*.75;c.style.strokeDasharray=a+' '+ci;c.style.strokeDashoffset=a-n*a;}
function Knob(el,i,mn,mx,dflt,fmt){
  var arc=el.querySelector('.value-arc'),valEl=document.getElementById('v'+i);
  var norm=(dflt-mn)/(mx-mn);
  function draw(n){norm=Math.max(0,Math.min(1,n));_arc(arc,norm);
    var v=mn+norm*(mx-mn);if(valEl)valEl.textContent=fmt?fmt(v):(v<10?v.toFixed(2):Math.round(v));}
  draw(norm);
  var y0,n0;
  el.addEventListener('mousedown',function(e){y0=e.clientY;n0=norm;e.preventDefault();
    document.body.style.cursor='ns-resize';
    function mm(e2){draw(n0+(y0-e2.clientY)*.005);_xhr(i,norm);}
    function mu(){_xhr(i,norm);document.body.style.cursor='';document.removeEventListener('mousemove',mm);document.removeEventListener('mouseup',mu);}
    document.addEventListener('mousemove',mm);document.addEventListener('mouseup',mu);});
  return draw;
}
window.updateParams=function(vals){Object.keys(vals).forEach(function(k){if(_ks[k])_ks[k](vals[k]);});};
window.onload=function(){_ks['p0']=Knob(document.getElementById('p0'),0,0.0,1.0,0.25,function(v){return v<10?v.toFixed(2):Math.round(v);});_ks['p1']=Knob(document.getElementById('p1'),1,0.0,1.0,0.5,function(v){return v<10?v.toFixed(2):Math.round(v);});_ks['p2']=Knob(document.getElementById('p2'),2,0.0,1.0,0.5,function(v){return v<10?v.toFixed(2):Math.round(v);});_ks['p3']=Knob(document.getElementById('p3'),3,0.0,1.0,0.4,function(v){return v<10?v.toFixed(2):Math.round(v);});};
</script></body></html>
)HTMLEOF");
}
