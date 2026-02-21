#include "PluginEditor.h"
#include "PluginProcessor.h"

WornEchoAudioProcessorEditor::WornEchoAudioProcessorEditor (WornEchoAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      webView (juce::WebBrowserComponent::Options{}
          .withResourceProvider (
              [this] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
              {
                  // JUCE 8 may pass full URL or just the path — normalise to path
                  auto path = url.contains ("://")
                      ? url.fromFirstOccurrenceOf ("plugin.local", false, false)
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
                  auto html = WornEchoAudioProcessorEditor::getHTML();
                  std::vector<std::byte> bytes (html.getNumBytesAsUTF8());
                  std::memcpy (bytes.data(), html.toRawUTF8(), bytes.size());
                  return juce::WebBrowserComponent::Resource { std::move (bytes), "text/html" };
              },
              juce::String { "https://plugin.local/" }))
{
    addAndMakeVisible (webView);
    webView.goToURL ("https://plugin.local/");
    setSize (560, 320);
    startTimerHz (30);
}

WornEchoAudioProcessorEditor::~WornEchoAudioProcessorEditor()
{
    stopTimer();
}

void WornEchoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void WornEchoAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void WornEchoAudioProcessorEditor::timerCallback()
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

juce::String WornEchoAudioProcessorEditor::getHTML()
{
    return juce::String (R"HTMLEOF(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WornEcho</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }

  body {
    width: 560px;
    height: 320px;
    overflow: hidden;
    background: #1a1208;
    font-family: 'Courier New', monospace;
    user-select: none;
    cursor: default;
  }

  .plugin-bg {
    width: 560px;
    height: 320px;
    background:
      radial-gradient(ellipse at 50% 0%, #2e1f08 0%, #1a1208 60%),
      repeating-linear-gradient(
        90deg,
        transparent,
        transparent 59px,
        rgba(255,180,60,0.03) 60px
      );
    position: relative;
    border: 1px solid #3d2a0a;
  }

  /* Tape reel texture overlay */
  .plugin-bg::before {
    content: '';
    position: absolute;
    inset: 0;
    background-image:
      repeating-linear-gradient(
        0deg,
        transparent,
        transparent 3px,
        rgba(0,0,0,0.08) 4px
      );
    pointer-events: none;
    z-index: 0;
  }

  .header {
    position: relative;
    z-index: 1;
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 8px 16px 6px;
    border-bottom: 1px solid #3d2a0a;
    background: linear-gradient(180deg, #221508 0%, transparent 100%);
  }

  .plugin-name {
    font-size: 22px;
    font-weight: bold;
    letter-spacing: 4px;
    color: #d4820a;
    text-shadow: 0 0 12px rgba(212,130,10,0.6), 0 0 24px rgba(212,130,10,0.2);
    font-family: 'Courier New', monospace;
  }

  .plugin-type {
    font-size: 9px;
    letter-spacing: 3px;
    color: #7a5520;
    text-transform: uppercase;
    text-align: right;
    line-height: 1.4;
  }

  .plugin-type span {
    display: block;
    color: #a06828;
  }

  .tape-indicator {
    display: flex;
    align-items: center;
    gap: 6px;
  }

  .reel {
    width: 16px;
    height: 16px;
    border: 2px solid #7a5520;
    border-radius: 50%;
    position: relative;
    animation: reelSpin 2s linear infinite;
  }

  .reel::after {
    content: '';
    position: absolute;
    top: 50%;
    left: 50%;
    width: 4px;
    height: 4px;
    background: #d4820a;
    border-radius: 50%;
    transform: translate(-50%, -50%);
  }

  .reel::before {
    content: '';
    position: absolute;
    top: 1px; left: 1px; right: 1px; bottom: 1px;
    border-top: 1px solid #7a5520;
    border-radius: 50%;
  }

  @keyframes reelSpin {
    from { transform: rotate(0deg); }
    to { transform: rotate(360deg); }
  }

  .main-content {
    position: relative;
    z-index: 1;
    display: flex;
    align-items: flex-start;
    padding: 10px 10px 8px;
    gap: 0;
  }

  .knobs-section {
    display: flex;
    flex: 1;
    gap: 0;
    justify-content: space-around;
    align-items: flex-start;
  }

  .knob-group {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
  }

  .knob-container {
    position: relative;
    width: 68px;
    height: 68px;
    cursor: ns-resize;
  }

  .knob-container svg {
    width: 68px;
    height: 68px;
  }

  .knob-label {
    font-size: 8px;
    letter-spacing: 2px;
    color: #7a5520;
    text-transform: uppercase;
    text-align: center;
  }

  .knob-value {
    font-size: 8px;
    color: #a06828;
    text-align: center;
    letter-spacing: 1px;
    min-width: 40px;
  }

  .vu-section {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    padding: 0 8px;
  }

  .vu-label {
    font-size: 7px;
    letter-spacing: 2px;
    color: #7a5520;
    text-transform: uppercase;
  }

  .vu-meters {
    display: flex;
    gap: 5px;
    align-items: flex-end;
    height: 180px;
  }

  .vu-meter-wrap {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 3px;
    height: 100%;
  }

  .vu-channel-label {
    font-size: 7px;
    color: #7a5520;
    letter-spacing: 1px;
  }

  .vu-track {
    width: 14px;
    flex: 1;
    background: #0f0a04;
    border: 1px solid #3d2a0a;
    border-radius: 2px;
    position: relative;
    overflow: hidden;
  }

  .vu-bar {
    position: absolute;
    bottom: 0;
    left: 0;
    right: 0;
    height: 0%;
    background: linear-gradient(
      to top,
      #1a8c1a 0%,
      #4ab84a 40%,
      #d4b80a 70%,
      #d4600a 85%,
      #cc2200 100%
    );
    border-radius: 1px;
    transition: none;
  }

  .vu-tick {
    position: absolute;
    right: -8px;
    width: 6px;
    height: 1px;
    background: #3d2a0a;
  }

  /* Screws */
  .screw {
    position: absolute;
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: radial-gradient(circle at 35% 35%, #5a4010, #2a1a05);
    border: 1px solid #3d2a0a;
    z-index: 2;
  }
  .screw::after {
    content: '';
    position: absolute;
    top: 50%; left: 50%;
    width: 70%; height: 1px;
    background: #1a0e03;
    transform: translate(-50%, -50%) rotate(45deg);
  }

  .screw-tl { top: 5px; left: 5px; }
  .screw-tr { top: 5px; right: 5px; }
  .screw-bl { bottom: 5px; left: 5px; }
  .screw-br { bottom: 5px; right: 5px; }

  .tape-path {
    position: absolute;
    bottom: 18px;
    left: 50%;
    transform: translateX(-50%);
    width: 200px;
    height: 6px;
    background: linear-gradient(180deg, #3d2a0a, #1a1208);
    border-top: 1px solid #5a3c10;
    border-bottom: 1px solid #0f0a04;
    z-index: 0;
    border-radius: 2px;
  }

  .section-divider {
    width: 1px;
    height: 200px;
    background: linear-gradient(180deg, transparent, #3d2a0a 20%, #3d2a0a 80%, transparent);
    margin: 0 4px;
    align-self: center;
  }

  .worn-overlay {
    position: absolute;
    inset: 0;
    pointer-events: none;
    z-index: 3;
    background:
      radial-gradient(ellipse at 20% 80%, rgba(255,180,60,0.04) 0%, transparent 50%),
      radial-gradient(ellipse at 80% 20%, rgba(255,120,20,0.03) 0%, transparent 40%);
  }
</style>
</head>
<body>
<div class="plugin-bg">
  <!-- Screws -->
  <div class="screw screw-tl"></div>
  <div class="screw screw-tr"></div>
  <div class="screw screw-bl"></div>
  <div class="screw screw-br"></div>

  <!-- Header -->
  <div class="header">
    <div class="tape-indicator">
      <div class="reel"></div>
      <div class="plugin-name">WORN ECHO</div>
      <div class="reel" style="animation-direction:reverse"></div>
    </div>
    <div class="plugin-type">
      Stereo Tape Delay<br>
      <span>WornEchoAudioProcessor</span>
    </div>
  </div>

  <!-- Main Content -->
  <div class="main-content">
    <div class="knobs-section" id="knobsSection">
      <!-- Knobs generated by JS -->
    </div>
    <div class="section-divider"></div>
    <!-- VU Meters -->
    <div class="vu-section">
      <div class="vu-label">OUTPUT</div>
      <div class="vu-meters">
        <div class="vu-meter-wrap">
          <div class="vu-channel-label">L</div>
          <div class="vu-track" id="vuTrackL">
            <div class="vu-bar" id="vuBarL"></div>
          </div>
        </div>
        <div class="vu-meter-wrap">
          <div class="vu-channel-label">R</div>
          <div class="vu-track" id="vuTrackR">
            <div class="vu-bar" id="vuBarR"></div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="worn-overlay"></div>
</div>

<script>
  // Parameter definitions
  const params = [
    { index: 0, id: 'p0', label: 'TIME',       value: 0.25, displayFn: v => Math.round(50 + v * 1950) + 'ms', color: '#d4820a' },
    { index: 1, id: 'p1', label: 'FEEDBACK',   value: 0.50, displayFn: v => Math.round(v * 95) + '%',         color: '#c47010' },
    { index: 2, id: 'p2', label: 'WOW',        value: 0.30, displayFn: v => (v * 100).toFixed(0) + '%',       color: '#b86010' },
    { index: 3, id: 'p3', label: 'FLUTTER',    value: 0.25, displayFn: v => (v * 100).toFixed(0) + '%',       color: '#a85020' },
    { index: 4, id: 'p4', label: 'SATURATION', value: 0.40, displayFn: v => (v * 100).toFixed(0) + '%',       color: '#c46820' },
    { index: 5, id: 'p5', label: 'MIX',        value: 0.50, displayFn: v => (v * 100).toFixed(0) + '%',       color: '#d4920a' },
  ];

  // Arc math helpers
  const CX = 34, CY = 34, R = 26;
  const DEG_START = -135, DEG_END = 135;
  const TRACK_COLOR = '#2a1c08';
  const KNOB_STROKE = 5;

  function polarToXY(cx, cy, r, deg) {
    const rad = (deg - 90) * Math.PI / 180;
    return { x: cx + r * Math.cos(rad), y: cy + r * Math.sin(rad) };
  }

  function arcPath(cx, cy, r, startDeg, endDeg) {
    const s = polarToXY(cx, cy, r, startDeg);
    const e = polarToXY(cx, cy, r, endDeg);
    const large = (endDeg - startDeg) > 180 ? 1 : 0;
    return `M ${s.x} ${s.y} A ${r} ${r} 0 ${large} 1 ${e.x} ${e.y}`;
  }

  function valueToDeg(v) {
    return DEG_START + v * (DEG_END - DEG_START);
  }

  function makeSVG(param) {
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('width', '68');
    svg.setAttribute('height', '68');
    svg.setAttribute('viewBox', '0 0 68 68');

    // Subtle glow bg
    const defs = document.createElementNS('http://www.w3.org/2000/svg', 'defs');
    const filter = document.createElementNS('http://www.w3.org/2000/svg', 'filter');
    filter.setAttribute('id', 'glow_' + param.index);
    const feGlow = document.createElementNS('http://www.w3.org/2000/svg', 'feDropShadow');
    feGlow.setAttribute('dx', '0'); feGlow.setAttribute('dy', '0');
    feGlow.setAttribute('stdDeviation', '2');
    feGlow.setAttribute('flood-color', param.color);
    feGlow.setAttribute('flood-opacity', '0.7');
    filter.appendChild(feGlow);
    defs.appendChild(filter);
    svg.appendChild(defs);

    // Knob body (dark circle)
    const body = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    body.setAttribute('cx', CX); body.setAttribute('cy', CY); body.setAttribute('r', '20');
    body.setAttribute('fill', 'url(#knobGrad_' + param.index + ')');
    body.setAttribute('stroke', '#3d2a0a'); body.setAttribute('stroke-width', '1');

    const defs2 = document.createElementNS('http://www.w3.org/2000/svg', 'defs');
    const grad = document.createElementNS('http://www.w3.org/2000/svg', 'radialGradient');
    grad.setAttribute('id', 'knobGrad_' + param.index);
    grad.setAttribute('cx', '40%'); grad.setAttribute('cy', '35%'); grad.setAttribute('r', '60%');
    const stop1 = document.createElementNS('http://www.w3.org/2000/svg', 'stop');
    stop1.setAttribute('offset', '0%'); stop1.setAttribute('stop-color', '#3d2a0a');
    const stop2 = document.createElementNS('http://www.w3.org/2000/svg', 'stop');
    stop2.setAttribute('offset', '100%'); stop2.setAttribute('stop-color', '#130d03');
    grad.appendChild(stop1); grad.appendChild(stop2);
    defs2.appendChild(grad);
    svg.appendChild(defs2);
    svg.appendChild(body);

    // Track arc
    const trackPath = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    trackPath.setAttribute('d', arcPath(CX, CY, R, DEG_START, DEG_END));
    trackPath.setAttribute('stroke', TRACK_COLOR);
    trackPath.setAttribute('stroke-width', KNOB_STROKE);
    trackPath.setAttribute('fill', 'none');
    trackPath.setAttribute('stroke-linecap', 'round');
    svg.appendChild(trackPath);

    // Value arc
    const valPath = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    valPath.setAttribute('id', 'arc_' + param.index);
    valPath.setAttribute('stroke', param.color);
    valPath.setAttribute('stroke-width', KNOB_STROKE);
    valPath.setAttribute('fill', 'none');
    valPath.setAttribute('stroke-linecap', 'round');
    valPath.setAttribute('filter', 'url(#glow_' + param.index + ')');
    svg.appendChild(valPath);

    // Indicator dot
    const dot = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    dot.setAttribute('id', 'dot_' + param.index);
    dot.setAttribute('r', '2.5');
    dot.setAttribute('fill', '#f0c060');
    svg.appendChild(dot);

    updateArc(param, svg);
    return svg;
  }

  function updateArc(param, svgEl) {
    const v = param.value;
    const endDeg = valueToDeg(v);
    const startDeg = DEG_START;

    const arcEl = (svgEl || document).getElementById('arc_' + param.index);
    const dotEl = (svgEl || document).getElementById('dot_' + param.index);
    if (!arcEl || !dotEl) return;

    if (Math.abs(endDeg - startDeg) < 0.5) {
      arcEl.setAttribute('d', '');
    } else {
      arcEl.setAttribute('d', arcPath(CX, CY, R, startDeg, endDeg));
    }

    const dotPos = polarToXY(CX, CY, R, endDeg);
    dotEl.setAttribute('cx', dotPos.x);
    dotEl.setAttribute('cy', dotPos.y);
  }

  // Build knob UI
  function buildKnobs() {
    const section = document.getElementById('knobsSection');
    params.forEach(param => {
      const group = document.createElement('div');
      group.className = 'knob-group';

      const container = document.createElement('div');
      container.className = 'knob-container';
      container.setAttribute('data-index', param.index);

      const svg = makeSVG(param);
      container.appendChild(svg);

      const label = document.createElement('div');
      label.className = 'knob-label';
      label.textContent = param.label;

      const valDisp = document.createElement('div');
      valDisp.className = 'knob-value';
      valDisp.id = 'val_' + param.index;
      valDisp.textContent = param.displayFn(param.value);

      group.appendChild(container);
      group.appendChild(label);
      group.appendChild(valDisp);
      section.appendChild(group);

      // Drag logic
      let dragging = false;
      let startY = 0;
      let startVal = 0;

      container.addEventListener('mousedown', e => {
        dragging = true;
        startY = e.clientY;
        startVal = param.value;
        e.preventDefault();
      });

      container.addEventListener('dblclick', () => {
        // Reset to default on double click
        const defaults = [0.25, 0.50, 0.30, 0.25, 0.40, 0.50];
        param.value = defaults[param.index];
        updateArc(param);
        document.getElementById('val_' + param.index).textContent = param.displayFn(param.value);
        sendParam(param);
      });

      document.addEventListener('mousemove', e => {
        if (!dragging) return;
        const dy = startY - e.clientY;
        const delta = dy / 180;
        param.value = Math.max(0, Math.min(1, startVal + delta));
        updateArc(param);
        document.getElementById('val_' + param.index).textContent = param.displayFn(param.value);
        sendParam(param);
      });

      document.addEventListener('mouseup', () => {
        dragging = false;
      });

      // Scroll wheel
      container.addEventListener('wheel', e => {
        e.preventDefault();
        const delta = e.deltaY < 0 ? 0.02 : -0.02;
        param.value = Math.max(0, Math.min(1, param.value + delta));
        updateArc(param);
        document.getElementById('val_' + param.index).textContent = param.displayFn(param.value);
        sendParam(param);
      }, { passive: false });
    });
  }

  function sendParam(param) {
    fetch("https://plugin.local/set/" + param.index + "/" + param.value.toFixed(4)).catch(() => {});
  }

  // VU meter animation
  let vuPhaseL = 0, vuPhaseR = Math.PI * 0.33;
  let vuValL = 0, vuValR = 0;
  let vuTargetL = 0, vuTargetR = 0;

  function animateVU() {
    const mixParam = params[5];
    const feedbackParam = params[1];
    const base = mixParam.value * 0.85;
    const fbBoost = feedbackParam.value * 0.15;

    const t = Date.now() * 0.001;
    vuPhaseL += 0.05;
    vuPhaseR += 0.047;

    const wobbleL = Math.sin(t * 3.1) * 0.06 + Math.sin(t * 7.3) * 0.03;
    const wobbleR = Math.sin(t * 3.4 + 0.8) * 0.06 + Math.sin(t * 6.9) * 0.03;

    vuTargetL = Math.max(0, Math.min(1, base + fbBoost + wobbleL));
    vuTargetR = Math.max(0, Math.min(1, base + fbBoost + wobbleR));

    // Smooth attack/decay
    const attack = 0.25, decay = 0.07;
    vuValL += (vuTargetL > vuValL) ? (vuTargetL - vuValL) * attack : (vuTargetL - vuValL) * decay;
    vuValR += (vuTargetR > vuValR) ? (vuTargetR - vuValR) * attack : (vuTargetR - vuValR) * decay;

    document.getElementById('vuBarL').style.height = (vuValL * 100).toFixed(1) + '%';
    document.getElementById('vuBarR').style.height = (vuValR * 100).toFixed(1) + '%';

    requestAnimationFrame(animateVU);
  }

  // Public API for host
  window.updateParams = function(values) {
    // values: { p0: 0.5, p1: 0.3, ... }
    params.forEach(param => {
      const key = 'p' + param.index;
      if (values.hasOwnProperty(key)) {
        param.value = Math.max(0, Math.min(1, parseFloat(values[key])));
        updateArc(param);
        const valEl = document.getElementById('val_' + param.index);
        if (valEl) valEl.textContent = param.displayFn(param.value);
      }
    });
  };

  // Init
  buildKnobs();
  animateVU();

  // Reel speed tied to time param
  function updateReelSpeed() {
    const timeVal = params[0].value;
    const speed = 0.5 + (1 - timeVal) * 3;
    document.querySelectorAll('.reel').forEach((r, i) => {
      r.style.animationDuration = speed + 's';
    });
    requestAnimationFrame(updateReelSpeed);
  }
  updateReelSpeed();
</script>
</body>
</html>
)HTMLEOF");
}
