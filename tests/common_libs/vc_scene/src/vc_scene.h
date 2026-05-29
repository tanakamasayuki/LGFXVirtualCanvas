#pragma once
//
// vc_scene.h
//
// Declarations only, intentionally neutral. The caller must include their
// graphics library (one of M5Unified.h / M5GFX.h / LovyanGFX.hpp) AND
// <LGFXVirtualCanvas.h> BEFORE this header so the `LovyanGFX` and
// `LGFXVirtualCanvas` types are in scope. The 3-way __has_include switch
// lives in vc_scene.cpp only (test-only pattern; production code should pin
// one graphics library).

// drawVcScene: compact scene in virtual coordinates that exercises basic
// shapes + text and straddles tile boundaries. Used by the Tier 2 build/
// parity tests against all three library entry points.
void drawVcScene(LGFXVirtualCanvas &g);

// savePng: write the full panel/canvas as PNG via createPng(). Returns true
// on success.
bool savePng(LovyanGFX &src, const char *path);
