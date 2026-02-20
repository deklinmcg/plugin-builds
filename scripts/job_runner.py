#!/usr/bin/env python3
"""
Plugin Builder — job_runner.py
Reads jobs from the queue, generates JUCE plugin source with Claude,
pushes to deklinmcg/plugin-builds → triggers GitHub Actions Mac build.
"""

import os
import json
import time
import shutil
import subprocess
import re
import anthropic
from pathlib import Path

# ── Config ────────────────────────────────────────────────────────────────────

JOBS_DIR      = Path("/home/deklin/plugin-builder/jobs")
GITHUB_PAT    = "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"
GITHUB_REPO   = "deklinmcg/plugin-builds"
CLONE_DIR     = Path("/tmp/plugin-builds-bot")
POLL_INTERVAL = 5  # seconds

client = anthropic.Anthropic(api_key=os.environ["ANTHROPIC_API_KEY"])

# ── Templates ─────────────────────────────────────────────────────────────────

def cmake_template(name):
    return f"""juce_add_plugin({name}
    COMPANY_NAME "APC"
    PLUGIN_MANUFACTURER_CODE Apcc
    PLUGIN_CODE {name[:4].ljust(4,'X')}
    FORMATS VST3 AU
    PRODUCT_NAME "{name}"
    VST3_CATEGORIES Fx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

juce_generate_juce_header({name})

target_sources({name} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions({name} PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries({name} PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
"""

def editor_h_template(name):
    return f"""#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class {name}AudioProcessorEditor : public juce::AudioProcessorEditor
{{
public:
    {name}AudioProcessorEditor ({name}AudioProcessor&);
    ~{name}AudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    {name}AudioProcessor& audioProcessor;
}};
"""

def editor_cpp_template(name):
    return f"""#include "PluginProcessor.h"
#include "PluginEditor.h"

{name}AudioProcessorEditor::{name}AudioProcessorEditor ({name}AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) {{ setSize (400, 300); }}

{name}AudioProcessorEditor::~{name}AudioProcessorEditor() {{}}
void {name}AudioProcessorEditor::paint (juce::Graphics& g) {{ g.fillAll (juce::Colours::black); }}
void {name}AudioProcessorEditor::resized() {{}}
"""

# ── Plugin name extraction ─────────────────────────────────────────────────────

def extract_plugin_name(text):
    for line in text.splitlines():
        if line.lower().startswith("plugin name:"):
            raw = line.split(":", 1)[1].strip()
            # CamelCase, no spaces
            return re.sub(r'[^A-Za-z0-9]', '', raw.title().replace(' ', ''))
    return "MyPlugin"

# ── Claude source generation ───────────────────────────────────────────────────

SYSTEM_PROMPT = """You are an expert JUCE C++ audio plugin developer.
Generate complete, compilable PluginProcessor.h and PluginProcessor.cpp files.

STRICT RULES — violation will cause build failures:
1. Use AudioParameterFloat for ALL parameters — registered with addParameter() in constructor
2. createEditor() returns new juce::GenericAudioProcessorEditor(*this)
3. hasEditor() is declared inline in the header: bool hasEditor() const override { return true; }
   Do NOT define hasEditor() again in the .cpp
4. Pre-allocate ALL buffers (AudioBuffer, delay lines etc) in prepareToPlay() — NEVER in processBlock()
5. Implement isBusesLayoutSupported() to accept mono and stereo in/out
6. Handle both mono (totalOut==1) and stereo (totalOut>=2) in processBlock()
7. Do NOT define setParameter()/getParameter() — use AudioParameterFloat->get() in processBlock()
8. getTailLengthSeconds() should return the longest possible reverb/delay tail in seconds

Return ONLY a JSON object like:
{"processor_h": "...full file contents...", "processor_cpp": "...full file contents..."}
No markdown, no explanation, just the JSON."""

def generate_source(plugin_name, apc_prompt):
    print(f"  Generating source for {plugin_name} with Claude...")
    resp = client.messages.create(
        model="claude-opus-4-6",
        max_tokens=6000,
        system=SYSTEM_PROMPT,
        messages=[{
            "role": "user",
            "content": f"Plugin name: {plugin_name}\n\nSpecification:\n{apc_prompt}"
        }]
    )
    raw = resp.content[0].text.strip()
    # Strip markdown code fences if present
    raw = re.sub(r'^```(?:json)?\s*', '', raw)
    raw = re.sub(r'\s*```$', '', raw)
    data = json.loads(raw)
    assert "processor_h" in data and "processor_cpp" in data
    return data

# ── GitHub push ────────────────────────────────────────────────────────────────

def ensure_repo():
    repo_url = f"https://{GITHUB_PAT}@github.com/{GITHUB_REPO}.git"
    if not CLONE_DIR.exists():
        print("  Cloning plugin-builds repo...")
        subprocess.run(["git", "clone", repo_url, str(CLONE_DIR)], check=True)
    else:
        subprocess.run(["git", "-C", str(CLONE_DIR), "remote", "set-url", "origin", repo_url], check=True)
        subprocess.run(["git", "-C", str(CLONE_DIR), "fetch", "origin"], check=True)

def push_to_github(plugin_name, source):
    ensure_repo()
    branch = f"plugin/{plugin_name}"

    # Checkout fresh branch from main
    subprocess.run(
        ["git", "-C", str(CLONE_DIR), "checkout", "-B", branch, "origin/main"],
        check=True
    )

    # Write plugin_name.txt
    (CLONE_DIR / "plugin_name.txt").write_text(plugin_name)

    # Write plugin files
    plugin_dir = CLONE_DIR / "plugin" / plugin_name / "Source"
    plugin_dir.mkdir(parents=True, exist_ok=True)
    cmake_dir = CLONE_DIR / "plugin" / plugin_name

    (cmake_dir / "CMakeLists.txt").write_text(cmake_template(plugin_name))
    (plugin_dir / "PluginProcessor.h").write_text(source["processor_h"])
    (plugin_dir / "PluginProcessor.cpp").write_text(source["processor_cpp"])
    (plugin_dir / "PluginEditor.h").write_text(editor_h_template(plugin_name))
    (plugin_dir / "PluginEditor.cpp").write_text(editor_cpp_template(plugin_name))

    # Git config
    subprocess.run(["git", "-C", str(CLONE_DIR), "config", "user.email", "bot@plugin-builder.xyz"], check=True)
    subprocess.run(["git", "-C", str(CLONE_DIR), "config", "user.name", "Plugin Builder Bot"], check=True)

    # Commit and push
    subprocess.run(["git", "-C", str(CLONE_DIR), "add", "."], check=True)
    subprocess.run(["git", "-C", str(CLONE_DIR), "commit", "-m", f"Build {plugin_name}"], check=True)
    subprocess.run(["git", "-C", str(CLONE_DIR), "push", "origin", branch, "--force"], check=True, capture_output=True)

    print(f"  Pushed to branch {branch} — GitHub Actions building now")
    return branch

# ── Job runner loop ────────────────────────────────────────────────────────────

def process_job(job_dir):
    job_dir = Path(job_dir)
    input_path  = job_dir / "input.json"
    result_path = job_dir / "result.json"

    with open(input_path) as f:
        job = json.load(f)

    apc_prompt  = job.get("text", "")
    plugin_name = extract_plugin_name(apc_prompt)

    print(f"\nProcessing job: {job_dir.name} → {plugin_name}")

    try:
        source = generate_source(plugin_name, apc_prompt)
        branch = push_to_github(plugin_name, source)

        result = {
            "status":    "pushed",
            "plugin":    plugin_name,
            "branch":    branch,
            "repo":      f"https://github.com/{GITHUB_REPO}",
            "actions":   f"https://github.com/{GITHUB_REPO}/actions",
            "message":   f"Build triggered for {plugin_name}. GitHub Actions is building — takes ~3 mins."
        }
    except Exception as e:
        result = {"status": "error", "error": str(e)}
        print(f"  ERROR: {e}")

    with open(result_path, "w") as f:
        json.dump(result, f, indent=2)

    print(f"  Result: {result['status']}")

def main():
    print("Plugin Builder job runner started — polling for jobs...")
    JOBS_DIR.mkdir(parents=True, exist_ok=True)

    while True:
        for job_dir in sorted(JOBS_DIR.iterdir()):
            input_path  = job_dir / "input.json"
            result_path = job_dir / "result.json"
            if input_path.exists() and not result_path.exists():
                process_job(job_dir)
        time.sleep(POLL_INTERVAL)

if __name__ == "__main__":
    main()
