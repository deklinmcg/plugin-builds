#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

static const char* const kJuceBridgeJS = R"_JUCE_END_(/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

if (
  typeof window.__JUCE__ !== "undefined" &&
  typeof window.__JUCE__.getAndroidUserScripts !== "undefined" &&
  typeof window.inAndroidUserScriptEval === "undefined"
) {
  window.inAndroidUserScriptEval = true;
  eval(window.__JUCE__.getAndroidUserScripts());
  delete window.inAndroidUserScriptEval;
}

{
  if (typeof window.__JUCE__ === "undefined") {
    console.warn(
      "The 'window.__JUCE__' object is undefined." +
        " Native integration features will not work." +
        " Defining a placeholder 'window.__JUCE__' object."
    );

    window.__JUCE__ = {
      postMessage: function () {},
    };
  }

  if (typeof window.__JUCE__.initialisationData === "undefined") {
    window.__JUCE__.initialisationData = {
      __juce__platform: [],
      __juce__functions: [],
      __juce__registeredGlobalEventIds: [],
      __juce__sliders: [],
      __juce__toggles: [],
      __juce__comboBoxes: [],
    };
  }

  class ListenerList {
    constructor() {
      this.listeners = new Map();
      this.listenerId = 0;
    }

    addListener(fn) {
      const newListenerId = this.listenerId++;
      this.listeners.set(newListenerId, fn);
      return newListenerId;
    }

    removeListener(id) {
      if (this.listeners.has(id)) {
        this.listeners.delete(id);
      }
    }

    callListeners(payload) {
      for (const [, value] of this.listeners) {
        value(payload);
      }
    }
  }

  class EventListenerList {
    constructor() {
      this.eventListeners = new Map();
    }

    addEventListener(eventId, fn) {
      if (!this.eventListeners.has(eventId))
        this.eventListeners.set(eventId, new ListenerList());

      const id = this.eventListeners.get(eventId).addListener(fn);

      return [eventId, id];
    }

    removeEventListener([eventId, id]) {
      if (this.eventListeners.has(eventId)) {
        this.eventListeners.get(eventId).removeListener(id);
      }
    }

    emitEvent(eventId, object) {
      if (this.eventListeners.has(eventId))
        this.eventListeners.get(eventId).callListeners(object);
    }
  }

  class Backend {
    constructor() {
      this.listeners = new EventListenerList();
    }

    addEventListener(eventId, fn) {
      return this.listeners.addEventListener(eventId, fn);
    }

    removeEventListener([eventId, id]) {
      this.listeners.removeEventListener([eventId, id]);
    }

    emitEvent(eventId, object) {
      window.__JUCE__.postMessage(
        JSON.stringify({ eventId: eventId, payload: object })
      );
    }

    emitByBackend(eventId, object) {
      this.listeners.emitEvent(eventId, JSON.parse(object));
    }
  }

  if (typeof window.__JUCE__.backend === "undefined")
    window.__JUCE__.backend = new Backend();
}

/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

class PromiseHandler {
  constructor() {
    this.lastPromiseId = 0;
    this.promises = new Map();

    window.__JUCE__.backend.addEventListener(
      "__juce__complete",
      ({ promiseId, result }) => {
        if (this.promises.has(promiseId)) {
          this.promises.get(promiseId).resolve(result);
          this.promises.delete(promiseId);
        }
      }
    );
  }

  createPromise() {
    const promiseId = this.lastPromiseId++;
    const result = new Promise((resolve, reject) => {
      this.promises.set(promiseId, { resolve: resolve, reject: reject });
    });
    return [promiseId, result];
  }
}

const promiseHandler = new PromiseHandler();

/**
 * Returns a function object that calls a function registered on the JUCE backend and forwards all
 * parameters to it.
 *
 * The provided name should be the same as the name argument passed to
 * WebBrowserComponent::Options.withNativeFunction() on the backend.
 *
 * @param {String} name
 */
function getNativeFunction(name) {
  if (!window.__JUCE__.initialisationData.__juce__functions.includes(name))
    console.warn(
      `Creating native function binding for '${name}', which is unknown to the backend`
    );

  const f = function () {
    const [promiseId, result] = promiseHandler.createPromise();

    window.__JUCE__.backend.emitEvent("__juce__invoke", {
      name: name,
      params: Array.prototype.slice.call(arguments),
      resultId: promiseId,
    });

    return result;
  };

  return f;
}

//==============================================================================

class ListenerList {
  constructor() {
    this.listeners = new Map();
    this.listenerId = 0;
  }

  addListener(fn) {
    const newListenerId = this.listenerId++;
    this.listeners.set(newListenerId, fn);
    return newListenerId;
  }

  removeListener(id) {
    if (this.listeners.has(id)) {
      this.listeners.delete(id);
    }
  }

  callListeners(payload) {
    for (const [, value] of this.listeners) {
      value(payload);
    }
  }
}

const BasicControl_valueChangedEventId = "valueChanged";
const BasicControl_propertiesChangedId = "propertiesChanged";
const SliderControl_sliderDragStartedEventId = "sliderDragStarted";
const SliderControl_sliderDragEndedEventId = "sliderDragEnded";

/**
 * SliderState encapsulates data and callbacks that are synchronised with a WebSliderRelay object
 * on the backend.
 *
 * Use getSliderState() to create a SliderState object. This object will be synchronised with the
 * WebSliderRelay backend object that was created using the same unique name.
 *
 * @param {String} name
 */
class SliderState {
  constructor(name) {
    if (!window.__JUCE__.initialisationData.__juce__sliders.includes(name))
      console.warn(
        "Creating SliderState for '" +
          name +
          "', which is unknown to the backend"
      );

    this.name = name;
    this.identifier = "__juce__slider" + this.name;
    this.scaledValue = 0;
    this.properties = {
      start: 0,
      end: 1,
      skew: 1,
      name: "",
      label: "",
      numSteps: 100,
      interval: 0,
      parameterIndex: -1,
    };
    this.valueChangedEvent = new ListenerList();
    this.propertiesChangedEvent = new ListenerList();

    window.__JUCE__.backend.addEventListener(this.identifier, (event) =>
      this.handleEvent(event)
    );

    window.__JUCE__.backend.emitEvent(this.identifier, {
      eventType: "requestInitialUpdate",
    });
  }

  /**
   * Sets the normalised value of the corresponding backend parameter. This value is always in the
   * [0, 1] range (inclusive).
   *
   * The meaning of this range is the same as in the case of
   * AudioProcessorParameter::getValue() (C++).
   *
   * @param {String} name
   */
  setNormalisedValue(newValue) {
    this.scaledValue = this.snapToLegalValue(
      this.normalisedToScaledValue(newValue)
    );

    window.__JUCE__.backend.emitEvent(this.identifier, {
      eventType: BasicControl_valueChangedEventId,
      value: this.scaledValue,
    });
  }

  /**
   * This function should be called first thing when the user starts interacting with the slider.
   */
  sliderDragStarted() {
    window.__JUCE__.backend.emitEvent(this.identifier, {
      eventType: SliderControl_sliderDragStartedEventId,
    });
  }

  /**
   * This function should be called when the user finished the interaction with the slider.
   */
  sliderDragEnded() {
    window.__JUCE__.backend.emitEvent(this.identifier, {
      eventType: SliderControl_sliderDragEndedEventId,
    });
  }

  /** Internal. */
  handleEvent(event) {
    if (event.eventType == BasicControl_valueChangedEventId) {
      this.scaledValue = event.value;
      this.valueChangedEvent.callListeners();
    }
    if (event.eventType == BasicControl_propertiesChangedId) {
      // eslint-disable-next-line no-unused-vars
      let { eventType: _, ...rest } = event;
      this.properties = rest;
      this.propertiesChangedEvent.callListeners();
    }
  }

  /**
   * Returns the scaled value of the parameter. This corresponds to the return value of
   * NormalisableRange::convertFrom0to1() (C++). This value will differ from a linear
   * [0, 1] range if a non-default NormalisableRange was set for the parameter.
   */
  getScaledValue() {
    return this.scaledValue;
  }

  /**
   * Returns the normalised value of the corresponding backend parameter. This value is always in the
   * [0, 1] range (inclusive).
   *
   * The meaning of this range is the same as in the case of
   * AudioProcessorParameter::getValue() (C++).
   *
   * @param {String} name
   */
  getNormalisedValue() {
    return Math.pow(
      (this.scaledValue - this.properties.start) /
        (this.properties.end - this.properties.start),
      this.properties.skew
    );
  }

  /** Internal. */
  normalisedToScaledValue(normalisedValue) {
    return (
      Math.pow(normalisedValue, 1 / this.properties.skew) *
        (this.properties.end - this.properties.start) +
      this.properties.start
    );
  }

  /** Internal. */
  snapToLegalValue(value) {
    const interval = this.properties.interval;

    if (interval == 0) return value;

    const start = this.properties.start;
    const clamp = (val, min = 0, max = 1) => Math.max(min, Math.min(max, val));

    return clamp(
      start + interval * Math.floor((value - start) / interval + 0.5),
      this.properties.start,
      this.properties.end
    );
  }
}

const sliderStates = new Map();

for (const sliderName of window.__JUCE__.initialisationData.__juce__sliders)
  sliderStates.set(sliderName, new SliderState(sliderName));

/**
 * Returns a SliderState object that is connected to the backend WebSliderRelay object that was
 * created with the same name argument.
 *
 * To register a WebSliderRelay object create one with the right name and add it to the
 * WebBrowserComponent::Options struct using withOptionsFrom.
 *
 * @param {String} name
 */
function getSliderState(name) {
  if (!sliderStates.has(name)) sliderStates.set(name, new SliderState(name));

  return sliderStates.get(name);
}

/**
 * ToggleState encapsulates data and callbacks that are synchronised with a WebToggleRelay object
 * on the backend.
 *
 * Use getToggleState() to create a ToggleState object. This object will be synchronised with the
 * WebToggleRelay backend object that was created using the same unique name.
 *
 * @param {String} name
 */
class ToggleState {
  constructor(name) {
    if (!window.__JUCE__.initialisationData.__juce__toggles.includes(name))
      console.warn(
        "Creating ToggleState for '" +
          name +
          "', which is unknown to the backend"
      );

    this.name = name;
    this.identifier = "__juce__toggle" + this.name;
    this.value = false;
    this.properties = {
      name: "",
      parameterIndex: -1,
    };
    this.valueChangedEvent = new ListenerList();
    this.propertiesChangedEvent = new ListenerList();

    window.__JUCE__.backend.addEventListener(this.identifier, (event) =>
      this.handleEvent(event)
    );

    window.__JUCE__.backend.emitEvent(this.identifier, {
      eventType: "requestInitialUpdate",
    });
  }

  /** Returns the value corresponding to the associated WebToggleRelay's (C++) state. */
  getValue() {
    return this.value;
  }

  /** Informs the backend to change the associated WebToggleRelay's (C++) state. */
  setValue(newValue) {
    this.value = newValue;

    window.__JUCE__.backend.emitEvent(this.identifier, {
      eventType: BasicControl_valueChangedEventId,
      value: this.value,
    });
  }

  /** Internal. */
  handleEvent(event) {
    if (event.eventType == BasicControl_valueChangedEventId) {
      this.value = event.value;
      this.valueChangedEvent.callListeners();
    }
    if (event.eventType == BasicControl_propertiesChangedId) {
      // eslint-disable-next-line no-unused-vars
      let { eventType: _, ...rest } = event;
      this.properties = rest;
      this.propertiesChangedEvent.callListeners();
    }
  }
}

const toggleStates = new Map();

for (const name of window.__JUCE__.initialisationData.__juce__toggles)
  toggleStates.set(name, new ToggleState(name));

/**
 * Returns a ToggleState object that is connected to the backend WebToggleButtonRelay object that was
 * created with the same name argument.
 *
 * To register a WebToggleButtonRelay object create one with the right name and add it to the
 * WebBrowserComponent::Options struct using withOptionsFrom.
 *
 * @param {String} name
 */
function getToggleState(name) {
  if (!toggleStates.has(name)) toggleStates.set(name, new ToggleState(name));

  return toggleStates.get(name);
}

/**
 * ComboBoxState encapsulates data and callbacks that are synchronised with a WebComboBoxRelay object
 * on the backend.
 *
 * Use getComboBoxState() to create a ComboBoxState object. This object will be synchronised with the
 * WebComboBoxRelay backend object that was created using the same unique name.
 *
 * @param {String} name
 */
class ComboBoxState {
  constructor(name) {
    if (!window.__JUCE__.initialisationData.__juce__comboBoxes.includes(name))
      console.warn(
        "Creating ComboBoxState for '" +
          name +
          "', which is unknown to the backend"
      );

    this.name = name;
    this.identifier = "__juce__comboBox" + this.name;
    this.value = 0.0;
    this.properties = {
      name: "",
      parameterIndex: -1,
      choices: [],
    };
    this.valueChangedEvent = new ListenerList();
    this.propertiesChangedEvent = new ListenerList();

    window.__JUCE__.backend.addEventListener(this.identifier, (event) =>
      this.handleEvent(event)
    );

    window.__JUCE__.backend.emitEvent(this.identifier, {
      eventType: "requestInitialUpdate",
    });
  }

  /**
   * Returns the value corresponding to the associated WebComboBoxRelay's (C++) state.
   *
   * This is an index identifying which element of the properties.choices array is currently
   * selected.
   */
  getChoiceIndex() {
    return Math.round(this.value * (this.properties.choices.length - 1));
  }

  /**
   * Informs the backend to change the associated WebComboBoxRelay's (C++) state.
   *
   * This should be called with the index identifying the selected element from the
   * properties.choices array.
   */
  setChoiceIndex(index) {
    const numItems = this.properties.choices.length;
    this.value = numItems > 1 ? index / (numItems - 1) : 0.0;

    window.__JUCE__.backend.emitEvent(this.identifier, {
      eventType: BasicControl_valueChangedEventId,
      value: this.value,
    });
  }

  /** Internal. */
  handleEvent(event) {
    if (event.eventType == BasicControl_valueChangedEventId) {
      this.value = event.value;
      this.valueChangedEvent.callListeners();
    }
    if (event.eventType == BasicControl_propertiesChangedId) {
      // eslint-disable-next-line no-unused-vars
      let { eventType: _, ...rest } = event;
      this.properties = rest;
      this.propertiesChangedEvent.callListeners();
    }
  }
}

const comboBoxStates = new Map();

for (const name of window.__JUCE__.initialisationData.__juce__comboBoxes)
  comboBoxStates.set(name, new ComboBoxState(name));

/**
 * Returns a ComboBoxState object that is connected to the backend WebComboBoxRelay object that was
 * created with the same name argument.
 *
 * To register a WebComboBoxRelay object create one with the right name and add it to the
 * WebBrowserComponent::Options struct using withOptionsFrom.
 *
 * @param {String} name
 */
function getComboBoxState(name) {
  if (!comboBoxStates.has(name))
    comboBoxStates.set(name, new ComboBoxState(name));

  return comboBoxStates.get(name);
}

/**
 * Appends a platform-specific prefix to the path to ensure that a request sent to this address will
 * be received by the backend's ResourceProvider.
 * @param {String} path
 */
function getBackendResourceAddress(path) {
  const platform =
    window.__JUCE__.initialisationData.__juce__platform.length > 0
      ? window.__JUCE__.initialisationData.__juce__platform[0]
      : "";

  if (platform == "windows" || platform == "android")
    return "https://juce.backend/" + path;

  if (platform == "macos" || platform == "ios" || platform == "linux")
    return "juce://juce.backend/" + path;

  console.warn(
    "getBackendResourceAddress() called, but no JUCE native backend is detected."
  );
  return path;
}

/**
 * This helper class is intended to aid the implementation of
 * AudioProcessorEditor::getControlParameterIndex() for editors using a WebView interface.
 *
 * Create an instance of this class and call its handleMouseMove() method in each mousemove event.
 *
 * This class can be used to continuously report the controlParameterIndexAnnotation attribute's
 * value related to the DOM element that is currently under the mouse pointer.
 *
 * This value is defined at all times as follows
 * * the annotation attribute's value for the DOM element directly under the mouse, if it has it,
 * * the annotation attribute's value for the first parent element, that has it,
 * * -1 otherwise.
 *
 * Whenever there is a change in this value, an event is emitted to the frontend with the new value.
 * You can use a ControlParameterIndexReceiver object on the backend to listen to these events.
 *
 * @param {String} controlParameterIndexAnnotation
 */
class ControlParameterIndexUpdater {
  constructor(controlParameterIndexAnnotation) {
    this.controlParameterIndexAnnotation = controlParameterIndexAnnotation;
    this.lastElement = null;
    this.lastControlParameterIndex = null;
  }

  handleMouseMove(event) {
    const currentElement = document.elementFromPoint(
      event.clientX,
      event.clientY
    );

    if (currentElement === this.lastElement) return;
    this.lastElement = currentElement;

    let controlParameterIndex = -1;

    if (currentElement !== null)
      controlParameterIndex = this.#getControlParameterIndex(currentElement);

    if (controlParameterIndex === this.lastControlParameterIndex) return;
    this.lastControlParameterIndex = controlParameterIndex;

    window.__JUCE__.backend.emitEvent(
      "__juce__controlParameterIndexChanged",
      controlParameterIndex
    );
  }

  //==============================================================================
  #getControlParameterIndex(element) {
    const isValidNonRootElement = (e) => {
      return e !== null && e !== document.documentElement;
    };

    while (isValidNonRootElement(element)) {
      if (element.hasAttribute(this.controlParameterIndexAnnotation)) {
        return element.getAttribute(this.controlParameterIndexAnnotation);
      }

      element = element.parentElement;
    }

    return -1;
  }
}

window.getSliderState          = getSliderState;
window.getToggleState          = getToggleState;
window.getComboBoxState        = getComboBoxState;
window.getNativeFunction       = getNativeFunction;
window.getBackendResourceAddress = getBackendResourceAddress;
)_JUCE_END_";

//==============================================================================
JunoBBDAudioProcessorEditor::JunoBBDAudioProcessorEditor (JunoBBDAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    webView = std::make_unique<juce::WebBrowserComponent> (
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withOptionsFrom (modeRelay)
            .withOptionsFrom (rateRelay)
            .withOptionsFrom (depthRelay)
            .withOptionsFrom (driftRelay)
            .withOptionsFrom (toneRelay)
            .withOptionsFrom (widthRelay)
            .withOptionsFrom (mixRelay)
            .withResourceProvider ([this] (const juce::String& url)
                -> std::optional<juce::WebBrowserComponent::Resource>
            {
                return getResource (url);
            })
    );

    addAndMakeVisible (*webView);

    for (auto* p : audioProcessor.getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::mode)
                modeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, modeRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::rate)
                rateAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, rateRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::depth)
                depthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, depthRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::drift)
                driftAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, driftRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::tone)
                toneAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, toneRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::width)
                widthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, widthRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::mix)
                mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, mixRelay);
    }

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    setSize (560, 320);
}

JunoBBDAudioProcessorEditor::~JunoBBDAudioProcessorEditor() {}

void JunoBBDAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void JunoBBDAudioProcessorEditor::resized()
{
    if (webView) webView->setBounds (getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource>
JunoBBDAudioProcessorEditor::getResource (const juce::String& url)
{
    if (url.endsWith ("juce_bridge.js"))
    {
        juce::String js (kJuceBridgeJS);
        std::vector<std::byte> bytes (js.getNumBytesAsUTF8());
        std::memcpy (bytes.data(), js.toRawUTF8(), bytes.size());
        return juce::WebBrowserComponent::Resource { std::move (bytes), "text/javascript" };
    }

    auto html = JunoBBDAudioProcessorEditor::getHTML();
    std::vector<std::byte> bytes (html.getNumBytesAsUTF8());
    std::memcpy (bytes.data(), html.toRawUTF8(), bytes.size());
    return juce::WebBrowserComponent::Resource { std::move (bytes), "text/html" };
}

juce::String JunoBBDAudioProcessorEditor::getHTML()
{
    return juce::String (R"HTMLEOF(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<script src="/juce_bridge.js"></script>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }

  body {
    width: 560px;
    height: 320px;
    overflow: hidden;
    background: #3a3530;
    font-family: 'Arial Narrow', Arial, sans-serif;
    user-select: none;
    cursor: default;
  }

  /* Top orange stripe */
  #top-stripe {
    width: 100%;
    height: 36px;
    background: linear-gradient(180deg, #ff8c00 0%, #e07000 60%, #c85e00 100%);
    display: flex;
    align-items: center;
    padding: 0 14px;
    position: relative;
    box-shadow: 0 2px 8px rgba(0,0,0,0.6);
  }

  #plugin-name {
    font-family: 'Arial', sans-serif;
    font-weight: 900;
    font-size: 20px;
    letter-spacing: 4px;
    color: #fff;
    text-shadow: 0 1px 3px rgba(0,0,0,0.5);
    text-transform: uppercase;
  }

  #plugin-subtitle {
    font-size: 9px;
    color: rgba(255,255,255,0.75);
    letter-spacing: 2px;
    margin-left: 12px;
    margin-top: 2px;
    text-transform: uppercase;
  }

  #vu-meters {
    position: absolute;
    right: 14px;
    top: 4px;
    display: flex;
    gap: 4px;
    align-items: flex-end;
  }

  .vu-track {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 2px;
  }

  .vu-label {
    font-size: 7px;
    color: rgba(255,255,255,0.8);
    letter-spacing: 1px;
    margin-bottom: 1px;
  }

  .vu-bar-container {
    width: 6px;
    height: 22px;
    background: rgba(0,0,0,0.4);
    border-radius: 1px;
    overflow: hidden;
    display: flex;
    flex-direction: column-reverse;
  }

  .vu-bar-fill {
    width: 100%;
    height: 0%;
    border-radius: 1px;
    transition: height 0.05s;
  }

  #vu-l .vu-bar-fill { background: linear-gradient(180deg, #ff3300 0%, #ffaa00 50%, #44ff44 100%); }
  #vu-r .vu-bar-fill { background: linear-gradient(180deg, #ff3300 0%, #ffaa00 50%, #44ff44 100%); }

  /* Main panel */
  #main-panel {
    padding: 10px 14px 8px 14px;
  }

  /* Mode section */
  #mode-section {
    display: flex;
    align-items: center;
    gap: 10px;
    margin-bottom: 14px;
    padding: 10px 12px;
    background: rgba(0,0,0,0.25);
    border-radius: 5px;
    border: 1px solid rgba(0,0,0,0.4);
    box-shadow: inset 0 1px 3px rgba(0,0,0,0.5);
  }

  .mode-section-label {
    font-size: 8px;
    color: #aaa;
    letter-spacing: 2px;
    text-transform: uppercase;
    margin-right: 4px;
    white-space: nowrap;
  }

  .mode-btn {
    width: 64px;
    height: 36px;
    border-radius: 3px;
    border: 2px solid rgba(0,0,0,0.5);
    cursor: pointer;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    position: relative;
    transition: all 0.12s ease;
    box-shadow: 0 2px 6px rgba(0,0,0,0.5), inset 0 1px 1px rgba(255,255,255,0.1);
  }

  #btn-mode1 {
    background: linear-gradient(180deg, #4a1a0a 0%, #2a0e05 100%);
    border-color: #6a2a10;
  }
  #btn-mode1.active {
    background: linear-gradient(180deg, #ff6020 0%, #cc3800 60%, #aa2800 100%);
    border-color: #ff8040;
    box-shadow: 0 0 12px rgba(255,80,20,0.7), 0 2px 4px rgba(0,0,0,0.4), inset 0 1px 1px rgba(255,200,180,0.3);
  }

  #btn-mode2 {
    background: linear-gradient(180deg, #4a2a00 0%, #2a1500 100%);
    border-color: #6a4010;
  }
  #btn-mode2.active {
    background: linear-gradient(180deg, #ff9020 0%, #cc6000 60%, #aa4800 100%);
    border-color: #ffb040;
    box-shadow: 0 0 12px rgba(255,150,20,0.7), 0 2px 4px rgba(0,0,0,0.4), inset 0 1px 1px rgba(255,220,180,0.3);
  }

  .mode-btn-label {
    font-size: 9px;
    font-weight: bold;
    letter-spacing: 1px;
    color: rgba(255,255,255,0.5);
    text-transform: uppercase;
  }
  .mode-btn.active .mode-btn-label { color: #fff; text-shadow: 0 0 6px rgba(255,200,150,0.8); }

  .mode-btn-sub {
    font-size: 7px;
    letter-spacing: 1px;
    color: rgba(255,255,255,0.3);
    margin-top: 1px;
    text-transform: uppercase;
  }
  .mode-btn.active .mode-btn-sub { color: rgba(255,255,255,0.8); }

  /* Crossfader */
  #crossfader-wrap {
    flex: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
  }

  #crossfader-label {
    font-size: 7px;
    color: #999;
    letter-spacing: 2px;
    text-transform: uppercase;
  }

  #crossfader-track {
    width: 100%;
    height: 8px;
    background: linear-gradient(90deg, #331500 0%, #1a0a00 40%, #220a00 60%, #2a1500 100%);
    border-radius: 4px;
    border: 1px solid rgba(0,0,0,0.6);
    position: relative;
    cursor: pointer;
    box-shadow: inset 0 1px 3px rgba(0,0,0,0.8);
  }

  #crossfader-fill {
    position: absolute;
    left: 0;
    top: 0;
    height: 100%;
    background: linear-gradient(90deg, #ff6020, #ff9020);
    border-radius: 4px;
    pointer-events: none;
  }

  #crossfader-thumb {
    position: absolute;
    top: 50%;
    width: 14px;
    height: 20px;
    background: linear-gradient(180deg, #d0c8b8 0%, #a09880 100%);
    border-radius: 2px;
    border: 1px solid #705848;
    transform: translate(-50%, -50%);
    cursor: grab;
    box-shadow: 0 2px 5px rgba(0,0,0,0.7);
    z-index: 5;
  }

  #crossfader-thumb:active { cursor: grabbing; }

  #crossfader-thumb::after {
    content: '';
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: 8px;
    height: 1px;
    background: #705848;
    box-shadow: 0 -2px 0 #705848, 0 2px 0 #705848;
  }

  /* Knobs row */
  #knobs-row {
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
    padding: 0 4px;
  }

  .knob-cell {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    width: 72px;
  }

  .knob-wrap {
    position: relative;
    width: 52px;
    height: 52px;
    cursor: grab;
  }
  .knob-wrap:active { cursor: grabbing; }

  .knob-svg { width: 52px; height: 52px; }

  .knob-value {
    font-size: 8px;
    color: #e8c080;
    letter-spacing: 0.5px;
    text-align: center;
    min-height: 10px;
    font-family: 'Courier New', monospace;
  }

  .knob-label {
    font-size: 9px;
    font-weight: bold;
    letter-spacing: 2px;
    color: #d0c8b8;
    text-transform: uppercase;
    text-align: center;
  }

  .knob-unit {
    font-size: 7px;
    color: #887060;
    letter-spacing: 1px;
    text-transform: uppercase;
  }

  /* Divider */
  #mode-knob-divider {
    width: 100%;
    height: 1px;
    background: linear-gradient(90deg, transparent, rgba(255,140,0,0.3), transparent);
    margin-bottom: 12px;
  }

  /* Panel screws */
  .screw {
    position: absolute;
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: radial-gradient(circle at 35% 35%, #888 0%, #444 60%, #222 100%);
    border: 1px solid #111;
    box-shadow: 0 1px 2px rgba(0,0,0,0.7);
  }

  #screw-tl { top: 40px; left: 6px; }
  #screw-tr { top: 40px; right: 6px; }
  #screw-bl { bottom: 6px; left: 6px; }
  #screw-br { bottom: 6px; right: 6px; }

  /* Roland-style ribs on top stripe sides */
  .stripe-rib {
    position: absolute;
    top: 8px;
    height: 20px;
    width: 3px;
    background: rgba(255,255,255,0.15);
    border-radius: 1px;
  }
  #rib-l { left: 120px; }
  #rib-r { right: 180px; }
</style>
</head>
<body>

<!-- Top stripe -->
<div id="top-stripe">
  <div id="plugin-name">JUNO BBD</div>
  <div id="plugin-subtitle">BBD Stereo Chorus</div>
  <div class="stripe-rib" id="rib-l"></div>
  <div class="stripe-rib" id="rib-r"></div>
  <div id="vu-meters">
    <div class="vu-track" id="vu-l">
      <div class="vu-label">L</div>
      <div class="vu-bar-container"><div class="vu-bar-fill" id="vu-l-fill"></div></div>
    </div>
    <div class="vu-track" id="vu-r">
      <div class="vu-label">R</div>
      <div class="vu-bar-container"><div class="vu-bar-fill" id="vu-r-fill"></div></div>
    </div>
  </div>
</div>

<!-- Screws -->
<div class="screw" id="screw-tl"></div>
<div class="screw" id="screw-tr"></div>
<div class="screw" id="screw-bl"></div>
<div class="screw" id="screw-br"></div>

<!-- Main panel -->
<div id="main-panel">

  <!-- Mode section -->
  <div id="mode-section">
    <div class="mode-section-label">MODE</div>

    <div class="mode-btn" id="btn-mode1">
      <div class="mode-btn-label">I</div>
      <div class="mode-btn-sub">SINGLE</div>
    </div>

    <div id="crossfader-wrap">
      <div id="crossfader-label">BLEND</div>
      <div id="crossfader-track">
        <div id="crossfader-fill"></div>
        <div id="crossfader-thumb"></div>
      </div>
    </div>

    <div class="mode-btn" id="btn-mode2">
      <div class="mode-btn-label">II</div>
      <div class="mode-btn-sub">DUAL</div>
    </div>
  </div>

  <div id="mode-knob-divider"></div>

  <!-- Knobs row -->
  <div id="knobs-row">
    <!-- Rate -->
    <div class="knob-cell" id="cell-rate">
      <div class="knob-wrap" id="knob-rate">
        <svg class="knob-svg" id="svg-rate" viewBox="0 0 52 52"></svg>
      </div>
      <div class="knob-value" id="val-rate">0.50</div>
      <div class="knob-label">RATE</div>
      <div class="knob-unit">Hz</div>
    </div>

    <!-- Depth -->
    <div class="knob-cell" id="cell-depth">
      <div class="knob-wrap" id="knob-depth">
        <svg class="knob-svg" id="svg-depth" viewBox="0 0 52 52"></svg>
      </div>
      <div class="knob-value" id="val-depth">50</div>
      <div class="knob-label">DEPTH</div>
      <div class="knob-unit">%</div>
    </div>

    <!-- Drift -->
    <div class="knob-cell" id="cell-drift">
      <div class="knob-wrap" id="knob-drift">
        <svg class="knob-svg" id="svg-drift" viewBox="0 0 52 52"></svg>
      </div>
      <div class="knob-value" id="val-drift">20</div>
      <div class="knob-label">DRIFT</div>
      <div class="knob-unit">%</div>
    </div>

    <!-- Tone -->
    <div class="knob-cell" id="cell-tone">
      <div class="knob-wrap" id="knob-tone">
        <svg class="knob-svg" id="svg-tone" viewBox="0 0 52 52"></svg>
      </div>
      <div class="knob-value" id="val-tone">50</div>
      <div class="knob-label">TONE</div>
      <div class="knob-unit"></div>
    </div>

    <!-- Width -->
    <div class="knob-cell" id="cell-width">
      <div class="knob-wrap" id="knob-width">
        <svg class="knob-svg" id="svg-width" viewBox="0 0 52 52"></svg>
      </div>
      <div class="knob-value" id="val-width">100</div>
      <div class="knob-label">WIDTH</div>
      <div class="knob-unit">%</div>
    </div>

    <!-- Mix -->
    <div class="knob-cell" id="cell-mix">
      <div class="knob-wrap" id="knob-mix">
        <svg class="knob-svg" id="svg-mix" viewBox="0 0 52 52"></svg>
      </div>
      <div class="knob-value" id="val-mix">50</div>
      <div class="knob-label">MIX</div>
      <div class="knob-unit">%</div>
    </div>
  </div>

</div>

<script>
(function() {
  'use strict';

  // ─── Knob Drawing ───────────────────────────────────────────────────────────
  const CX = 26, CY = 26, R_BG = 20, R_VAL = 20;
  const START_DEG = -135, SWEEP = 270;

  function degToRad(d) { return d * Math.PI / 180; }

  function describeArc(cx, cy, r, startDeg, endDeg) {
    const s = degToRad(startDeg - 90);
    const e = degToRad(endDeg - 90);
    const x1 = cx + r * Math.cos(s);
    const y1 = cy + r * Math.sin(s);
    const x2 = cx + r * Math.cos(e);
    const y2 = cy + r * Math.sin(e);
    const large = (endDeg - startDeg) > 180 ? 1 : 0;
    return `M ${x1} ${y1} A ${r} ${r} 0 ${large} 1 ${x2} ${y2}`;
  }

  function buildKnobSVG(svgEl, normVal, accent) {
    svgEl.innerHTML = '';

    // Shadow circle
    const shadow = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    shadow.setAttribute('cx', CX); shadow.setAttribute('cy', CY + 1); shadow.setAttribute('r', 22);
    shadow.setAttribute('fill', 'rgba(0,0,0,0.5)'); shadow.setAttribute('filter', 'blur(2px)');
    svgEl.appendChild(shadow);

    // Knob body
    const bodyGrad = document.createElementNS('http://www.w3.org/2000/svg', 'radialGradient');
    bodyGrad.setAttribute('id', 'bg-' + svgEl.id);
    bodyGrad.setAttribute('cx', '40%'); bodyGrad.setAttribute('cy', '30%');
    bodyGrad.setAttribute('r', '65%');
    const stops = [['#c8bca8','0%'],['#a09080','40%'],['#786050','80%'],['#584030','100%']];
    stops.forEach(([c,o]) => {
      const st = document.createElementNS('http://www.w3.org/2000/svg','stop');
      st.setAttribute('offset',o); st.setAttribute('stop-color',c); bodyGrad.appendChild(st);
    });
    const defs = document.createElementNS('http://www.w3.org/2000/svg','defs');
    defs.appendChild(bodyGrad); svgEl.appendChild(defs);

    const body = document.createElementNS('http://www.w3.org/2000/svg','circle');
    body.setAttribute('cx',CX); body.setAttribute('cy',CY); body.setAttribute('r',21);
    body.setAttribute('fill',`url(#bg-${svgEl.id})`);
    body.setAttribute('stroke','#2a1a10'); body.setAttribute('stroke-width','1');
    svgEl.appendChild(body);

    // BG track
    const bgPath = document.createElementNS('http://www.w3.org/2000/svg','path');
    bgPath.setAttribute('d', describeArc(CX,CY,R_BG, START_DEG, START_DEG+SWEEP));
    bgPath.setAttribute('stroke','rgba(0,0,0,0.55)');
    bgPath.setAttribute('stroke-width','3.5');
    bgPath.setAttribute('fill','none');
    bgPath.setAttribute('stroke-linecap','round');
    svgEl.appendChild(bgPath);

    // Value arc
    if (normVal > 0.001) {
      const endDeg = START_DEG + normVal * SWEEP;
      const valPath = document.createElementNS('http://www.w3.org/2000/svg','path');
      valPath.setAttribute('d', describeArc(CX,CY,R_VAL, START_DEG, endDeg));
      valPath.setAttribute('stroke', accent || '#ff8020');
      valPath.setAttribute('stroke-width','3.5');
      valPath.setAttribute('fill','none');
      valPath.setAttribute('stroke-linecap','round');
      // Glow filter
      const filter = document.createElementNS('http://www.w3.org/2000/svg','filter');
      filter.setAttribute('id','glow-'+svgEl.id);
      const blur = document.createElementNS('http://www.w3.org/2000/svg','feGaussianBlur');
      blur.setAttribute('stdDeviation','1.5'); blur.setAttribute('result','blurred');
      const merge = document.createElementNS('http://www.w3.org/2000/svg','feMerge');
      const mn1 = document.createElementNS('http://www.w3.org/2000/svg','feMergeNode');
      mn1.setAttribute('in','blurred');
      const mn2 = document.createElementNS('http://www.w3.org/2000/svg','feMergeNode');
      mn2.setAttribute('in','SourceGraphic');
      merge.appendChild(mn1); merge.appendChild(mn2);
      filter.appendChild(blur); filter.appendChild(merge);
      defs.appendChild(filter);
      valPath.setAttribute('filter',`url(#glow-${svgEl.id})`);
      svgEl.appendChild(valPath);
    }

    // Pointer line
    const angle = degToRad((START_DEG + normVal * SWEEP) - 90);
    const px2 = CX + 13 * Math.cos(angle);
    const py2 = CY + 13 * Math.sin(angle);
    const px1 = CX + 6 * Math.cos(angle);
    const py1 = CY + 6 * Math.sin(angle);
    const ptr = document.createElementNS('http://www.w3.org/2000/svg','line');
    ptr.setAttribute('x1',px1); ptr.setAttribute('y1',py1);
    ptr.setAttribute('x2',px2); ptr.setAttribute('y2',py2);
    ptr.setAttribute('stroke','rgba(255,255,255,0.9)'); ptr.setAttribute('stroke-width','1.5');
    ptr.setAttribute('stroke-linecap','round');
    svgEl.appendChild(ptr);

    // Center dot
    const dot = document.createElementNS('http://www.w3.org/2000/svg','circle');
    dot.setAttribute('cx',CX); dot.setAttribute('cy',CY); dot.setAttribute('r',3.5);
    dot.setAttribute('fill','#1a0e08');
    svgEl.appendChild(dot);
  }

  // ─── Knob Drag Logic ────────────────────────────────────────────────────────
  function setupKnob(wrapId, svgId, paramId, displayId, formatFn, accent) {
    const wrap = document.getElementById(wrapId);
    const svg  = document.getElementById(svgId);
    const disp = document.getElementById(displayId);

    let state;
    try { state = getSliderState(paramId); } catch(e) { return; }

    let currentNorm = 0;

    state.valueChangedEvent.addListener(() => {
      currentNorm = state.getNormalisedValue();
      buildKnobSVG(svg, currentNorm, accent);
      if (disp) disp.textContent = formatFn(state.getScaledValue());
    });

    let dragging = false;
    let startY = 0;
    let startNorm = 0;

    wrap.addEventListener('mousedown', (e) => {
      e.preventDefault();
      dragging = true;
      startY = e.clientY;
      startNorm = currentNorm;
      state.sliderDragStarted();
      document.addEventListener('mousemove', onMove);
      document.addEventListener('mouseup', onUp);
    });

    function onMove(e) {
      if (!dragging) return;
      const dy = startY - e.clientY;
      const delta = dy / 150;
      const newNorm = Math.max(0, Math.min(1, startNorm + delta));
      state.setNormalisedValue(newNorm);
    }

    function onUp() {
      if (!dragging) return;
      dragging = false;
      state.sliderDragEnded();
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('mouseup', onUp);
    }

    // Double-click reset
    wrap.addEventListener('dblclick', () => {
      const def = (state.properties.defaultValue !== undefined)
        ? state.properties.defaultValue
        : 0.5;
      const norm = (def - state.properties.start) / (state.properties.end - state.properties.start);
      state.sliderDragStarted();
      state.setNormalisedValue(norm);
      state.sliderDragEnded();
    });
  }

  // ─── Format Helpers ─────────────────────────────────────────────────────────
  function fmtRate(v) { return v.toFixed(2); }
  function fmtPct(v)  { return Math.round(v * 100) + ''; }
  function fmtTone(v) { return Math.round(v * 100) + ''; }

  // ─── Mode crossfader (mode param) ──────────────────────────────────────────
  function setupMode() {
    const track = document.getElementById('crossfader-track');
    const thumb = document.getElementById('crossfader-thumb');
    const fill  = document.getElementById('crossfader-fill');
    const btn1  = document.getElementById('btn-mode1');
    const btn2  = document.getElementById('btn-mode2');

    let modeState;
    try { modeState = getSliderState('mode'); } catch(e) { return; }

    let currentNorm = 0;

    function updateCrossfader(norm) {
      currentNorm = norm;
      const trackW = track.offsetWidth;
      const pxPos = norm * trackW;
      thumb.style.left = pxPos + 'px';
      fill.style.width = pxPos + 'px';

      // Button states
      if (norm < 0.35) {
        btn1.classList.add('active');
        btn2.classList.remove('active');
      } else if (norm > 0.65) {
        btn2.classList.add('active');
        btn1.classList.remove('active');
      } else {
        btn1.classList.add('active');
        btn2.classList.add('active');
      }
    }

    modeState.valueChangedEvent.addListener(() => {
      updateCrossfader(modeState.getNormalisedValue());
    });

    // Drag
    let dragging = false;
    let trackRect;

    function startDrag(e) {
      e.preventDefault();
      dragging = true;
      trackRect = track.getBoundingClientRect();
      modeState.sliderDragStarted();
      document.addEventListener('mousemove', onMove);
      document.addEventListener('mouseup', onUp);
      onMove(e);
    }

    function onMove(e) {
      if (!dragging) return;
      const x = e.clientX - trackRect.left;
      const norm = Math.max(0, Math.min(1, x / trackRect.width));
      modeState.setNormalisedValue(norm);
    }

    function onUp() {
      dragging = false;
      modeState.sliderDragEnded();
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('mouseup', onUp);
    }

    track.addEventListener('mousedown', startDrag);
    thumb.addEventListener('mousedown', startDrag);

    // Button clicks
    btn1.addEventListener('click', () => {
      modeState.sliderDragStarted();
      modeState.setNormalisedValue(0.0);
      modeState.sliderDragEnded();
    });
    btn2.addEventListener('click', () => {
      modeState.sliderDragStarted();
      modeState.setNormalisedValue(1.0);
      modeState.sliderDragEnded();
    });
  }

  // ─── VU Meters ──────────────────────────────────────────────────────────────
  (function vuMeters() {
    const lFill = document.getElementById('vu-l-fill');
    const rFill = document.getElementById('vu-r-fill');
    let lVal = 0, rVal = 0;
    let lTarget = 0, rTarget = 0;
    let t = 0;

    function tick() {
      t += 0.03;
      // Organic animation simulating audio activity
      lTarget = Math.abs(Math.sin(t * 1.3) * Math.sin(t * 0.7 + 0.5)) * 0.7
              + Math.abs(Math.sin(t * 3.1 + 1.0)) * 0.15;
      rTarget = Math.abs(Math.sin(t * 1.1 + 0.8) * Math.sin(t * 0.9)) * 0.7
              + Math.abs(Math.sin(t * 2.8 + 2.0)) * 0.15;
      lTarget = Math.min(1, lTarget);
      rTarget = Math.min(1, rTarget);

      lVal += (lTarget - lVal) * 0.25;
      rVal += (rTarget - rVal) * 0.25;

      lFill.style.height = (lVal * 100) + '%';
      rFill.style.height = (rVal * 100) + '%';

      requestAnimationFrame(tick);
    }
    requestAnimationFrame(tick);
  })();

  // ─── Init ───────────────────────────────────────────────────────────────────
  function init() {
    setupMode();

    setupKnob('knob-rate',  'svg-rate',  'rate',  'val-rate',  fmtRate, '#ff9030');
    setupKnob('knob-depth', 'svg-depth', 'depth', 'val-depth', fmtPct,  '#ff8020');
    setupKnob('knob-drift', 'svg-drift', 'drift', 'val-drift', fmtPct,  '#e07820');
    setupKnob('knob-tone',  'svg-tone',  'tone',  'val-tone',  fmtTone, '#d87030');
    setupKnob('knob-width', 'svg-width', 'width', 'val-width', fmtPct,  '#f09020');
    setupKnob('knob-mix',   'svg-mix',   'mix',   'val-mix',   fmtPct,  '#ffa030');
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
</script>
</body>
</html>
)HTMLEOF");
}
