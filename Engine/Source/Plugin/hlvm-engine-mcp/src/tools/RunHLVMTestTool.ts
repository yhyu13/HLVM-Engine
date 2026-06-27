import { MCPTool, z } from "mcp-framework";
import { resolveRepoRoot } from "../utils/repoRoot.js";
import { runtimeTestExists } from "../utils/ctest.js";
import { runHLVMTest } from "../utils/build.js";

const schema = z.object({
  target: z
    .string()
    .describe("CTest test name to run, e.g. TestSceneGraphNode."),
  config: z
    .enum(["Debug", "RelWithDebInfo", "Release"])
    .optional()
    .describe("Build configuration to use."),
  skipBuild: z
    .boolean()
    .optional()
    .describe(
      "If true, skip cmake --build and run the already-built test binary directly via ctest. " +
        "Use this for fast iteration after the binary is compiled."
    ),
  repoRoot: z
    .string()
    .optional()
    .describe("Optional absolute path to the HLVM Engine repository root."),
});

class RunHLVMTestTool extends MCPTool {
  name = "run_hlvm_test";
  description =
    "Build and run a single HLVM Engine Runtime test by name. " +
    "The test name must be a valid CTest-registered Runtime test. " +
    "Returns the exit code, build/test phase, and a tail of the log output.";
  schema = schema;

  async execute(input: z.infer<typeof schema>) {
    const config = input.config ?? "Debug";
    const repoRoot = resolveRepoRoot(input.repoRoot);

    const exists = await runtimeTestExists(repoRoot, config, input.target);
    if (!exists) {
      throw new Error(
        `No Runtime test named '${input.target}' found for config '${config}'. ` +
          `Use list_hlvm_tests to see available tests.`
      );
    }

    const result = await runHLVMTest({
      repoRoot,
      config,
      target: input.target,
      skipBuild: input.skipBuild,
      onProgress: async (progress, message) => {
        await this.reportProgress(progress, 100, message);
      },
    });

    return result;
  }
}

export default RunHLVMTestTool;
