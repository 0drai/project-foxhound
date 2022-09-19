/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Exposed=Window]
interface Screen : EventTarget {
  // CSSOM-View
  // http://dev.w3.org/csswg/cssom-view/#the-screen-interface
  [Throws, TaintSource]
  readonly attribute long availWidth;
  [Throws, TaintSource]
  readonly attribute long availHeight;
  [Throws, TaintSource]
  readonly attribute long width;
  [Throws, TaintSource]
  readonly attribute long height;
  [Throws, TaintSource]
  readonly attribute long colorDepth;
  [Throws, TaintSource]
  readonly attribute long pixelDepth;

  [Throws]
  readonly attribute long top;
  [Throws]
  readonly attribute long left;
  [Throws, TaintSource]
  readonly attribute long availTop;
  [Throws, TaintSource]
  readonly attribute long availLeft;

  /**
   * DEPRECATED, use ScreenOrientation API instead.
   * Returns the current screen orientation.
   * Can be: landscape-primary, landscape-secondary,
   *         portrait-primary or portrait-secondary.
   */
  [NeedsCallerType]
  readonly attribute DOMString mozOrientation;

  attribute EventHandler onmozorientationchange;

  /**
   * DEPRECATED, use ScreenOrientation API instead.
   * Lock screen orientation to the specified type.
   */
  [Throws]
  boolean mozLockOrientation(DOMString orientation);
  [Throws]
  boolean mozLockOrientation(sequence<DOMString> orientation);

  /**
   * DEPRECATED, use ScreenOrientation API instead.
   * Unlock the screen orientation.
   */
  void mozUnlockOrientation();
};

// https://w3c.github.io/screen-orientation
partial interface Screen {
  readonly attribute ScreenOrientation orientation;
};

// https://wicg.github.io/media-capabilities/#idl-index
enum ScreenColorGamut {
  "srgb",
  "p3",
  "rec2020",
};

[Func="nsScreen::MediaCapabilitiesEnabled",
 Exposed=Window]
interface ScreenLuminance {
  readonly attribute double min;
  readonly attribute double max;
  readonly attribute double maxAverage;
};

partial interface Screen {
  [Func="nsScreen::MediaCapabilitiesEnabled"]
  readonly attribute ScreenColorGamut colorGamut;
  [Func="nsScreen::MediaCapabilitiesEnabled"]
  readonly attribute ScreenLuminance? luminance;

  [Func="nsScreen::MediaCapabilitiesEnabled"]
  attribute EventHandler onchange;
};
