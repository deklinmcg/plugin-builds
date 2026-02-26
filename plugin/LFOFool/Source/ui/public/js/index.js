import * as Juce from "./juce/index.js";

// ── LFO colours ───────────────────────────────────────────────────────────────
const LFO_COLORS = ["#00FFFF", "#FFC300", "#FF00FF", "#00FF88"];

// ── Canvas waveform animation ─────────────────────────────────────────────────
const canvas = document.getElementById("waveform-canvas");
const ctx    = canvas.getContext("2d");
const W = 800, H = 180;

// Per-LFO state tracked locally for waveform rendering
const waveState = [
  { phase: 0, freq: 1.0,  shape: 0, amp: 0.7  },
  { phase: 0.5, freq: 1.7, shape: 1, amp: 0.55 },
  { phase: 1.2, freq: 0.6, shape: 2, amp: 0.6  },
  { phase: 0.8, freq: 2.3, shape: 3, amp: 0.4  },
];

function shapeY(phase, shape) {
  const p = ((phase % 1) + 1) % 1;
  switch (shape) {
    case 0: return Math.sin(p * Math.PI * 2);
    case 1: return p < 0.5 ? p * 4 - 1 : 3 - p * 4;
    case 2: return p < 0.5 ? 1 : -1;
    case 3: return p * 2 - 1;
    case 4: return 1 - p * 2;
    case 5: return Math.sin(p * Math.PI * 2);  // fallback for random
    default: return Math.sin(p * Math.PI * 2);
  }
}

let animTime = 0;

function drawWaveforms() {
  ctx.clearRect(0, 0, W, H);

  // Grid
  ctx.strokeStyle = "#1E1E2E";
  ctx.lineWidth = 1;
  for (let i = 1; i < 4; i++) {
    ctx.beginPath();
    ctx.moveTo(0, H * i / 4);
    ctx.lineTo(W, H * i / 4);
    ctx.stroke();
  }
  ctx.strokeStyle = "#2A2A3E";
  ctx.beginPath(); ctx.moveTo(0, H / 2); ctx.lineTo(W, H / 2); ctx.stroke();

  // LFO curves
  waveState.forEach((lfo, i) => {
    ctx.strokeStyle = LFO_COLORS[i];
    ctx.lineWidth = 1.5;
    ctx.globalAlpha = 0.75;
    ctx.beginPath();
    for (let x = 0; x <= W; x++) {
      const p = (x / W * lfo.freq * 0.3 + animTime * lfo.freq * 0.3 + lfo.phase) % 1;
      const y = (H / 2) - shapeY(p, lfo.shape) * lfo.amp * (H / 2 - 16);
      x === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.stroke();
  });
  ctx.globalAlpha = 1;

  animTime += 0.005;
  requestAnimationFrame(drawWaveforms);
}
drawWaveforms();

// ── JUCE parameter states ─────────────────────────────────────────────────────
// Map knob rotation (-135° to +135°) ↔ normalised value (0.0 to 1.0)
const ROT_MIN = -135, ROT_MAX = 135;

function normToRot(norm) {
  return ROT_MIN + norm * (ROT_MAX - ROT_MIN);
}
function rotToNorm(rot) {
  return (rot - ROT_MIN) / (ROT_MAX - ROT_MIN);
}
function clampRot(r) {
  return Math.max(ROT_MIN, Math.min(ROT_MAX, r));
}

function setKnobRot(knob, rot) {
  knob.style.transform = `rotate(${rot}deg)`;
}

// Initialise JUCE slider state for a knob element
function bindKnob(knob) {
  const paramId = knob.dataset.param;
  if (!paramId) return;

  const state = Juce.getSliderState(paramId);

  // JUCE → UI
  state.addListenerCallback((norm) => {
    setKnobRot(knob, normToRot(norm));
    // Update waveform speed for rate knobs
    updateWaveFreq(paramId, norm);
  });

  // Set initial position
  setKnobRot(knob, normToRot(state.getNormalisedValue()));
  updateWaveFreq(paramId, state.getNormalisedValue());

  // UI → JUCE (drag)
  let dragging = false, startY = 0, startRot = 0;

  function getRot() {
    const t = knob.style.transform || "rotate(0deg)";
    return parseFloat(t.replace("rotate(", "").replace("deg)", "")) || 0;
  }

  knob.addEventListener("mousedown", (e) => {
    dragging = true;
    startY   = e.clientY;
    startRot = getRot();
    e.preventDefault();
  });

  document.addEventListener("mousemove", (e) => {
    if (!dragging) return;
    const rot  = clampRot(startRot + (startY - e.clientY));
    setKnobRot(knob, rot);
    state.setNormalisedValue(rotToNorm(rot));
    updateWaveFreq(paramId, rotToNorm(rot));
  });

  document.addEventListener("mouseup", () => { dragging = false; });
}

// Update waveform animation freq when a rate knob changes
function updateWaveFreq(paramId, norm) {
  const match = paramId.match(/^lfo(\d)_rate$/);
  if (!match) return;
  const idx = parseInt(match[1]) - 1;
  // Rate param uses skewed range 0.01–20 Hz; match the canvas display range
  waveState[idx].freq = Math.pow(10, (norm - 0.5) * 2);  // approx log mapping
}

// ── Bind all knobs ────────────────────────────────────────────────────────────
document.querySelectorAll(".knob[data-param]").forEach(bindKnob);

// ── Bind shape selectors ──────────────────────────────────────────────────────
for (let n = 1; n <= 4; n++) {
  const container = document.getElementById(`shape-${n}`);
  const paramId   = `lfo${n}_shape`;
  const color     = LFO_COLORS[n - 1];
  const state     = Juce.getSliderState(paramId);

  function applyShape(normVal) {
    const shapeIdx = Math.round(normVal * 6);  // 0–6 mapped 0–1 in APVTS
    waveState[n - 1].shape = shapeIdx;
    container.querySelectorAll(".shape-btn").forEach((btn, i) => {
      const active = (i === shapeIdx);
      btn.classList.toggle("active", active);
      btn.querySelector("svg").setAttribute("stroke", active ? color : "var(--text-dim)");
    });
  }

  state.addListenerCallback(applyShape);
  applyShape(state.getNormalisedValue());

  container.querySelectorAll(".shape-btn").forEach((btn, i) => {
    btn.addEventListener("click", () => {
      state.setNormalisedValue(i / 6);
    });
  });
}

// ── Bind enable toggles ───────────────────────────────────────────────────────
for (let n = 1; n <= 4; n++) {
  const toggle  = document.getElementById(`lfo${n}-enabled`);
  const paramId = `lfo${n}_enabled`;
  const state   = Juce.getToggleState(paramId);

  state.addListenerCallback((val) => {
    toggle.classList.toggle("on", val);
  });
  toggle.classList.toggle("on", state.getValue());

  toggle.addEventListener("click", () => {
    state.setValue(!state.getValue());
  });
}

// ── Bind target dropdowns ─────────────────────────────────────────────────────
for (let n = 1; n <= 4; n++) {
  const sel     = document.getElementById(`target-${n}`);
  const paramId = `lfo${n}_target`;
  const state   = Juce.getSliderState(paramId);

  state.addListenerCallback((norm) => {
    sel.selectedIndex = Math.round(norm * 4);
  });
  sel.selectedIndex = Math.round(state.getNormalisedValue() * 4);

  sel.addEventListener("change", () => {
    state.setNormalisedValue(parseInt(sel.value) / 4);
  });
}

// ── BPM Sync button ───────────────────────────────────────────────────────────
const syncBtn   = document.getElementById("sync-btn");
const syncState = Juce.getToggleState("bpm_sync");

syncState.addListenerCallback((val) => {
  syncBtn.classList.toggle("on", val);
});
syncBtn.classList.toggle("on", syncState.getValue());

syncBtn.addEventListener("click", () => {
  syncState.setValue(!syncState.getValue());
});
