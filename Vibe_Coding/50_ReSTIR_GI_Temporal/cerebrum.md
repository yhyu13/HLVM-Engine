# Cerebrum — ReSTIR GI Temporal Implementation

## Key Learnings

### ReSTIR GI Algorithm Requirements
- ReSTIR GI requires **RIS (Reservoir-based Importance Sampling)** — generating M candidate light paths and selecting among them
- Simply copying per-pixel data into a "reservoir" struct is NOT ReSTIR — it's just data formatting
- The selected sample position `y` must be propagated through temporal and spatial reuse
- **Output must be radiance at `y`**, not the center pixel radiance

### Critical Semantic Issues Discovered
- **Alpha channel**: GI outputs hit distance in alpha. ReSTIR passes reservoir weight W in alpha. ReBLUR expects hit distance.
- This is not a simple bug — it's a fundamental semantic mismatch across the pipeline
- Must preserve hit distance semantics OR add a separate hit distance texture

### Architecture vs Implementation Gap
- Having correct header files, pass infrastructure, and ping-pong buffers does NOT mean the algorithm is correct
- The FReSTIRPass class, bindings, and pipeline integration were well-structured
- But the HLSL shaders fundamentally didn't implement the described algorithm

### Why Self-Review Failed to Catch This
- Phase reviews focused on: binding correctness, code style, NVRHI patterns, pass integration
- Did NOT verify: does the algorithm actually do what ReSTIR GI is supposed to do?
- Passed all tests because tests only checked for crashes, not correctness

## Do-Not-Repeat

| Date | Issue | Prevention |
|------|-------|------------|
| 2026-06-05 | ReSTIR Generation doesn't actually sample | Verify algorithmic correctness BEFORE integration |
| 2026-06-05 | Spatial reuse ignores `y` | Check that selected sample position is actually used |
| 2026-06-05 | Alpha semantic mismatch | Define semantic contract for all texture channels upfront |

## Decision Log

| Decision | Reasoning |
|----------|-----------|
| Tear down vs fix | Fixing requires rewriting all three phases — same effort as rebuilding |
| Why tests passed | Only crash-testing, no image quality verification |
| Why 3x3 appeared to work | Bilateral filter effect, not ReSTIR working |

## User Preferences

| Preference | Evidence |
|------------|----------|
| Want comprehensive diagnostic | Dumped to claude.md despite GateGuard |
| "Tear down and rebuild from ash" | Original request was clear about severity |
