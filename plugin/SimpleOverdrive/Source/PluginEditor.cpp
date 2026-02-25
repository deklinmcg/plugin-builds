#include "PluginEditor.h"
#include "PluginProcessor.h"

SimpleOverdriveAudioProcessorEditor::SimpleOverdriveAudioProcessorEditor (SimpleOverdriveAudioProcessor& p)
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
                  auto html = SimpleOverdriveAudioProcessorEditor::getHTML();
                  std::vector<std::byte> bytes (html.getNumBytesAsUTF8());
                  std::memcpy (bytes.data(), html.toRawUTF8(), bytes.size());
                  return juce::WebBrowserComponent::Resource { std::move (bytes), "text/html" };
              },
              juce::String { "juce://plugin/" }))
{
    addAndMakeVisible (webView);
    webView.goToURL ("juce://plugin/");
    setSize (560, 320);
    startTimerHz (30);
}

SimpleOverdriveAudioProcessorEditor::~SimpleOverdriveAudioProcessorEditor()
{
    stopTimer();
}

void SimpleOverdriveAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void SimpleOverdriveAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void SimpleOverdriveAudioProcessorEditor::timerCallback()
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

juce::String SimpleOverdriveAudioProcessorEditor::getHTML()
{
    return juce::String (R"HTMLEOF(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><style>
*{box-sizing:border-box;margin:0;padding:0}
html,body{width:560px;height:320px;overflow:hidden}
.kw{display:flex;flex-direction:column;align-items:center;gap:5px}
.knob-ring{width:52px;height:52px;border-radius:50%;
  background:radial-gradient(circle at 35% 30%,#484440,#1e1c1a);
  border:2px solid #555;
  box-shadow:0 3px 8px rgba(0,0,0,.7),inset 0 1px 0 rgba(255,255,255,.06);
  cursor:ns-resize;position:relative;flex-shrink:0}
.knob-ring .dot{position:absolute;top:5px;left:50%;width:3px;height:14px;
  margin-left:-1.5px;border-radius:2px;transform-origin:50% 20px}
.kv{font-size:10px;color:#888;font-family:monospace;text-align:center;min-width:54px}
.kl{font-size:9px;letter-spacing:2px;text-transform:uppercase;color:#555}
.tog{display:inline-flex;border:1px solid #444;border-radius:3px;overflow:hidden}
.tog button{padding:5px 10px;background:transparent;border:none;
  font-size:10px;letter-spacing:1px;text-transform:uppercase;cursor:pointer;color:#666}
.tog button.on{color:#111}
.sec{font-size:9px;letter-spacing:3px;text-transform:uppercase;color:#444;margin-bottom:12px}

body{background:#1a1a1a;}
.tog button.on{background:#e07a00;}

#main-container{
  width:560px;height:320px;
  display:flex;flex-direction:column;
  align-items:center;justify-content:center;
  gap:0;
}
.plugin-name{
  font-family:monospace;
  font-size:13px;
  letter-spacing:6px;
  text-transform:uppercase;
  color:#e07a00;
  margin-bottom:36px;
  opacity:0.85;
}
#k0{gap:10px;}
#k0 .knob-ring{
  width:100px;
  height:100px;
  border-radius:50%;
  background:radial-gradient(circle at 35% 30%,#5a4010,#2a1c00);
  border:2.5px solid #e07a00;
  box-shadow:0 4px 24px rgba(224,122,0,0.25),0 6px 16px rgba(0,0,0,0.8),inset 0 1px 0 rgba(255,200,80,0.10);
}
#k0 .knob-ring .dot{
  top:8px;
  width:4px;
  height:24px;
  margin-left:-2px;
  border-radius:3px;
  transform-origin:50% 42px;
}
#k0 .kv{
  font-size:13px;
  color:#e07a00;
  font-family:monospace;
  letter-spacing:1px;
  margin-top:2px;
}
#k0 .kl{
  font-size:10px;
  letter-spacing:5px;
  color:#a05500;
  font-family:monospace;
}
</style></head>
<body>

<div id="main-container">
  <div class="plugin-name">SimpleOverdrive</div>
  <div class="kw" id="k0">
    <div class="knob-ring"><div class="dot" style="background:#e07a00"></div></div>
    <div class="kv" id="k0-v">1.00</div>
    <div class="kl">Drive</div>
  </div>
</div>

<script>
var _ks={};
function _xhr(idx,norm){
  var x=new XMLHttpRequest();
  x.open('GET','juce://plugin/set/'+idx+'/'+norm.toFixed(4),true);
  x.send();
}
function knob(el,idx,min,max,init,fmt){
  var dot=el.querySelector('.knob-ring .dot'), valEl=el.querySelector('.kv');
  var norm=(init-min)/(max-min);
  function draw(n){
    norm=Math.max(0,Math.min(1,n));
    dot.style.transform='rotate('+(-135+norm*270)+'deg)';
    var v=min+norm*(max-min);
    if(valEl)valEl.textContent=fmt?fmt(v):(v<10?v.toFixed(2):Math.round(v));
  }
  draw(norm);
  el.querySelector('.knob-ring').addEventListener('mousedown',function(e){
    var y0=e.clientY,n0=norm;e.preventDefault();
    function mm(e2){draw(n0+(y0-e2.clientY)/120);_xhr(idx,norm);}
    function mu(){_xhr(idx,norm);document.removeEventListener('mousemove',mm);document.removeEventListener('mouseup',mu);}
    document.addEventListener('mousemove',mm);document.addEventListener('mouseup',mu);
  });
  return draw;
}
function tog(el,cb){
  var bs=el.querySelectorAll('button');
  bs.forEach(function(b,i){b.onclick=function(){
    bs.forEach(function(x){x.classList.remove('on')});b.classList.add('on');if(cb)cb(i);
  };});
}
window.updateParams=function(vals){
  Object.keys(vals).forEach(function(k){if(_ks[k])_ks[k](vals[k]);});
};
window.onload=function(){
  _ks['p0'] = knob(document.getElementById('k0'), 0, 1.0, 20.0, 1.0, function(v){ return v.toFixed(2); });
};
</script></body></html>
)HTMLEOF");
}
