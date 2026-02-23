#include "PluginEditor.h"
#include "PluginProcessor.h"

MoogGroovFilterAudioProcessorEditor::MoogGroovFilterAudioProcessorEditor (MoogGroovFilterAudioProcessor& p)
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
                  auto html = MoogGroovFilterAudioProcessorEditor::getHTML();
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

MoogGroovFilterAudioProcessorEditor::~MoogGroovFilterAudioProcessorEditor()
{
    stopTimer();
}

void MoogGroovFilterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void MoogGroovFilterAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void MoogGroovFilterAudioProcessorEditor::timerCallback()
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

juce::String MoogGroovFilterAudioProcessorEditor::getHTML()
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

/* Accent overrides */
.tog button.on{background:#ffb347;color:#111;font-weight:700;}
.knob-ring{background:radial-gradient(circle at 35% 30%,#5a4a2a,#1e1c1a);}
body{background:#1a1a1a;font-family:'Segoe UI',Arial,sans-serif;}
.kv{color:#ffb347;font-size:12px;}
.kl{color:#ffffff;font-size:9px;letter-spacing:2px;}
.sec{color:#ffb347;letter-spacing:3px;font-size:9px;}

/* Layout */
#plugin-wrap{
  width:560px;height:320px;
  background:#1a1a1a;
  display:flex;flex-direction:column;
  position:relative;
}
/* Panel lines */
#plugin-wrap::before{
  content:'';position:absolute;left:0;right:0;top:44px;height:1px;background:#2e2e2e;
}
#plugin-wrap::after{
  content:'';position:absolute;left:340px;top:50px;bottom:8px;width:1px;background:#2e2e2e;
}
.divline-v{position:absolute;left:340px;top:50px;bottom:8px;width:1px;background:#2e2e2e;}
.divline-h2{position:absolute;left:0;right:0;top:44px;height:1px;background:#2e2e2e;}

#header{
  display:flex;align-items:center;justify-content:space-between;
  padding:8px 18px 6px 18px;
  border-bottom:1px solid #2e2e2e;
}
.plugin-name{
  font-size:17px;font-weight:700;letter-spacing:4px;
  text-transform:uppercase;color:#ffffff;
  text-shadow:0 0 10px rgba(255,179,71,0.35);
}
.plugin-sub{font-size:9px;color:#555;letter-spacing:3px;text-transform:uppercase;margin-top:2px;}

/* Main area */
#main{display:flex;flex:1;padding:10px 14px 10px 14px;gap:0;}

/* Left section */
#left{display:flex;flex-direction:column;align-items:center;justify-content:center;width:200px;gap:8px;}
.left-top{display:flex;align-items:flex-end;justify-content:center;gap:18px;width:100%;}
.cutoff-wrap{display:flex;flex-direction:column;align-items:center;}
.cutoff-wrap .knob-ring{width:70px;height:70px;}
.cutoff-wrap .knob-ring .dot{transform-origin:50% 27px;height:18px;}
.cutoff-wrap .kv{font-size:13px;}
.left-bot{display:flex;align-items:flex-end;justify-content:center;gap:22px;width:100%;margin-top:4px;}
.sec-label{font-size:8px;letter-spacing:3px;text-transform:uppercase;color:#ffb347;text-align:center;margin-bottom:4px;}

/* Middle section */
#mid{display:flex;flex-direction:column;align-items:center;justify-content:center;width:140px;gap:10px;border-left:1px solid #2e2e2e;border-right:1px solid #2e2e2e;padding:0 10px;}

/* Right section */
#right{display:flex;flex-direction:column;align-items:center;justify-content:center;width:200px;gap:8px;padding-left:10px;}

.right-top{display:flex;align-items:flex-end;justify-content:center;gap:18px;width:100%;}
.right-bot{display:flex;flex-direction:column;align-items:center;gap:8px;width:100%;margin-top:4px;}

.tog-row{display:flex;flex-direction:column;align-items:center;gap:4px;width:100%;}
.tog-label{font-size:8px;letter-spacing:2px;text-transform:uppercase;color:#ffffff;text-align:center;}

.sync-div-wrap{display:flex;flex-direction:column;align-items:center;gap:4px;}
.sync-row{display:flex;gap:6px;align-items:center;}
.div-tog{display:inline-flex;border:1px solid #333;border-radius:3px;overflow:hidden;}
.div-tog button{padding:4px 7px;background:transparent;border:none;font-size:9px;letter-spacing:0px;cursor:pointer;color:#555;}
.div-tog button.on{background:#ffb347;color:#111;font-weight:700;}

.bottom-row{display:flex;align-items:flex-end;justify-content:flex-end;gap:18px;width:100%;margin-top:6px;}

/* Large cutoff knob ring override */
.cutoff-wrap .knob-ring{
  width:72px;height:72px;
  background:radial-gradient(circle at 35% 30%,#6a5020,#1e1c1a);
  border:2px solid #ffb347;
  box-shadow:0 4px 14px rgba(255,179,71,0.25),inset 0 1px 0 rgba(255,255,255,.08);
}

</style></head>
<body>
<!-- LAYOUT_HERE -->
<div id="plugin-wrap">
  <div id="header">
    <div>
      <div class="plugin-name">MoogGroov<span style="color:#ffb347">Filter</span></div>
      <div class="plugin-sub">4-pole Ladder · LFO Groove · Warm Saturation</div>
    </div>
    <div style="font-size:9px;color:#444;letter-spacing:2px;">4-POLE</div>
  </div>

  <div id="main">
    <!-- LEFT: Cutoff big knob + Resonance + Drive -->
    <div id="left">
      <div class="sec-label">Filter</div>
      <div class="left-top">
        <!-- Resonance -->
        <div class="kw" id="k1">
          <div class="knob-ring"><div class="dot" style="background:#ffffff"></div></div>
          <div class="kv" id="k1-v">0.30</div>
          <div class="kl">Resonance</div>
        </div>
        <!-- Cutoff (big) -->
        <div class="cutoff-wrap kw" id="k0">
          <div class="knob-ring"><div class="dot" style="background:#ffffff"></div></div>
          <div class="kv" id="k0-v">2000 Hz</div>
          <div class="kl">Cutoff</div>
        </div>
        <!-- Drive -->
        <div class="kw" id="k8">
          <div class="knob-ring"><div class="dot" style="background:#ffffff"></div></div>
          <div class="kv" id="k8-v">0.20</div>
          <div class="kl">Drive</div>
        </div>
      </div>
    </div>

    <!-- MID: Mix + Output + sync toggles vertical -->
    <div id="mid">
      <div class="sec-label">Output</div>
      <div class="kw" id="k9">
        <div class="knob-ring"><div class="dot" style="background:#ffffff"></div></div>
        <div class="kv" id="k9-v">1.00</div>
        <div class="kl">Mix</div>
      </div>
      <div class="kw" id="k10">
        <div class="knob-ring"><div class="dot" style="background:#ffffff"></div></div>
        <div class="kv" id="k10-v">0.0 dB</div>
        <div class="kl">Output</div>
      </div>
    </div>

    <!-- RIGHT: LFO section -->
    <div id="right">
      <div class="sec-label">LFO</div>
      <div class="right-top">
        <!-- LFO Rate -->
        <div class="kw" id="k2">
          <div class="knob-ring"><div class="dot" style="background:#ffffff"></div></div>
          <div class="kv" id="k2-v">1.00 Hz</div>
          <div class="kl">Rate</div>
        </div>
        <!-- LFO Depth -->
        <div class="kw" id="k6">
          <div class="knob-ring"><div class="dot" style="background:#ffffff"></div></div>
          <div class="kv" id="k6-v">0.50</div>
          <div class="kl">Depth</div>
        </div>
      </div>

      <div class="right-bot">
        <!-- Shape toggle -->
        <div class="tog-row">
          <div class="tog-label">Shape</div>
          <div class="tog" id="tog-shape">
            <button class="on">Sine</button>
            <button>Tri</button>
          </div>
        </div>

        <!-- Polarity toggle -->
        <div class="tog-row">
          <div class="tog-label">Polarity</div>
          <div class="tog" id="tog-pol">
            <button class="on">Bi</button>
            <button>Uni</button>
          </div>
        </div>

        <!-- Sync mode toggle + division -->
        <div class="sync-div-wrap">
          <div class="tog-label" style="margin-bottom:4px;">Sync Mode</div>
          <div class="sync-row">
            <div class="tog" id="tog-sync">
              <button class="on">Free</button>
              <button>Sync</button>
            </div>
          </div>
          <div style="margin-top:6px;">
            <div class="tog-label" style="margin-bottom:3px;">Division</div>
            <div class="div-tog" id="tog-div">
              <button>1/16</button>
              <button>1/8</button>
              <button class="on">1/4</button>
              <button>1/2</button>
              <button>1/1</button>
              <button>2/1</button>
            </div>
          </div>
        </div>
      </div>
    </div>
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
/* INIT_HERE */
// p0: Cutoff 20-20000 Hz log, default 2000
_ks['p0'] = knob(document.getElementById('k0'), 0, 20, 20000, 2000, function(v){
  if(v>=1000) return (v/1000).toFixed(2)+'kHz';
  return Math.round(v)+' Hz';
});

// p1: Resonance 0.0-0.95, default 0.3
_ks['p1'] = knob(document.getElementById('k1'), 1, 0.0, 0.95, 0.3, function(v){
  return v.toFixed(2);
});

// p2: LFO Rate 0.01-20.0 Hz log, default 1.0
_ks['p2'] = knob(document.getElementById('k2'), 2, 0.01, 20.0, 1.0, function(v){
  return v.toFixed(2)+' Hz';
});

// p3: LFO Sync division 0-5 (1/16,1/8,1/4,1/2,1/1,2/1), default 2 (1/4)
// handled by tog-div below

// p4: SyncMode 0=Free,1=Sync, default 0
// handled by tog-sync below

// p5: LFO Shape 0=Sine,1=Tri, default 0
// handled by tog-shape below

// p6: LFO Depth 0.0-1.0, default 0.5
_ks['p6'] = knob(document.getElementById('k6'), 6, 0.0, 1.0, 0.5, function(v){
  return v.toFixed(2);
});

// p7: LFO Polarity 0=Bi,1=Uni, default 0
// handled by tog-pol below

// p8: Drive 0.0-1.0, default 0.2
_ks['p8'] = knob(document.getElementById('k8'), 8, 0.0, 1.0, 0.2, function(v){
  return v.toFixed(2);
});

// p9: Mix 0.0-1.0, default 1.0
_ks['p9'] = knob(document.getElementById('k9'), 9, 0.0, 1.0, 1.0, function(v){
  return v.toFixed(2);
});

// p10: Output -12.0-6.0 dB, default 0.0
_ks['p10'] = knob(document.getElementById('k10'), 10, -12.0, 6.0, 0.0, function(v){
  return v.toFixed(1)+' dB';
});

// Toggle: LFO Shape (p5)
tog(document.getElementById('tog-shape'), function(i){
  _xhr(5, i===0?0.0:1.0);
});

// Toggle: Polarity (p7)
tog(document.getElementById('tog-pol'), function(i){
  _xhr(7, i===0?0.0:1.0);
});

// Toggle: SyncMode (p4)
tog(document.getElementById('tog-sync'), function(i){
  _xhr(4, i===0?0.0:1.0);
});

// Toggle: LFO Sync Division (p3) — 6 options mapped 0..1
var divBtns = document.getElementById('tog-div').querySelectorAll('button');
divBtns.forEach(function(b,i){
  b.onclick=function(){
    divBtns.forEach(function(x){x.classList.remove('on')});
    b.classList.add('on');
    _xhr(3, i/5.0);
  };
});

};
</script></body></html>
)HTMLEOF");
}
