#!/usr/bin/env bash
# verify_extent_invariants.sh — v191/v192 extent-substitution check.
#
# WHY THIS EXISTS: the cron runspace that wrote the v191/v192 patches has no
# shell (tirith denies every terminal invocation, including `pwd`), so it could
# not compile or run anything. This is the verification it wanted to run.
#
# Checks the arithmetic the patches depend on, in isolation from the engine:
#   v191  GBufferScale = WIDTH / max(HalfResWidth,1)   (was FB.width / ...)
#   v192  RcpFull{W,H} = 1/WIDTH, 1/HEIGHT             (was 1/FB.{width,height})
#         resolve grid = (WIDTH+7)/8 x (HEIGHT+7)/8    (was FB-derived)
#
# Usage:  bash verify_extent_invariants.sh
# Exit:   0 = all invariants hold, non-zero = a premise is wrong.
set -euo pipefail

SRC="$(mktemp -t hermes-verify-extent-XXXXXX.cpp)"
BIN="${SRC%.cpp}"
trap 'rm -f "$SRC" "$BIN"' EXIT

cat >"$SRC" <<'EOF'
#include <algorithm>
#include <cstdint>
#include <cstdio>

static const std::uint32_t WIDTH  = 800;   // TestReSTIR_GI_Temporal.cpp:106
static const std::uint32_t HEIGHT = 600;   // :107

static int fails = 0;
static void check(bool ok, const char* what)
{
    if (!ok) { std::printf("FAIL: %s\n", what); ++fails; }
}

int main()
{
    // --- v191: GBufferScale at both ReSTIR sites (:1039, :1087) --------------
    // Patched form is invariant; the guard survives HalfResWidth == 0, which is
    // its value before CreateGBufferTextures runs.
    check(static_cast<float>(WIDTH / std::max(400u, 1u)) == 2.0f, "v191 scale != 2 at half-res 400");
    check(static_cast<float>(WIDTH / std::max(0u,   1u)) == 800.0f, "v191 divide-by-zero guard");

    // The regression the patch removes: old form under a resized swapchain.
    // Integer division truncates BEFORE the cast, so both directions are silent.
    const std::uint32_t H = 400;
    check(static_cast<float>( 800u / std::max(H, 1u)) == 2.0f, "v191 not a no-op at 800 (patch must not move pixels)");
    check(static_cast<float>( 600u / std::max(H, 1u)) == 1.0f, "v191 600-wide premise: max(int(s),1) -> identity map, undoing v183");
    check(static_cast<float>(1200u / std::max(H, 1u)) == 3.0f, "v191 1200-wide premise: stride 3 indexes an 800-wide GBuffer OOB");

    // --- v192: resolve constants (:1126-1127) and grid (:1156) ---------------
    check(1.0f / static_cast<float>(WIDTH)  == 1.0f / 800.0f, "v192 RcpFullW");
    check(1.0f / static_cast<float>(HEIGHT) == 1.0f / 600.0f, "v192 RcpFullH");

    const std::uint32_t gx = (WIDTH + 7) / 8, gy = (HEIGHT + 7) / 8;
    check(gx == 100u && gy == 75u, "v192 grid extent");
    // Resolve_cs.hlsl writes FullResOutput[tid.xy] with NO bounds guard (:34,
    // :73; only early-out is the depth test at :32), so the grid must cover the
    // fixed output exactly and never overrun it.
    check(gx * 8u >= WIDTH  && gx * 8u - WIDTH  < 8u, "v192 grid must cover W exactly, no overrun");
    check(gy * 8u >= HEIGHT && gy * 8u - HEIGHT < 8u, "v192 grid must cover H exactly, no overrun");

    std::printf(fails ? "\n%d FAILURE(S)\n" : "\nALL INVARIANTS HOLD (%d failures)\n", fails);
    return fails != 0;
}
EOF

g++ -std=c++17 -Wall -Wextra -Werror -o "$BIN" "$SRC"
"$BIN"
