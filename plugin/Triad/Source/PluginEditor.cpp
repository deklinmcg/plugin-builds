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
TriadAudioProcessorEditor::TriadAudioProcessorEditor (TriadAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    webView = std::make_unique<juce::WebBrowserComponent> (
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withOptionsFrom (layer1_pitchRelay)
            .withOptionsFrom (layer2_pitchRelay)
            .withOptionsFrom (layer3_pitchRelay)
            .withOptionsFrom (sizeRelay)
            .withOptionsFrom (decayRelay)
            .withOptionsFrom (shimmer_mixRelay)
            .withOptionsFrom (lfo_rateRelay)
            .withOptionsFrom (lfo_depthRelay)
            .withOptionsFrom (lfo_targetRelay)
            .withOptionsFrom (seq_stepsRelay)
            .withOptionsFrom (seq_rateRelay)
            .withOptionsFrom (seq_targetRelay)
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
            if (rp->paramID == ParameterIDs::layer1_pitch)
                layer1_pitchAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, layer1_pitchRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::layer2_pitch)
                layer2_pitchAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, layer2_pitchRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::layer3_pitch)
                layer3_pitchAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, layer3_pitchRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::size)
                sizeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, sizeRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::decay)
                decayAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, decayRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::shimmer_mix)
                shimmer_mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, shimmer_mixRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::lfo_rate)
                lfo_rateAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, lfo_rateRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::lfo_depth)
                lfo_depthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, lfo_depthRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::lfo_target)
                lfo_targetAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*rp, lfo_targetRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::seq_steps)
                seq_stepsAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, seq_stepsRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::seq_rate)
                seq_rateAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, seq_rateRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::seq_target)
                seq_targetAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*rp, seq_targetRelay);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID == ParameterIDs::mix)
                mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*rp, mixRelay);
    }

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    setSize (560, 320);
}

TriadAudioProcessorEditor::~TriadAudioProcessorEditor() {}

void TriadAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void TriadAudioProcessorEditor::resized()
{
    if (webView) webView->setBounds (getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource>
TriadAudioProcessorEditor::getResource (const juce::String& url)
{
    if (url.endsWith ("juce_bridge.js"))
    {
        juce::String js (kJuceBridgeJS);
        std::vector<std::byte> bytes (js.getNumBytesAsUTF8());
        std::memcpy (bytes.data(), js.toRawUTF8(), bytes.size());
        return juce::WebBrowserComponent::Resource { std::move (bytes), "text/javascript" };
    }

    auto html = TriadAudioProcessorEditor::getHTML();
    std::vector<std::byte> bytes (html.getNumBytesAsUTF8());
    std::memcpy (bytes.data(), html.toRawUTF8(), bytes.size());
    return juce::WebBrowserComponent::Resource { std::move (bytes), "text/html" };
}

juce::String TriadAudioProcessorEditor::getHTML()
{
    return juce::String (R"HTMLEOF(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Triad</title>
<script src="/juce_bridge.js"></script>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  
  body {
    width: 560px;
    height: 320px;
    background: #0d0d12;
    color: #c8ccd8;
    font-family: 'Segoe UI', system-ui, sans-serif;
    overflow: hidden;
    user-select: none;
  }

  #app {
    width: 560px;
    height: 320px;
    display: flex;
    flex-direction: column;
    position: relative;
  }

  /* Header */
  #header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 4px 12px 3px 12px;
    background: linear-gradient(180deg, #13131f 0%, #0d0d12 100%);
    border-bottom: 1px solid #1e1e2e;
    flex-shrink: 0;
  }

  #plugin-name {
    font-size: 15px;
    font-weight: 700;
    letter-spacing: 4px;
    text-transform: uppercase;
    background: linear-gradient(135deg, #a78bfa, #38bdf8, #fb923c);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
  }

  #plugin-sub {
    font-size: 8px;
    color: #3a3a5a;
    letter-spacing: 2px;
    text-transform: uppercase;
  }

  /* VU meters */
  #vu-container {
    display: flex;
    gap: 3px;
    align-items: flex-end;
  }

  .vu-meter {
    width: 5px;
    height: 28px;
    background: #0a0a10;
    border: 1px solid #1e1e2e;
    border-radius: 1px;
    position: relative;
    overflow: hidden;
    display: flex;
    flex-direction: column-reverse;
  }

  .vu-fill {
    width: 100%;
    background: linear-gradient(0deg, #38bdf8 0%, #a78bfa 60%, #ef4444 100%);
    transition: height 0.05s;
    border-radius: 1px;
  }

  /* Top controls section */
  #top-section {
    display: flex;
    align-items: flex-start;
    padding: 6px 10px 4px 10px;
    gap: 6px;
    flex-shrink: 0;
  }

  /* Layer pitch knobs group */
  #layer-knobs {
    display: flex;
    gap: 6px;
    align-items: flex-start;
  }

  /* Centre knobs group */
  #center-knobs {
    display: flex;
    gap: 4px;
    align-items: flex-start;
    margin-left: 4px;
  }

  /* Right: mix */
  #right-section {
    margin-left: auto;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
  }

  /* Knob component */
  .knob-wrap {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 2px;
    cursor: pointer;
  }

  .knob-label {
    font-size: 7px;
    letter-spacing: 1px;
    text-transform: uppercase;
    color: #4a4a6a;
    text-align: center;
  }

  .knob-value {
    font-size: 7px;
    color: #6a6a9a;
    text-align: center;
    min-width: 28px;
    font-variant-numeric: tabular-nums;
  }

  /* Separator */
  #separator {
    height: 1px;
    background: linear-gradient(90deg, transparent, #1e1e38, #2a2a50, #1e1e38, transparent);
    margin: 0 10px;
    flex-shrink: 0;
  }

  /* Bottom modulation section */
  #bottom-section {
    display: flex;
    flex: 1;
    padding: 4px 8px 4px 8px;
    gap: 6px;
    min-height: 0;
  }

  /* LFO section */
  #lfo-section {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 3px;
    width: 100px;
    flex-shrink: 0;
  }

  #lfo-title {
    font-size: 7px;
    letter-spacing: 2px;
    color: #3a3a5a;
    text-transform: uppercase;
    align-self: flex-start;
  }

  #lfo-knobs {
    display: flex;
    gap: 4px;
    align-items: flex-start;
  }

  #lfo-target-row {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    width: 100%;
    gap: 2px;
  }

  .target-label-small {
    font-size: 7px;
    color: #3a3a5a;
    letter-spacing: 1px;
    text-transform: uppercase;
  }

  .combo-select {
    background: #0a0a14;
    border: 1px solid #2a2a40;
    color: #8888aa;
    font-size: 7px;
    padding: 2px 4px;
    border-radius: 2px;
    width: 100%;
    cursor: pointer;
    outline: none;
    -webkit-appearance: none;
    appearance: none;
  }

  .combo-select:focus {
    border-color: #4a4a70;
  }

  /* Sequencer section */
  #seq-section {
    display: flex;
    flex-direction: column;
    flex: 1;
    gap: 3px;
    min-width: 0;
  }

  #seq-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 6px;
  }

  #seq-title {
    font-size: 7px;
    letter-spacing: 2px;
    color: #3a3a5a;
    text-transform: uppercase;
    white-space: nowrap;
  }

  #seq-controls {
    display: flex;
    align-items: center;
    gap: 6px;
  }

  #seq-steps-wrap, #seq-rate-wrap {
    display: flex;
    align-items: center;
    gap: 3px;
  }

  .seq-ctrl-label {
    font-size: 7px;
    color: #3a3a5a;
    letter-spacing: 1px;
    text-transform: uppercase;
    white-space: nowrap;
  }

  #seq-grid {
    display: flex;
    gap: 2px;
    flex: 1;
    align-items: stretch;
  }

  .seq-step {
    flex: 1;
    background: #0f0f1a;
    border: 1px solid #1a1a2a;
    border-radius: 2px;
    position: relative;
    cursor: pointer;
    min-width: 0;
    overflow: hidden;
    transition: border-color 0.1s;
  }

  .seq-step.active-step {
    border-color: #a78bfa;
    box-shadow: 0 0 6px rgba(167,139,250,0.4);
  }

  .seq-step.inactive {
    opacity: 0.35;
  }

  .seq-step-fill {
    position: absolute;
    bottom: 0;
    left: 0;
    width: 100%;
    background: linear-gradient(180deg, #a78bfa 0%, #7c3aed 100%);
    border-radius: 1px;
    transition: height 0.05s;
  }

  .seq-step.active-step .seq-step-fill {
    filter: brightness(1.4);
  }

  .seq-step-num {
    position: absolute;
    bottom: 2px;
    left: 50%;
    transform: translateX(-50%);
    font-size: 6px;
    color: #2a2a40;
    pointer-events: none;
    z-index: 2;
  }
</style>
</head>
<body>
<div id="app">
  <!-- HEADER -->
  <div id="header">
    <div>
      <div id="plugin-name">TRIAD</div>
      <div id="plugin-sub">Three-Layer Shimmer Reverb</div>
    </div>
    <div id="vu-container">
      <div class="vu-meter"><div class="vu-fill" id="vu-l" style="height:0%"></div></div>
      <div class="vu-meter"><div class="vu-fill" id="vu-r" style="height:0%"></div></div>
    </div>
  </div>

  <!-- TOP SECTION -->
  <div id="top-section">
    <!-- Layer pitch knobs -->
    <div id="layer-knobs">
      <div class="knob-wrap">
        <div class="knob-label" style="color:#a78bfa88">Layer 1</div>
        <canvas id="knob-layer1_pitch" width="56" height="56" data-param="layer1_pitch" data-color="#a78bfa"></canvas>
        <div class="knob-value" id="val-layer1_pitch">0</div>
        <div class="knob-label">Pitch</div>
      </div>
      <div class="knob-wrap">
        <div class="knob-label" style="color:#2dd4bf88">Layer 2</div>
        <canvas id="knob-layer2_pitch" width="56" height="56" data-param="layer2_pitch" data-color="#2dd4bf"></canvas>
        <div class="knob-value" id="val-layer2_pitch">0</div>
        <div class="knob-label">Pitch</div>
      </div>
      <div class="knob-wrap">
        <div class="knob-label" style="color:#fb923c88">Layer 3</div>
        <canvas id="knob-layer3_pitch" width="56" height="56" data-param="layer3_pitch" data-color="#fb923c"></canvas>
        <div class="knob-value" id="val-layer3_pitch">0</div>
        <div class="knob-label">Pitch</div>
      </div>
    </div>

    <!-- Centre knobs -->
    <div id="center-knobs">
      <div class="knob-wrap">
        <canvas id="knob-size" width="42" height="42" data-param="size" data-color="#60a5fa"></canvas>
        <div class="knob-value" id="val-size">0.00</div>
        <div class="knob-label">Size</div>
      </div>
      <div class="knob-wrap">
        <canvas id="knob-decay" width="42" height="42" data-param="decay" data-color="#60a5fa"></canvas>
        <div class="knob-value" id="val-decay">0.0</div>
        <div class="knob-label">Decay</div>
      </div>
      <div class="knob-wrap">
        <canvas id="knob-shimmer_mix" width="42" height="42" data-param="shimmer_mix" data-color="#e879f9"></canvas>
        <div class="knob-value" id="val-shimmer_mix">0.00</div>
        <div class="knob-label">Shimmer</div>
      </div>
    </div>

    <!-- Mix knob -->
    <div id="right-section">
      <div class="knob-wrap">
        <canvas id="knob-mix" width="42" height="42" data-param="mix" data-color="#38bdf8"></canvas>
        <div class="knob-value" id="val-mix">0.00</div>
        <div class="knob-label">Mix</div>
      </div>
    </div>
  </div>

  <!-- SEPARATOR -->
  <div id="separator"></div>

  <!-- BOTTOM SECTION -->
  <div id="bottom-section">
    <!-- LFO Section -->
    <div id="lfo-section">
      <div id="lfo-title">LFO</div>
      <div id="lfo-knobs">
        <div class="knob-wrap">
          <canvas id="knob-lfo_rate" width="36" height="36" data-param="lfo_rate" data-color="#a78bfa"></canvas>
          <div class="knob-value" id="val-lfo_rate">0.50</div>
          <div class="knob-label">Rate</div>
        </div>
        <div class="knob-wrap">
          <canvas id="knob-lfo_depth" width="36" height="36" data-param="lfo_depth" data-color="#a78bfa"></canvas>
          <div class="knob-value" id="val-lfo_depth">0.00</div>
          <div class="knob-label">Depth</div>
        </div>
      </div>
      <div id="lfo-target-row">
        <div class="target-label-small">Target</div>
        <select class="combo-select" id="combo-lfo_target">
          <option value="0">Layer 1 Pitch</option>
          <option value="1">Layer 2 Pitch</option>
          <option value="2">Layer 3 Pitch</option>
          <option value="3">Size</option>
          <option value="4">Decay</option>
          <option value="5">Mix</option>
        </select>
      </div>
    </div>

    <!-- Sequencer Section -->
    <div id="seq-section">
      <div id="seq-header">
        <div id="seq-title">Sequencer</div>
        <div id="seq-controls">
          <div id="seq-steps-wrap">
            <span class="seq-ctrl-label">Steps</span>
            <canvas id="knob-seq_steps" width="28" height="28" data-param="seq_steps" data-color="#38bdf8"></canvas>
            <span class="knob-value" id="val-seq_steps" style="font-size:7px">16</span>
          </div>
          <div id="seq-rate-wrap">
            <span class="seq-ctrl-label">Rate</span>
            <canvas id="knob-seq_rate" width="28" height="28" data-param="seq_rate" data-color="#38bdf8"></canvas>
            <span class="knob-value" id="val-seq_rate" style="font-size:7px">1.0</span>
          </div>
          <div style="display:flex;flex-direction:column;gap:2px">
            <div class="target-label-small">Target</div>
            <select class="combo-select" id="combo-seq_target" style="width:72px">
              <option value="0">Layer 1 Pitch</option>
              <option value="1">Layer 2 Pitch</option>
              <option value="2">Layer 3 Pitch</option>
              <option value="3">Size</option>
              <option value="4">Decay</option>
              <option value="5">Mix</option>
            </select>
          </div>
        </div>
      </div>

      <div id="seq-grid">
        <!-- Steps generated by JS -->
      </div>
    </div>
  </div>
</div>

<script>
// ─── Utilities ──────────────────────────────────────────────────────────────

function hexToRgb(hex) {
  const r = parseInt(hex.slice(1,3),16);
  const g = parseInt(hex.slice(3,5),16);
  const b = parseInt(hex.slice(5,7),16);
  return {r,g,b};
}

function drawKnobCanvas(canvas, normVal, color) {
  const size = canvas.width;
  const ctx = canvas.getContext('2d');
  const cx = size/2, cy = size/2;
  const radius = size*0.36;
  const trackWidth = size*0.09;

  ctx.clearRect(0, 0, size, size);

  const startAngle = (Math.PI * 3/4);
  const endAngle = (Math.PI * 9/4);
  const valueAngle = startAngle + normVal * (endAngle - startAngle);

  // Glow for large knobs
  if (size >= 50) {
    const rgb = hexToRgb(color);
    ctx.save();
    ctx.shadowColor = color;
    ctx.shadowBlur = 10 * normVal + 3;
    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, valueAngle);
    ctx.strokeStyle = 'transparent';
    ctx.lineWidth = trackWidth;
    ctx.stroke();
    ctx.restore();
  }

  // Background track
  ctx.beginPath();
  ctx.arc(cx, cy, radius, startAngle, endAngle);
  ctx.strokeStyle = '#1a1a28';
  ctx.lineWidth = trackWidth;
  ctx.lineCap = 'round';
  ctx.stroke();

  // Value arc
  if (normVal > 0) {
    const rgb = hexToRgb(color);
    const grad = ctx.createLinearGradient(cx - radius, cy, cx + radius, cy);
    grad.addColorStop(0, `rgba(${rgb.r},${rgb.g},${rgb.b},0.6)`);
    grad.addColorStop(1, `rgba(${rgb.r},${rgb.g},${rgb.b},1.0)`);
    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, valueAngle);
    ctx.strokeStyle = grad;
    ctx.lineWidth = trackWidth;
    ctx.lineCap = 'round';
    ctx.stroke();
  }

  // Knob body
  const bodyRadius = radius - trackWidth * 0.8;
  const bodyGrad = ctx.createRadialGradient(cx - bodyRadius*0.2, cy - bodyRadius*0.2, 0, cx, cy, bodyRadius);
  bodyGrad.addColorStop(0, '#2a2a3a');
  bodyGrad.addColorStop(1, '#111118');
  ctx.beginPath();
  ctx.arc(cx, cy, bodyRadius, 0, Math.PI * 2);
  ctx.fillStyle = bodyGrad;
  ctx.fill();

  // Pointer
  const pointerAngle = startAngle + normVal * (endAngle - startAngle);
  const px = cx + Math.cos(pointerAngle) * bodyRadius * 0.65;
  const py = cy + Math.sin(pointerAngle) * bodyRadius * 0.65;
  ctx.beginPath();
  ctx.moveTo(cx, cy);
  ctx.lineTo(px, py);
  ctx.strokeStyle = color;
  ctx.lineWidth = size >= 50 ? 2 : 1.5;
  ctx.lineCap = 'round';
  ctx.stroke();

  // Center dot
  ctx.beginPath();
  ctx.arc(cx, cy, size*0.04, 0, Math.PI*2);
  ctx.fillStyle = color;
  ctx.fill();
}

// ─── Knob Interaction ────────────────────────────────────────────────────────

function setupKnob(paramId) {
  const canvas = document.getElementById('knob-' + paramId);
  if (!canvas) return;
  const color = canvas.dataset.color || '#60a5fa';
  const valEl = document.getElementById('val-' + paramId);

  let state;
  try { state = getSliderState(paramId); } catch(e) { return; }

  let normVal = 0.5;

  function formatVal(v, id) {
    if (id.includes('pitch')) return (v >= 0 ? '+' : '') + v.toFixed(1);
    if (id === 'decay') return v.toFixed(1) + 's';
    if (id === 'lfo_rate') return v.toFixed(2) + 'Hz';
    if (id === 'seq_rate') return v.toFixed(2);
    if (id === 'seq_steps') return Math.round(v).toString();
    return v.toFixed(2);
  }

  state.valueChangedEvent.addListener(() => {
    normVal = state.getNormalisedValue();
    const scaled = state.getScaledValue();
    drawKnobCanvas(canvas, normVal, color);
    if (valEl) valEl.textContent = formatVal(scaled, paramId);
  });

  // Drag
  let dragging = false;
  let startY = 0;
  let startNorm = 0;

  canvas.addEventListener('mousedown', (e) => {
    dragging = true;
    startY = e.clientY;
    startNorm = normVal;
    state.sliderDragStarted();
    e.preventDefault();
  });

  window.addEventListener('mousemove', (e) => {
    if (!dragging) return;
    const dy = startY - e.clientY;
    const sensitivity = e.shiftKey ? 0.002 : 0.007;
    let newNorm = Math.max(0, Math.min(1, startNorm + dy * sensitivity));
    state.setNormalisedValue(newNorm);
    normVal = newNorm;
    drawKnobCanvas(canvas, normVal, color);
  });

  window.addEventListener('mouseup', () => {
    if (dragging) {
      dragging = false;
      state.sliderDragEnded();
    }
  });

  // Double click reset
  canvas.addEventListener('dblclick', () => {
    const props = state.properties;
    const def = props.defaultValue !== undefined ? props.defaultValue : (props.start + props.end) / 2;
    const norm = (def - props.start) / (props.end - props.start);
    state.sliderDragStarted();
    state.setNormalisedValue(Math.max(0, Math.min(1, norm)));
    state.sliderDragEnded();
  });

  drawKnobCanvas(canvas, normVal, color);
}

// ─── ComboBox Setup ──────────────────────────────────────────────────────────

function setupCombo(paramId) {
  const select = document.getElementById('combo-' + paramId);
  if (!select) return;
  let state;
  try { state = getComboBoxState(paramId); } catch(e) { return; }

  state.valueChangedEvent.addListener(() => {
    const idx = state.getChoiceIndex();
    select.value = idx.toString();
  });

  select.addEventListener('change', () => {
    state.setChoiceIndex(parseInt(select.value));
  });
}

// ─── Sequencer Setup ─────────────────────────────────────────────────────────

const SEQ_VALUES = new Array(16).fill(0.5);
let activeStep = 0;
let numActiveSteps = 16;
let seqAnimFrame = null;
let lastStepTime = 0;
let seqRateBPM = 1.0;

function buildSeqGrid() {
  const grid = document.getElementById('seq-grid');
  grid.innerHTML = '';
  for (let i = 0; i < 16; i++) {
    const step = document.createElement('div');
    step.className = 'seq-step' + (i >= numActiveSteps ? ' inactive' : '');
    step.id = 'seq-step-' + i;

    const fill = document.createElement('div');
    fill.className = 'seq-step-fill';
    fill.id = 'seq-fill-' + i;
    fill.style.height = (SEQ_VALUES[i] * 100) + '%';

    const num = document.createElement('div');
    num.className = 'seq-step-num';
    num.textContent = i + 1;

    step.appendChild(fill);
    step.appendChild(num);
    grid.appendChild(step);

    // Click to set value
    step.addEventListener('mousedown', (e) => {
      const rect = step.getBoundingClientRect();
      const relY = e.clientY - rect.top;
      const val = 1 - relY / rect.height;
      SEQ_VALUES[i] = Math.max(0, Math.min(1, val));
      fill.style.height = (SEQ_VALUES[i] * 100) + '%';
      e.preventDefault();
    });

    step.addEventListener('mousemove', (e) => {
      if (e.buttons !== 1) return;
      const rect = step.getBoundingClientRect();
      const relY = e.clientY - rect.top;
      const val = 1 - relY / rect.height;
      SEQ_VALUES[i] = Math.max(0, Math.min(1, val));
      fill.style.height = (SEQ_VALUES[i] * 100) + '%';
    });
  }
}

function updateSeqStepColors() {
  for (let i = 0; i < 16; i++) {
    const step = document.getElementById('seq-step-' + i);
    if (!step) continue;
    step.classList.toggle('active-step', i === activeStep && i < numActiveSteps);
    step.classList.toggle('inactive', i >= numActiveSteps);
  }
}

// Animate sequencer playback
function animateSeq(ts) {
  const interval = (60000 / 120) * seqRateBPM;
  if (ts - lastStepTime > interval) {
    lastStepTime = ts;
    activeStep = (activeStep + 1) % numActiveSteps;
    updateSeqStepColors();
  }
  seqAnimFrame = requestAnimationFrame(animateSeq);
}

// ─── VU Meter Animation ───────────────────────────────────────────────────────

let vuL = 0, vuR = 0;
function animateVU() {
  // Simulate gentle movement
  const t = Date.now() * 0.001;
  const target = 0.15 + 0.12 * (Math.sin(t * 1.7) * 0.5 + 0.5) + 0.05 * Math.random();
  vuL += (target - vuL) * 0.15;
  vuR += (target * 0.95 - vuR) * 0.15;
  document.getElementById('vu-l').style.height = (vuL * 100) + '%';
  document.getElementById('vu-r').style.height = (vuR * 100) + '%';
  requestAnimationFrame(animateVU);
}

// ─── Init ─────────────────────────────────────────────────────────────────────

function init() {
  // Build sequencer grid
  buildSeqGrid();

  // Setup all knobs
  const knobParams = [
    'layer1_pitch','layer2_pitch','layer3_pitch',
    'size','decay','shimmer_mix',
    'lfo_rate','lfo_depth',
    'seq_steps','seq_rate','mix'
  ];
  knobParams.forEach(setupKnob);

  // Setup combos
  setupCombo('lfo_target');
  setupCombo('seq_target');

  // Listen to seq_steps for grid update
  try {
    const stepsState = getSliderState('seq_steps');
    stepsState.valueChangedEvent.addListener(() => {
      numActiveSteps = Math.round(stepsState.getScaledValue());
      updateSeqStepColors();
    });
  } catch(e) {}

  // Listen to seq_rate
  try {
    const rateState = getSliderState('seq_rate');
    rateState.valueChangedEvent.addListener(() => {
      seqRateBPM = rateState.getScaledValue();
    });
  } catch(e) {}

  // Start animations
  animateVU();
  requestAnimationFrame(animateSeq);
}

window.addEventListener('DOMContentLoaded', init);
</script>
</body>
</html>
)HTMLEOF");
}
