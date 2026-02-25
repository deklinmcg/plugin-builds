#include "PluginEditor.h"
#include "PluginProcessor.h"

MyPluginAudioProcessorEditor::MyPluginAudioProcessorEditor (MyPluginAudioProcessor& p)
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
                  auto html = MyPluginAudioProcessorEditor::getHTML();
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

MyPluginAudioProcessorEditor::~MyPluginAudioProcessorEditor()
{
    stopTimer();
}

void MyPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void MyPluginAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void MyPluginAudioProcessorEditor::timerCallback()
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

juce::String MyPluginAudioProcessorEditor::getHTML()
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
.tog button.on{background:#c8a84b;color:#111;}
body{
  background:#16140f;
  font-family:'Helvetica Neue',Helvetica,Arial,sans-serif;
  color:#aaa;
  display:flex;
  flex-direction:column;
  align-items:center;
}
.header{
  width:100%;
  display:flex;
  align-items:center;
  justify-content:center;
  padding:16px 24px 8px 24px;
  position:relative;
  border-bottom:1px solid #2a2820;
}
.plugin-name{
  font-size:18px;
  letter-spacing:8px;
  text-transform:uppercase;
  color:#c8a84b;
  font-weight:300;
  text-shadow:0 0 16px rgba(200,168,75,0.3);
}
.plugin-sub{
  font-size:8px;
  letter-spacing:4px;
  text-transform:uppercase;
  color:#3a3620;
  position:absolute;
  bottom:6px;
  right:20px;
}
.main-area{
  display:flex;
  flex-direction:row;
  align-items:flex-start;
  justify-content:center;
  width:100%;
  padding:18px 24px 10px 24px;
  gap:0;
  flex:1;
}
.section{
  display:flex;
  flex-direction:column;
  align-items:center;
  padding:0 18px;
  border-right:1px solid #22201a;
}
.section:last-child{border-right:none;}
.sec{margin-bottom:14px;color:#3a3620;}
.knobs-row{
  display:flex;
  flex-direction:row;
  gap:18px;
  align-items:flex-end;
}
.divider{
  width:1px;
  background:#22201a;
  align-self:stretch;
  margin:0 4px;
}
.footer{
  width:100%;
  border-top:1px solid #1e1c14;
  display:flex;
  align-items:center;
  justify-content:center;
  padding:8px 24px;
  gap:32px;
}
.footer-label{
  font-size:8px;
  letter-spacing:3px;
  text-transform:uppercase;
  color:#2e2c20;
}
</style></head>
<body>

<div class="header">
  <span class="plugin-name">MyPlugin</span>
  <span class="plugin-sub">Audio Processor</span>
</div>

<div class="main-area">

  <div class="section">
    <div class="sec">Input</div>
    <div class="knobs-row">
      <div class="kw" id="k0">
        <div class="knob-ring"><div class="dot" style="background:#c8a84b"></div></div>
        <div class="kv" id="k0-v">0.00</div>
        <div class="kl">Gain</div>
      </div>
      <div class="kw" id="k1">
        <div class="knob-ring"><div class="dot" style="background:#c8a84b"></div></div>
        <div class="kv" id="k1-v">0.00</div>
        <div class="kl">Drive</div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="sec">Tone</div>
    <div class="knobs-row">
      <div class="kw" id="k2">
        <div class="knob-ring"><div class="dot" style="background:#7ab8d4"></div></div>
        <div class="kv" id="k2-v">0.00</div>
        <div class="kl">Low</div>
      </div>
      <div class="kw" id="k3">
        <div class="knob-ring"><div class="dot" style="background:#7ab8d4"></div></div>
        <div class="kv" id="k3-v">0.00</div>
        <div class="kl">Mid</div>
      </div>
      <div class="kw" id="k4">
        <div class="knob-ring"><div class="dot" style="background:#7ab8d4"></div></div>
        <div class="kv" id="k4-v">0.00</div>
        <div class="kl">High</div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="sec">Dynamics</div>
    <div class="knobs-row">
      <div class="kw" id="k5">
        <div class="knob-ring"><div class="dot" style="background:#b47ad4"></div></div>
        <div class="kv" id="k5-v">0.00</div>
        <div class="kl">Attack</div>
      </div>
      <div class="kw" id="k6">
        <div class="knob-ring"><div class="dot" style="background:#b47ad4"></div></div>
        <div class="kv" id="k6-v">0.00</div>
        <div class="kl">Release</div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="sec">Output</div>
    <div class="knobs-row">
      <div class="kw" id="k7">
        <div class="knob-ring"><div class="dot" style="background:#c8a84b"></div></div>
        <div class="kv" id="k7-v">0.00</div>
        <div class="kl">Mix</div>
      </div>
      <div class="kw" id="k8">
        <div class="knob-ring"><div class="dot" style="background:#c8a84b"></div></div>
        <div class="kv" id="k8-v">0.00</div>
        <div class="kl">Volume</div>
      </div>
    </div>
  </div>

</div>

<div class="footer">
  <span class="footer-label">MyPluginAudioProcessor</span>
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
  _ks['p0'] = knob(document.getElementById('k0'), 0, -24, 24, 0, function(v){return (v>=0?'+':'')+v.toFixed(1)+' dB';});
  _ks['p1'] = knob(document.getElementById('k1'), 1, 0, 100, 0, function(v){return v.toFixed(1)+'%';});
  _ks['p2'] = knob(document.getElementById('k2'), 2, -12, 12, 0, function(v){return (v>=0?'+':'')+v.toFixed(1)+' dB';});
  _ks['p3'] = knob(document.getElementById('k3'), 3, -12, 12, 0, function(v){return (v>=0?'+':'')+v.toFixed(1)+' dB';});
  _ks['p4'] = knob(document.getElementById('k4'), 4, -12, 12, 0, function(v){return (v>=0?'+':'')+v.toFixed(1)+' dB';});
  _ks['p5'] = knob(document.getElementById('k5'), 5, 1, 200, 10, function(v){return v.toFixed(0)+' ms';});
  _ks['p6'] = knob(document.getElementById('k6'), 6, 10, 2000, 100, function(v){return v.toFixed(0)+' ms';});
  _ks['p7'] = knob(document.getElementById('k7'), 7, 0, 100, 100, function(v){return v.toFixed(0)+'%';});
  _ks['p8'] = knob(document.getElementById('k8'), 8, -24, 24, 0, function(v){return (v>=0?'+':'')+v.toFixed(1)+' dB';});
};
</script></body></html>
)HTMLEOF");
}
