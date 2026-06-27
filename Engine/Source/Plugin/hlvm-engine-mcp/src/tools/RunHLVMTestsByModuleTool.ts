import { MCPTool, z } from "mcp-framework";
import { resolveRepoRoot } from "../utils/repoRoot.js";
import { listRuntimeTests } from "../utils/ctest.js";
import { runHLVMTest } from "../utils/build.js";

const schema = z.object({
  module: z
    .enum(["Runtime"])
    .describe("Test module to run. Only Runtime is supported in v1."),
  config: z
    .enum(["Debug", "RelWithDebInfo", "Release"])
    .optional()
    .describe("Build configuration to use."),
  pattern: z
    .string()
    .optional()
    .describe("Optional regex pattern to filter which tests in the module to run."),
  repoRoot: z
    .string()
    .optional()
    .describe("Optional absolute path to the HLVM Engine repository root."),
});

class RunHLVMTestsByModuleTool extends MCPTool {
  name = "run_hlvm_tests_by_module";
  description =
    "Run multiple HLVM Engine Runtime tests filtered by module and optional name pattern. " +
    "Returns an aggregated summary of pass/fail results. " +
    "This can be long-running; progress notifications are sent between tests.";
  schema = schema;

  async execute(input: z.infer<typeof schema>) {
    const config = input.config ?? "Debug";
    const repoRoot = resolveRepoRoot(input.repoRoot);

    if (input.module !== "Runtime") {
      throw new Error(
        `Module '${input.module}' is not supported in v1. Only 'Runtime' tests can be run.`
      );
    }

    const tests = await listRuntimeTests(repoRoot, config, input.pattern);
    if (tests.length === 0) {
      return {
        success: true,
        module: input.module,
        config,
        passed: 0,
        failed: 0,
        results: [],
        note: "No tests matched the given pattern.",
      };
    }

    const results = [];
    let passed = 0;
    let failed = 0;

    for (let i = 0; i < tests.length; i++) {
      const test = tests[i];
      await this.reportProgress(i, tests.length, `Running ${test.name} (${i + 1}/${tests.length})`);

      const result = await runHLVMTest({
        repoRoot,
        config,
        target: test.name,
        skipBuild: true,
      });

      if (result.success) {
        passed++;
      } else {
        failed++;
      }

      results.push({
        name: test.name,
        ...result,
      });
    }

    await this.reportProgress(tests.length, tests.length, "All module tests finished");

    return {
      success: failed === 0,
      module: input.module,
      config,
      pattern: input.pattern ?? null,
      passed,
      failed,
      total: tests.length,
      results,
    };
  }
}

export default RunHLVMTestsByModuleTool;
