#include "PluginEditor.h"
#include "PluginProcessor.h"

LFOToolProAudioProcessorEditor::LFOToolProAudioProcessorEditor (LFOToolProAudioProcessor& p)
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
                  auto html = LFOToolProAudioProcessorEditor::getHTML();
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

LFOToolProAudioProcessorEditor::~LFOToolProAudioProcessorEditor()
{
    stopTimer();
}

void LFOToolProAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void LFOToolProAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void LFOToolProAudioProcessorEditor::timerCallback()
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

juce::String LFOToolProAudioProcessorEditor::getHTML()
{
    return juce::String (R"HTMLEOF(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><style>
*{box-sizing:border-box;margin:0;padding:0}
html,body{width:560px;height:320px;overflow:hidden;background:#0f0f0f;font-family:'Segoe UI',monospace}
.kw{display:flex;flex-direction:column;align-items:center;gap:3px}
.knob-ring{width:36px;height:36px;border-radius:50%;
  background:radial-gradient(circle at 35% 30%,#2a2826,#111);
  border:2px solid #333;
  box-shadow:0 2px 6px rgba(0,0,0,.8),inset 0 1px 0 rgba(255,255,255,.04);
  cursor:ns-resize;position:relative;flex-shrink:0}
.knob-ring .dot{position:absolute;top:3px;left:50%;width:2px;height:9px;
  margin-left:-1px;border-radius:2px;transform-origin:50% 15px}
.kv{font-size:8px;color:#888;font-family:monospace;text-align:center;min-width:38px}
.kl{font-size:7px;letter-spacing:1.5px;text-transform:uppercase;color:#555}
.tog{display:inline-flex;border:1px solid #333;border-radius:3px;overflow:hidden}
.tog button{padding:2px 5px;background:transparent;border:none;
  font-size:7px;letter-spacing:1px;text-transform:uppercase;cursor:pointer;color:#555}
.tog button.on{background:#00ffe0;color:#000}
.sec{font-size:8px;letter-spacing:2px;text-transform:uppercase;color:#444}
/* Layout */
#main{width:560px;height:320px;display:flex;flex-direction:column;background:#0f0f0f;position:relative}
#title-bar{height:24px;display:flex;align-items:center;padding:0 10px;border-bottom:1px solid #1a1a1a;flex-shrink:0}
#plugin-name{font-size:11px;letter-spacing:4px;text-transform:uppercase;color:#00ffe0;font-weight:700}
#plugin-sub{font-size:8px;letter-spacing:2px;color:#444;margin-left:10px}
#lanes{display:flex;flex:1;overflow:hidden}
.lane{flex:1;display:flex;flex-direction:column;border-right:1px solid #1a1a1a;padding:4px;position:relative}
.lane:last-child{border-right:none}
.lane-label{font-size:7px;letter-spacing:3px;text-transform:uppercase;color:#00ffe0;opacity:.7;text-align:center;margin-bottom:3px}
.wave-display{width:100%;height:36px;background:#0a0a0a;border:1px solid #1a1a1a;border-radius:2px;margin-bottom:4px;position:relative;overflow:hidden}
.wave-display canvas{display:block}
.lane-knobs{display:flex;flex-wrap:wrap;justify-content:center;gap:3px;flex:1}
.lane-sync{display:flex;justify-content:center;margin-bottom:3px}
#bottom{height:60px;border-top:1px solid #1a1a1a;display:flex;align-items:center;padding:0 8px;gap:6px;flex-shrink:0;background:#0a0a0a}
.bottom-knobs{display:flex;gap:6px;align-items:center}
.bottom-sep{width:1px;height:40px;background:#1a1a1a}
#grid-overlay{position:absolute;top:0;left:0;width:100%;height:100%;pointer-events:none}
.shape-tog{display:flex;border:1px solid #333;border-radius:2px;overflow:hidden;margin-bottom:3px}
.shape-tog button{padding:1px 3px;background:transparent;border:none;font-size:6px;cursor:pointer;color:#555;min-width:14px}
.shape-tog button.on{background:#00ffe0;color:#000}
</style></head>
<body>
<div id="main">
  <div id="title-bar">
    <span id="plugin-name">LFOToolPro</span>
    <span id="plugin-sub">4× Tempo-Synced LFO Modulator</span>
  </div>
  <div id="lanes">
    <!-- Lane 1: Volume -->
    <div class="lane" id="lane1">
      <div class="lane-label">Volume</div>
      <div class="wave-display"><canvas id="wave1" width="120" height="36"></canvas></div>
      <div class="shape-tog" id="shp1">
        <button title="Sine">∿</button>
        <button title="Sq">⊓</button>
        <button title="Saw">/</button>
        <button title="Tri">∧</button>
      </div>
      <div class="lane-sync">
        <div class="tog" id="sync1tog">
          <button>Hz</button><button class="on">Sync</button>
        </div>
      </div>
      <div class="lane-knobs">
        <div class="kw" id="k1"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">1.0</div><div class="kl">Rate</div></div>
        <div class="kw" id="k3"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">0.50</div><div class="kl">Depth</div></div>
        <div class="kw" id="k4"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">0°</div><div class="kl">Phase</div></div>
        <div class="kw" id="k5"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">1.00</div><div class="kl">GDepth</div></div>
      </div>
    </div>
    <!-- Lane 2: Filter -->
    <div class="lane" id="lane2">
      <div class="lane-label">Filter</div>
      <div class="wave-display"><canvas id="wave2" width="120" height="36"></canvas></div>
      <div class="shape-tog" id="shp2">
        <button title="Sine">∿</button>
        <button title="Sq">⊓</button>
        <button title="Saw">/</button>
        <button title="Tri">∧</button>
      </div>
      <div class="lane-sync">
        <div class="tog" id="sync2tog">
          <button>Hz</button><button class="on">Sync</button>
        </div>
      </div>
      <div class="lane-knobs">
        <div class="kw" id="k7"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">1.0</div><div class="kl">Rate</div></div>
        <div class="kw" id="k9"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">0.50</div><div class="kl">Depth</div></div>
        <div class="kw" id="k10"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">0°</div><div class="kl">Phase</div></div>
        <div class="kw" id="k11"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">1.00</div><div class="kl">GDepth</div></div>
      </div>
    </div>
    <!-- Lane 3: Pan -->
    <div class="lane" id="lane3">
      <div class="lane-label">Pan</div>
      <div class="wave-display"><canvas id="wave3" width="120" height="36"></canvas></div>
      <div class="shape-tog" id="shp3">
        <button title="Sine">∿</button>
        <button title="Sq">⊓</button>
        <button title="Saw">/</button>
        <button title="Tri">∧</button>
      </div>
      <div class="lane-sync">
        <div class="tog" id="sync3tog">
          <button>Hz</button><button class="on">Sync</button>
        </div>
      </div>
      <div class="lane-knobs">
        <div class="kw" id="k14"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">1.0</div><div class="kl">Rate</div></div>
        <div class="kw" id="k16"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">0.50</div><div class="kl">Depth</div></div>
        <div class="kw" id="k17"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">0°</div><div class="kl">Phase</div></div>
        <div class="kw" id="k18"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">1.00</div><div class="kl">GDepth</div></div>
      </div>
    </div>
    <!-- Lane 4: Reverb -->
    <div class="lane" id="lane4">
      <div class="lane-label">Reverb</div>
      <div class="wave-display"><canvas id="wave4" width="120" height="36"></canvas></div>
      <div class="shape-tog" id="shp4">
        <button title="Sine">∿</button>
        <button title="Sq">⊓</button>
        <button title="Saw">/</button>
        <button title="Tri">∧</button>
      </div>
      <div class="lane-sync">
        <div class="tog" id="sync4tog">
          <button>Hz</button><button class="on">Sync</button>
        </div>
      </div>
      <div class="lane-knobs">
        <div class="kw" id="k21"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">1.0</div><div class="kl">Rate</div></div>
        <div class="kw" id="k23"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">0.50</div><div class="kl">Depth</div></div>
        <div class="kw" id="k24"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">0°</div><div class="kl">Phase</div></div>
        <div class="kw" id="k25"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">1.00</div><div class="kl">GDepth</div></div>
      </div>
    </div>
  </div>
  <!-- Bottom strip -->
  <div id="bottom">
    <div class="sec" style="color:#00ffe0;opacity:.5;writing-mode:horizontal-tb;font-size:7px;">GLOBAL</div>
    <div class="bottom-knobs">
      <div class="kw" id="k12"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">8kHz</div><div class="kl">FilterBase</div></div>
      <div class="kw" id="k26"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">0.50</div><div class="kl">RvbSize</div></div>
      <div class="kw" id="k27"><div class="knob-ring"><div class="dot" style="background:#00ffe0"></div></div><div class="kv">0.50</div><div class="kl">Damp</div></div>
    </div>
    <div class="bottom-sep"></div>
    <div class="bottom-knobs">
      <div class="kw" id="k28"><div class="knob-ring"><div class="dot" style="background:#ff6040"></div></div><div class="kv">0.0dB</div><div class="kl">OutTrim</div></div>
    </div>
    <div class="bottom-sep"></div>
    <!-- Mini LFO phase indicators -->
    <div style="display:flex;flex-direction:column;gap:3px;margin-left:4px">
      <div style="font-size:7px;color:#444;letter-spacing:2px">LFO PHASE</div>
      <div style="display:flex;gap:4px" id="phase-indicators">
        <canvas id="pi1" width="22" height="22"></canvas>
        <canvas id="pi2" width="22" height="22"></canvas>
        <canvas id="pi3" width="22" height="22"></canvas>
        <canvas id="pi4" width="22" height="22"></canvas>
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

// Shape state per LFO
var lfoShapes=[0,0,0,0];
var lfoPhases=[0,0,0,0];
var lfoDepths=[0.5,0.5,0.5,0.5];

function drawWave(canvasId,shape,depth){
  var c=document.getElementById(canvasId);
  if(!c)return;
  var ctx=c.getContext('2d');
  var w=c.width,h=c.height;
  ctx.clearRect(0,0,w,h);
  // grid
  ctx.strokeStyle='#1a1a1a';
  ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(0,h/2);ctx.lineTo(w,h/2);ctx.stroke();
  // fill
  var grad=ctx.createLinearGradient(0,0,0,h);
  grad.addColorStop(0,'rgba(0,255,224,0.3)');
  grad.addColorStop(1,'rgba(0,255,224,0.0)');
  ctx.fillStyle=grad;
  ctx.strokeStyle='#00ffe0';
  ctx.lineWidth=1.5;
  ctx.beginPath();
  var pts=[];
  for(var i=0;i<w;i++){
    var t=i/w;
    var v;
    if(shape===0) v=Math.sin(t*Math.PI*2);
    else if(shape===1) v=t<0.5?1:-1;
    else if(shape===2) v=2*t-1;
    else v=t<0.5?(4*t-1):(3-4*t);
    pts.push(v);
  }
  // scale by depth (visual only)
  var amp=(h/2-3)*Math.max(0.1,depth);
  ctx.moveTo(0,h/2-pts[0]*amp);
  for(var i=1;i<w;i++) ctx.lineTo(i,h/2-pts[i]*amp);
  ctx.stroke();
  // fill below
  ctx.lineTo(w,h/2);ctx.lineTo(0,h/2);ctx.closePath();ctx.fill();
}

function drawPhaseIndicator(canvasId,phaseNorm){
  var c=document.getElementById(canvasId);
  if(!c)return;
  var ctx=c.getContext('2d');
  var w=c.width,h=c.height,r=9;
  ctx.clearRect(0,0,w,h);
  ctx.strokeStyle='#222';
  ctx.lineWidth=1.5;
  ctx.beginPath();ctx.arc(w/2,h/2,r,0,Math.PI*2);ctx.stroke();
  var angle=(phaseNorm*Math.PI*2)-Math.PI/2;
  ctx.strokeStyle='#00ffe0';
  ctx.lineWidth=2;
  ctx.beginPath();
  ctx.moveTo(w/2,h/2);
  ctx.lineTo(w/2+Math.cos(angle)*r,h/2+Math.sin(angle)*r);
  ctx.stroke();
  ctx.fillStyle='#00ffe0';
  ctx.beginPath();ctx.arc(w/2+Math.cos(angle)*r,h/2+Math.sin(angle)*r,2,0,Math.PI*2);ctx.fill();
}

function setupShapeToggle(togId,lfoIdx,waveCanvasId){
  var el=document.getElementById(togId);
  var bs=el.querySelectorAll('button');
  bs[0].classList.add('on');
  bs.forEach(function(b,i){
    b.onclick=function(){
      bs.forEach(function(x){x.classList.remove('on')});
      b.classList.add('on');
      lfoShapes[lfoIdx]=i;
      var norm=i/3;
      _xhr([0,6,13,20][lfoIdx],norm);
      drawWave(waveCanvasId,i,lfoDepths[lfoIdx]);
    };
  });
}

function setupSyncToggle(togId,paramIdx){
  var el=document.getElementById(togId);
  var bs=el.querySelectorAll('button');
  bs.forEach(function(b,i){
    b.onclick=function(){
      bs.forEach(function(x){x.classList.remove('on')});
      b.classList.add('on');
      _xhr(paramIdx,i===1?1:0);
    };
  });
}

window.onload=function(){
  // Resize wave canvases to fit lane width
  var lanes=document.querySelectorAll('.lane');
  for(var i=0;i<4;i++){
    var c=document.getElementById('wave'+(i+1));
    var wd=lanes[i].querySelector('.wave-display');
    c.width=wd.offsetWidth||120;
    c.height=36;
  }

  // Shape toggles (p0, p6, p13, p20)
  setupShapeToggle('shp1',0,'wave1');
  setupShapeToggle('shp2',1,'wave2');
  setupShapeToggle('shp3',2,'wave3');
  setupShapeToggle('shp4',3,'wave4');

  // Sync toggles (p2, p8, p15, p22)
  setupSyncToggle('sync1tog',2);
  setupSyncToggle('sync2tog',8);
  setupSyncToggle('sync3tog',15);
  setupSyncToggle('sync4tog',22);

  var rateLogFmt=function(v){return v<1?'1/'+(Math.round(1/v)):v.toFixed(1);};
  var hzFmt=function(v){return v>=1000?(v/1000).toFixed(1)+'k':Math.round(v)+'Hz';};
  var degFmt=function(v){return Math.round(v)+'°';};
  var dbFmt=function(v){return (v>=0?'+':'')+v.toFixed(1)+'dB';};

  // LFO1: p1=Rate, p3=Depth, p4=Phase, p5=GlobalDepth
  _ks['p1']=knob(document.getElementById('k1'),1,0.0625,32,1.0,rateLogFmt);
  _ks['p3']=knob(document.getElementById('k3'),3,0,1,0.5,function(v){lfoDepths[0]=v;drawWave('wave1',lfoShapes[0],v);return v.toFixed(2);});
  _ks['p4']=knob(document.getElementById('k4'),4,0,360,0,degFmt);
  _ks['p5']=knob(document.getElementById('k5'),5,0,1,1.0,function(v){return v.toFixed(2);});

  // LFO2: p7=Rate, p9=Depth, p10=Phase, p11=GlobalDepth, p12=FilterBase
  _ks['p7']=knob(document.getElementById('k7'),7,0.0625,32,1.0,rateLogFmt);
  _ks['p9']=knob(document.getElementById('k9'),9,0,1,0.5,function(v){lfoDepths[1]=v;drawWave('wave2',lfoShapes[1],v);return v.toFixed(2);});
  _ks['p10']=knob(document.getElementById('k10'),10,0,360,0,degFmt);
  _ks['p11']=knob(document.getElementById('k11'),11,0,1,1.0,function(v){return v.toFixed(2);});

  // LFO3: p14=Rate, p16=Depth, p17=Phase, p18=GlobalDepth
  _ks['p14']=knob(document.getElementById('k14'),14,0.0625,32,1.0,rateLogFmt);
  _ks['p16']=knob(document.getElementById('k16'),16,0,1,0.5,function(v){lfoDepths[2]=v;drawWave('wave3',lfoShapes[2],v);return v.toFixed(2);});
  _ks['p17']=knob(document.getElementById('k17'),17,0,360,0,degFmt);
  _ks['p18']=knob(document.getElementById('k18'),18,0,1,1.0,function(v){return v.toFixed(2);});

  // LFO4: p21=Rate, p23=Depth, p24=Phase, p25=GlobalDepth
  _ks['p21']=knob(document.getElementById('k21'),21,0.0625,32,1.0,rateLogFmt);
  _ks['p23']=knob(document.getElementById('k23'),23,0,1,0.5,function(v){lfoDepths[3]=v;drawWave('wave4',lfoShapes[3],v);return v.toFixed(2);});
  _ks['p24']=knob(document.getElementById('k24'),24,0,360,0,degFmt);
  _ks['p25']=knob(document.getElementById('k25'),25,0,1,1.0,function(v){return v.toFixed(2);});

  // FilterBase p12
  _ks['p12']=knob(document.getElementById('k12'),12,200,18000,8000,hzFmt);
  // ReverbBaseSize p26
  _ks['p26']=knob(document.getElementById('k26'),26,0.1,1.0,0.5,function(v){return v.toFixed(2);});
  // ReverbDamping p27
  _ks['p27']=knob(document.getElementById('k27'),27,0,1,0.5,function(v){return v.toFixed(2);});
  // OutputTrim p28
  _ks['p28']=knob(document.getElementById('k28'),28,-12,12,0,dbFmt);

  // Initial wave draws
  drawWave('wave1',0,0.5);
  drawWave('wave2',0,0.5);
  drawWave('wave3',0,0.5);
  drawWave('wave4',0,0.5);

  // Phase indicators
  drawPhaseIndicator('pi1',0);
  drawPhaseIndicator('pi2',0);
  drawPhaseIndicator('pi3',0);
  drawPhaseIndicator('pi4',0);

  // Animate phase indicators
  var phases=[0,0,0,0];
  var rates=[1,1,1,1];
  function animatePhase(){
    for(var i=0;i<4;i++){
      phases[i]=(phases[i]+0.005*rates[i])%1;
      drawPhaseIndicator('pi'+(i+1),phases[i]);
    }
    requestAnimationFrame(animatePhase);
  }
  animatePhase();
};
</script></body></html>
)HTMLEOF");
}
