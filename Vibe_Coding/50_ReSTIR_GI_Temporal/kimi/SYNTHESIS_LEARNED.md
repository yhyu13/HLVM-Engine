# Synthesis: What We Learned from claude.md + cerebrum.md + Impl Critics

**Date**: 2026-06-05
**Scope**: Meta-analysis of why the ReSTIR implementation failed and how reviews missed it

---

## 1. The claude.md Diagnostic (Complementary to Our Audit)

The claude.md review reached the **same verdict** (REBUILD FROM ASH) and identified overlapping but distinct critical bugs:

### CRITICAL BUG #4: No Importance Sampling (RIS) — NEW INSIGHT

Our audit focused on "ReSTIR is a post-process" and "reservoirs store surface positions." claude.md adds the specific missing algorithmic pieces:

**What ReSTIR requires** (from Bitterli et al.):
1. **Q-sampling**: Generate candidates using a proposal distribution
2. **Evaluate target**: Compute `p_hat(y) = luminance(radiance(y))` for each candidate
3. **RIS selection**: Choose among M candidates with probability proportional to `p_hat(y) / q(y)`
4. **Store result**: `y` = selected sample position, `W` = unbiased weight

**What the code does**: None of the above. M=1, no choice, no Q-sampling.

### The Alpha Channel Semantic Mismatch — Detailed Data Flow

claude.md traces the exact corruption path:

| Stage | Channel | Expected | Actual | Value |
|-------|---------|----------|--------|-------|
| FewBounceGI.hlsl | A | hitDist | hardcoded | `1.0` |
| ReSTIR_Generate | Reservoir1.w | hitDist | hardcoded | `1.0` |
| ReSTIR_Spatial | Output A | hitDist | `spatial.W` | luminance (0-100+) |
| ReBLUR_cs.hlsl | hitDist | distance | reads Output A | luminance |
| ReBLUR `GetNormHitDist` | normHitDist | [0,1] | `hitDist / f` | 10+ (clamped) |

**Impact**: ReBLUR's confidence estimation and anti-lag are completely broken because they operate on luminance values instead of hit distances.

### What Spatial Should Actually Output

claude.md provides the exact fix:
```hlsl
// WRONG (current):
float3 centerRadiance = gRadiance.Load(int3(pixel, 0)).rgb;
gOutRadiance[pixel] = float4(centerRadiance, spatial.W);

// CORRECT:
float3 selectedRadiance = gRadiance.Load(int3(spatial.y, 0)).rgb;
gOutRadiance[pixel] = float4(selectedRadiance, spatial.hitDist);
```

The reservoir's selected sample position `y` must be used to **lookup radiance from the input texture at that position**.

### The "3x3 Grid Worked" Myth

The self-review claimed 3x3 worked but Poisson failed. claude.md explains:
- 3x3 grid was effectively just a **3x3 bilateral filter**
- Center pixel dominated due to weight bugs
- No actual reservoir-based selection was happening
- Both are broken — 3x3 just hid it better

---

## 2. The cerebrum.md Insights (Meta-Learning)

### Why Self-Review Failed

The phase-by-phase reviews systematically checked the wrong things:

| What Was Checked | Result | What Was IGNORED | Result |
|-----------------|--------|-----------------|--------|
| Binding correctness | ✓ Pass | Algorithmic correctness | ✗ Fail |
| Code style | ✓ Pass | Does it implement ReSTIR? | ✗ Fail |
| NVRHI patterns | ✓ Pass | Visual output verification | ✗ Fail |
| Pass integration | ✓ Pass | Selected sample used? | ✗ Fail |
| Texture transitions | ✓ Pass | Alpha semantics | ✗ Fail |
| Ping-pong swaps | ✓ Pass | RIS actually happening? | ✗ Fail |
| No RAW hazards | ✓ Pass | Q-sampling present? | ✗ Fail |

**Root cause**: Reviews were code-structure reviews, not algorithm-verification reviews.

### The Architecture vs Implementation Gap

From cerebrum.md:
> "Having correct header files, pass infrastructure, and ping-pong buffers does NOT mean the algorithm is correct. The FReSTIRPass class, bindings, and pipeline integration were well-structured. But the HLSL shaders fundamentally didn't implement the described algorithm."

**Lesson**: Good C++ architecture can hide completely broken shaders. Review shaders FIRST, infrastructure SECOND.

### Do-Not-Repeat List (Institutional Knowledge)

| Prevention Measure | Why It Matters |
|-------------------|---------------|
| Verify algorithmic correctness **BEFORE** integration | The C++ pass was reviewed 5 times; the shader algorithm was never reviewed |
| Check that selected sample position is **actually used** | Spatial pass computed `spatial.y` then ignored it |
| Define semantic contract for **all texture channels upfront** | Alpha meant 3 different things to 3 different passes |

---

## 3. Impl Critics: A Study in Systematic Blindness

Reading the impl critics in sequence reveals a pattern of escalating false confidence:

### Phase 7a Critic (impl_critic.md)
- **Verdict**: REVISE (minor)
- **Issues found**: Unused sampler, unnecessary UAV write, duplicated helper
- **Missed**: Generation doesn't sample, reservoirs are meaningless
- **Confidence**: 8/10

### Phase 7b Critic (impl_7b_critic.md)
- **Verdict**: APPROVE with minor notes
- **Issues found**: PrevViewProj updated twice, no motion vectors, hardcoded threshold
- **Missed**: Temporal reprojection uses wrong depth, no prev-frame data
- **Confidence**: 9/10

### Phase 8 Critic (impl_8_critic.md)
- **Verdict**: APPROVE with minor notes
- **Issues found**: Simplified view reconstruction, debug texture shared, alpha channel changed
- **Missed**: Spatial pass is a no-op, W read from wrong channel, plane check skipped
- **Confidence**: 8/10
- **Key failure**: NOTED the alpha mismatch but ACCEPTED it with flawed reasoning: "W_merged is correlated with signal strength... test still passes"

### Phase 9 Critic (impl_9_critic.md)
- **Verdict**: APPROVE
- **Issues found**: Sky intensity hardcoded, no HDR verification
- **Missed**: Everything
- **Confidence**: 9/10

### Pattern: Confidence Inversely Correlated with Correctness

As the implementation grew more complex and more broken, reviewer confidence *increased*:
- Phase 7a: 8/10 (some doubt)
- Phase 7b: 9/10 (less doubt)
- Phase 8: 8/10 (slight doubt about alpha)
- Phase 9: 9/10 (fully confident)

**Why?** The reviewers were pattern-matching against "does this look like correct rendering code?" not "does this implement the correct algorithm?" More code = looks more complete = higher confidence.

---

## 4. Synthesized Root Cause Hierarchy

```
LEVEL 1: Test Design Failure
  └─ Test only checks for crashes (no image comparison)
  └─ Test runs for 5 seconds then exits (no convergence observation)
  └─ Frame dumps exist but were never systematically reviewed

LEVEL 2: Review Process Failure
  └─ Reviews focused on C++ infrastructure, not HLSL algorithms
  └─ No algorithmic trace-through of "what does this shader DO?"
  └─ Alpha semantic mismatch was noted but accepted with weak justification
  └─ "Test passes" was treated as correctness evidence

LEVEL 3: Architectural Misunderstanding
  └─ ReSTIR was designed as a post-process instead of a sampling algorithm
  └─ Reservoirs store surface positions instead of light samples
  └─ Spatial pass outputs center pixel instead of selected sample

LEVEL 4: Implementation Bugs (Symptoms, not Root Cause)
  └─ ReBLUR no reprojection
  └─ Uninitialized output texture
  └─ Wrong W channel read
  └─ Current-frame depth validation
  └─ ... (20+ bugs documented in TECHNICAL_BUG_BREAKDOWN.md)
```

**Key insight**: The implementation bugs (Level 4) are symptoms. The real failures are at Levels 1-3.

---

## 5. What a Correct Review Process Would Have Caught

### Checkpoint 1: Generation Shader Review

**Question**: "Show me where candidates are generated and where RIS selection happens."

**Expected**: Multiple candidate directions/points generated per pixel, streaming RIS loop.

**Actual**: `M = 1`, `pdf = 1.0`, no loop, no selection.

**Correct verdict**: REJECT immediately. This is not ReSTIR.

### Checkpoint 2: Spatial Shader Review

**Question**: "Trace the data flow: the spatial merge selects a winner. Where is that winner's radiance written to output?"

**Expected**: `gOutRadiance[pixel] = gRadiance.Load(int3(winnerPosition, 0))`

**Actual**: `gOutRadiance[pixel] = gRadiance.Load(int3(pixel, 0))` — center pixel, always.

**Correct verdict**: REJECT. Winner is computed then thrown away.

### Checkpoint 3: End-to-End Data Flow Review

**Question**: "Follow the alpha channel from GI shader through to ReBLUR. What does it represent at each stage?"

**Expected**: hitDist → hitDist → hitDist → hitDist (consistent)

**Actual**: 1.0 → 1.0 → W_merged → luminance (inconsistent)

**Correct verdict**: REJECT. Semantic contract broken.

### Checkpoint 4: Visual Verification

**Question**: "Show me the frame dumps for frames 1, 2, 3, 4."

**Expected**: Sponza scene with progressive noise reduction, stable colors.

**Actual**: Magenta, black, noise, green.

**Correct verdict**: REJECT immediately. Output is total corruption.

---

## 6. Key Differences Between Our Audit and claude.md

| Aspect | Our Audit | claude.md |
|--------|-----------|-----------|
| Verdict | REBUILD | REBUILD FROM ASH |
| Frame dumps examined | Yes (corruption confirmed) | Not mentioned |
| Line-by-line bug count | 20+ bugs across 8 files | 4 critical bugs |
| Shader bugs detailed | Yes (TECHNICAL_BUG_BREAKDOWN.md) | High-level only |
| ReBLUR analysis | Reads history at wrong coord, uninitialized texture | Focused on alpha mismatch |
| Rebuild roadmap | 20-day phased plan provided | Pseudocode for generation + spatial |
| Meta-analysis of reviews | Yes (this file) | No |

**Complementary value**: claude.md provides the correct pseudocode and emphasizes the RIS algorithm. Our audit provides the exhaustive bug inventory and rebuild plan.

---

## 7. Unified Lessons for Future Implementation

1. **Shaders before infrastructure**: Write and review HLSL algorithmic correctness before creating C++ pass classes.
2. **Visual verification every phase**: Every PR must include frame dumps. No exceptions.
3. **Semantic contracts in writing**: Document what every texture channel means before writing code.
4. **Algorithmic trace-through**: Reviewers must trace data flow end-to-end, not just check bindings.
5. **Test image quality, not just crashes**: Automated tests need reference image comparison (PSNR/SSIM).
6. **Beware false confidence**: More code and passing tests do not mean correctness.
7. **Do not accept "test passes" as evidence**: If the test only checks for crashes, it proves nothing about algorithmic correctness.

---

*This synthesis integrates insights from claude.md, cerebrum.md, and the four phase-by-phase implementation critics. It should be read alongside the other three documents in this folder.*
