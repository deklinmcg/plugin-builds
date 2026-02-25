#include "PluginEditor.h"
#include "PluginProcessor.h"

LFOToolV1AudioProcessorEditor::LFOToolV1AudioProcessorEditor (LFOToolV1AudioProcessor& p)
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
                  auto html = LFOToolV1AudioProcessorEditor::getHTML();
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

LFOToolV1AudioProcessorEditor::~LFOToolV1AudioProcessorEditor()
{
    stopTimer();
}

void LFOToolV1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void LFOToolV1AudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void LFOToolV1AudioProcessorEditor::timerCallback()
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

juce::String LFOToolV1AudioProcessorEditor::getHTML()
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

body{background:#18161a;font-family:sans-serif;}
.plugin-header{display:flex;align-items:center;justify-content:space-between;padding:10px 18px 6px 18px;}
.plugin-name{font-size:15px;letter-spacing:5px;text-transform:uppercase;color:#b07aff;font-weight:700;text-shadow:0 0 12px #7c3aff88;}
.plugin-sub{font-size:9px;letter-spacing:3px;color:#555;text-transform:uppercase;}
.main-content{display:flex;flex-direction:row;gap:0;padding:0 10px;}
.panel{background:#1c1a20;border:1px solid #2a2730;border-radius:6px;padding:10px 8px 8px 8px;display:flex;flex-direction:column;align-items:center;}
.panel-row{display:flex;flex-direction:row;align-items:flex-end;gap:10px;justify-content:center;}
.divider{width:1px;background:#2a2730;margin:0 6px;align-self:stretch;}
.tog button.on{background:#7c3aff;color:#fff;}
.tog{border-color:#3a3550;}
.panel-label{font-size:8px;letter-spacing:3px;text-transform:uppercase;color:#5a4a7a;margin-bottom:7px;text-align:center;}
.sync-row{display:flex;flex-direction:row;align-items:center;gap:8px;margin-bottom:6px;}
.sync-label{font-size:8px;letter-spacing:2px;text-transform:uppercase;color:#555;}
.waveform-display{width:80px;height:36px;border:1px solid #2a2730;border-radius:4px;background:#110f14;display:flex;align-items:center;justify-content:center;margin-bottom:6px;}
.waveform-display svg{display:block;}
.lfo-vis{width:76px;height:32px;}
.bottom-row{display:flex;flex-direction:row;gap:10px;padding:4px 10px 0 10px;align-items:flex-end;}
.kw .kl{color:#5a4a7a;}
.kw .kv{color:#9a7acc;}
</style></head>
<body>

<div class="plugin-header">
  <div>
    <div class="plugin-name">LFOTool V1</div>
    <div class="plugin-sub">Modulation LFO</div>
  </div>
  <div style="display:flex;flex-direction:column;align-items:flex-end;gap:4px;">
    <div class="sync-row">
      <span class="sync-label">Mode</span>
      <div class="tog" id="tog-syncmode">
        <button>Free</button>
        <button>Sync</button>
      </div>
    </div>
    <div class="sync-row">
      <span class="sync-label">Filter</span>
      <div class="tog" id="tog-filtertype">
        <button>LP</button>
        <button>HP</button>
        <button>BP</button>
      </div>
    </div>
  </div>
</div>

<div class="main-content">

  <!-- LFO Section -->
  <div class="panel" style="flex:0 0 auto;min-width:148px;">
    <div class="panel-label">LFO</div>
    <div class="waveform-display" id="lfo-wave-display">
      <svg class="lfo-vis" id="lfo-svg" viewBox="0 0 76 32">
        <path id="lfo-path" stroke="#b07aff" stroke-width="1.5" fill="none" d=""/>
      </svg>
    </div>
    <div class="panel-row" style="margin-bottom:6px;">
      <div class="kw" id="k0">
        <div class="knob-ring"><div class="dot" style="background:#b07aff"></div></div>
        <div class="kv">1.00</div>
        <div class="kl">Rate</div>
      </div>
      <div class="kw" id="k1">
        <div class="knob-ring"><div class="dot" style="background:#b07aff"></div></div>
        <div class="kv">1/4</div>
        <div class="kl">Division</div>
      </div>
    </div>
    <div class="panel-row">
      <div class="kw" id="k3">
        <div class="knob-ring"><div class="dot" style="background:#b07aff"></div></div>
        <div class="kv">S-Crv</div>
        <div class="kl">Shape</div>
      </div>
      <div class="kw" id="k4">
        <div class="knob-ring"><div class="dot" style="background:#b07aff"></div></div>
        <div class="kv">0°</div>
        <div class="kl">Phase</div>
      </div>
    </div>
  </div>

  <div class="divider"></div>

  <!-- Depth Section -->
  <div class="panel" style="flex:0 0 auto;min-width:140px;">
    <div class="panel-label">Depth</div>
    <div class="panel-row" style="margin-bottom:8px;">
      <div class="kw" id="k5">
        <div class="knob-ring"><div class="dot" style="background:#ff7aaa"></div></div>
        <div class="kv">0.80</div>
        <div class="kl">Volume</div>
      </div>
      <div class="kw" id="k6">
        <div class="knob-ring"><div class="dot" style="background:#ff7aaa"></div></div>
        <div class="kv">0.00</div>
        <div class="kl">Filter</div>
      </div>
    </div>
    <div class="panel-row">
      <div class="kw" id="k7">
        <div class="knob-ring"><div class="dot" style="background:#ff7aaa"></div></div>
        <div class="kv">0.00</div>
        <div class="kl">Pan</div>
      </div>
      <div class="kw" id="k11">
        <div class="knob-ring"><div class="dot" style="background:#ff7aaa"></div></div>
        <div class="kv">0.10</div>
        <div class="kl">Smooth</div>
      </div>
    </div>
  </div>

  <div class="divider"></div>

  <!-- Filter Section -->
  <div class="panel" style="flex:0 0 auto;min-width:120px;">
    <div class="panel-label">Filter</div>
    <div class="panel-row" style="margin-bottom:8px;">
      <div class="kw" id="k8">
        <div class="knob-ring"><div class="dot" style="background:#7ad4ff"></div></div>
        <div class="kv">2kHz</div>
        <div class="kl">Cutoff</div>
      </div>
    </div>
    <div class="panel-row">
      <div class="kw" id="k9">
        <div class="knob-ring"><div class="dot" style="background:#7ad4ff"></div></div>
        <div class="kv">0.10</div>
        <div class="kl">Res</div>
      </div>
    </div>
  </div>

  <div class="divider"></div>

  <!-- Output Section -->
  <div class="panel" style="flex:0 0 auto;min-width:110px;">
    <div class="panel-label">Output</div>
    <div class="panel-row" style="margin-bottom:8px;">
      <div class="kw" id="k12">
        <div class="knob-ring"><div class="dot" style="background:#7affb0"></div></div>
        <div class="kv">1.00</div>
        <div class="kl">Mix</div>
      </div>
    </div>
    <div class="panel-row">
      <div class="kw" id="k13">
        <div class="knob-ring"><div class="dot" style="background:#7affb0"></div></div>
        <div class="kv">0.0dB</div>
        <div class="kl">Gain</div>
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

// Waveform preview
var _shapeIdx=5;
function drawWave(shapeIdx){
  _shapeIdx=shapeIdx;
  var pts=[];
  var W=76,H=32,n=120;
  for(var i=0;i<=n;i++){
    var t=i/n;
    var raw=0;
    switch(Math.round(shapeIdx)){
      case 0: raw=Math.sin(t*2*Math.PI); break;
      case 1: raw=1.0-Math.abs(t*4.0-2.0); raw=Math.max(-1,Math.min(1,raw)); break;
      case 2: raw=1.0-2.0*t; break;
      case 3: raw=2.0*t-1.0; break;
      case 4: raw=t<0.5?1.0:-1.0; break;
      case 5: raw=Math.tanh(3.0*Math.sin(t*2*Math.PI))/Math.tanh(3.0); break;
    }
    var x=t*W;
    var y=(1.0-(raw+1.0)*0.5)*H;
    pts.push((i===0?'M':'L')+x.toFixed(1)+','+y.toFixed(1));
  }
  document.getElementById('lfo-path').setAttribute('d',pts.join(' '));
}

var divNames=['1/32','1/16','1/8T','1/8','1/4T','1/4','1/2','1/1','2/1','4/1'];
var shapeNames=['Sine','Tri','SawDn','SawUp','Sqr','S-Crv'];

window.onload=function(){
  /* INIT_HERE */
  _ks['p0']=knob(document.getElementById('k0'),0,0.01,20.0,1.0,function(v){return v.toFixed(2)+'Hz';});
  _ks['p1']=knob(document.getElementById('k1'),1,0,10,3,function(v){var i=Math.round(Math.max(0,Math.min(10,v)));return divNames[i]||'1/4';});
  _ks['p3']=knob(document.getElementById('k3'),3,0,5,5,function(v){var i=Math.round(Math.max(0,Math.min(5,v)));drawWave(i);return shapeNames[i]||'S-Crv';});
  _ks['p4']=knob(document.getElementById('k4'),4,0.0,360.0,0.0,function(v){return Math.round(v)+'°';});
  _ks['p5']=knob(document.getElementById('k5'),5,0.0,1.0,0.8,function(v){return v.toFixed(2);});
  _ks['p6']=knob(document.getElementById('k6'),6,0.0,1.0,0.0,function(v){return v.toFixed(2);});
  _ks['p7']=knob(document.getElementById('k7'),7,0.0,1.0,0.0,function(v){return v.toFixed(2);});
  _ks['p8']=knob(document.getElementById('k8'),8,20.0,20000.0,2000.0,function(v){return v>=1000?(v/1000).toFixed(1)+'kHz':Math.round(v)+'Hz';});
  _ks['p9']=knob(document.getElementById('k9'),9,0.0,1.0,0.1,function(v){return v.toFixed(2);});
  _ks['p11']=knob(document.getElementById('k11'),11,0.0,1.0,0.1,function(v){return v.toFixed(2);});
  _ks['p12']=knob(document.getElementById('k12'),12,0.0,1.0,1.0,function(v){return v.toFixed(2);});
  _ks['p13']=knob(document.getElementById('k13'),13,-12.0,12.0,0.0,function(v){return (v>=0?'+':'')+v.toFixed(1)+'dB';});

  // SyncMode toggle (param index 2)
  var smTog=document.getElementById('tog-syncmode');
  var smBtns=smTog.querySelectorAll('button');
  smBtns[1].classList.add('on'); // default sync=1
  tog(smTog,function(i){_xhr(2,i);});

  // FilterType toggle (param index 10)
  var ftTog=document.getElementById('tog-filtertype');
  var ftBtns=ftTog.querySelectorAll('button');
  ftBtns[0].classList.add('on'); // default lp=0
  tog(ftTog,function(i){_xhr(10,i/2);});

  drawWave(5);
};
</script></body></html>
)HTMLEOF");
}
