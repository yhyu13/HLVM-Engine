// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Utility/CVar/CVarMacros.h"

// =============================================================================
// Ray Traced GI Tunables
// =============================================================================

AUTO_CVAR_INT(r_GI_MaxBounces, 3, "Maximum number of indirect bounces (0 = direct only)", EConsoleVariableFlag::Saved)
AUTO_CVAR_INT(r_GI_SPP, 8, "Samples per pixel for indirect lighting (1-32)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_GI_AmbientScale, 0.3f, "Ambient light multiplier for occluded surfaces", EConsoleVariableFlag::Saved)
AUTO_CVAR_BOOL(r_GI_ShadowRays, true, "Enable shadow rays for direct lighting", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_GI_ShadowTMin, 0.001f, "Shadow ray TMin (avoid self-intersection)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_GI_ShadowTMax, 1000.0f, "Shadow ray TMax (max shadow distance)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_GI_RayTMin, 0.001f, "GI bounce ray TMin", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_GI_RayTMax, 1000.0f, "GI bounce ray TMax", EConsoleVariableFlag::Saved)

// =============================================================================
// ReSTIR GI Tunables
// =============================================================================

AUTO_CVAR_INT(r_ReSTIR_NumCandidates, 8, "ReSTIR generation: number of candidate samples per pixel", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ReSTIR_MaxM, 30.0f, "ReSTIR temporal: maximum reservoir M value", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ReSTIR_DepthThreshold, 0.05f, "ReSTIR temporal/spatial: depth rejection threshold", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ReSTIR_NormalThreshold, 0.5f, "ReSTIR temporal/spatial: normal dot rejection threshold", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ReSTIR_SpatialRadius, 1.0f, "ReSTIR spatial: neighbor sampling radius in pixels", EConsoleVariableFlag::Saved)
AUTO_CVAR_BOOL(r_ReSTIR_EnableTemporal, true, "Enable ReSTIR temporal resampling", EConsoleVariableFlag::Saved)
AUTO_CVAR_BOOL(r_ReSTIR_EnableSpatial, true, "Enable ReSTIR spatial resampling", EConsoleVariableFlag::Saved)

// =============================================================================
// Denoiser Tunables
// =============================================================================

AUTO_CVAR_BOOL(r_GI_Denoise, true, "Enable bilateral denoiser on GI output", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_GI_DenoiseSigma, 2.0f, "Bilateral denoiser spatial sigma", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_GI_DenoiseSigmaLum, 4.0f, "Bilateral denoiser luminance sigma", EConsoleVariableFlag::Saved)

// =============================================================================
// ReBLUR Denoiser Tunables
// =============================================================================

AUTO_CVAR_BOOL(r_ReBLUR_Enable, false, "Enable ReBLUR temporal+spatial denoiser (replaces bilateral)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ReBLUR_BlurRadius, 6.0f, "ReBLUR spatial blur radius in pixels", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ReBLUR_NormalWeight, 0.2f, "ReBLUR normal rejection weight", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ReBLUR_PlaneWeight, 50.0f, "ReBLUR plane distance weight", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ReBLUR_AntiLag, 0.5f, "ReBLUR anti-lag intensity (0-1)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ReBLUR_HistoryFadeIn, 6.0f, "ReBLUR history fade-in frames", EConsoleVariableFlag::Saved)
