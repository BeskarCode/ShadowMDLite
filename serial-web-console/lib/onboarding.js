/**
 * First-run guided setup: the steps and how to move between them (no DOM).
 * welcome → sketch → (patch → flash →) connect → walk → done
 * Plain script (no modules) so index.html still opens from file://.
 */
(function (root) {
  "use strict";

  const STEPS = ["welcome", "sketch", "patch", "flash", "connect", "walk", "done"];
  const KEY = "shadowmd_onboarding_v1";

  function initialState() {
    return { step: "welcome", skipped: false, detect: null };
  }

  function with_(state, patch) {
    return Object.assign({}, state, patch);
  }

  /** The state after `input` on the current step; unknown input leaves the step as it is. */
  function next(state, input) {
    const s = state || initialState();
    const i = input || {};
    switch (s.step) {
      case "welcome":
        return i.start ? with_(s, { step: "sketch" }) : s;
      case "sketch":
        if (i.skip) return with_(s, { step: "connect" });
        if (i.detect) {
          // Lite always has the forwarding; an unknown sketch goes on, with its warnings kept
          const needsPatch = i.detect.variant === "classic" && i.detect.forwarding !== true;
          return with_(s, { step: needsPatch ? "patch" : "connect", detect: i.detect });
        }
        return s;
      case "patch":
        return with_(s, { step: "flash" });
      case "flash":
        return i.flashed || i.skip ? with_(s, { step: "connect" }) : s;
      case "connect":
        return i.pong ? with_(s, { step: "walk" }) : s;
      case "walk":
        return with_(s, { step: "done" });
      default:
        return s;
    }
  }

  /**
   * Opens by itself the first time (nothing saved yet), and again after a reload when the guide
   * was left half-way (the parts it saved must not stop it); never once finished or skipped.
   */
  function shouldAutoOpen(state, hasSavedConfig) {
    const s = state || initialState();
    if (s.step === "done" || s.skipped) return false;
    return !hasSavedConfig || s.step !== "welcome";
  }

  function loadState(storage) {
    try {
      const raw = storage && storage.getItem(KEY);
      if (!raw) return initialState();
      const s = JSON.parse(raw);
      if (!s || typeof s !== "object" || STEPS.indexOf(s.step) < 0) return initialState();
      return { step: s.step, skipped: !!s.skipped, detect: s.detect && typeof s.detect === "object" ? s.detect : null };
    } catch (_) {
      return initialState();
    }
  }

  function saveState(storage, state) {
    try {
      if (storage) storage.setItem(KEY, JSON.stringify(state));
    } catch (_) {
      // private window or blocked storage: the guide still works, it just will not remember
    }
  }

  const api = { STEPS, KEY, initialState, next, shouldAutoOpen, loadState, saveState };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.Onboarding = api;
})(this);
