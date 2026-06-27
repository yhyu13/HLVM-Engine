import { MCPTool, z } from "mcp-framework";
import { resolveRepoRoot } from "../utils/repoRoot.js";
import { listRuntimeTests } from "../utils/ctest.js";

const schema = z.object({
  config: z
    .enum(["Debug", "RelWithDebInfo", "Release"])
    .optional()
    .describe("Build configuration to inspect for registered tests."),
  pattern: z
    .string()
    .optional()
    .describe("Optional regex pattern to filter test names."),
  repoRoot: z
    .string()
    .optional()
    .describe("Optional absolute path to the HLVM Engine repository root."),
});

class ListHLVMTestsTool extends MCPTool {
  name = "list_hlvm_tests";
  description =
    "List available HLVM Engine Runtime tests registered with CTest. " +
    "Returns test names and the module they belong to. " +
    "Common tests are not supported in this version because they are not built as standalone executables.";
  schema = schema;

  async execute(input: z.infer<typeof schema>) {
    const config = input.config ?? "Debug";
    const repoRoot = resolveRepoRoot(input.repoRoot);
    const tests = await listRuntimeTests(repoRoot, config, input.pattern);

    return {
      config,
      count: tests.length,
      tests,
      note: "Only Runtime tests are listed. Common tests are not available as standalone executables in v1.",
    };
  }
}

export default ListHLVMTestsTool;
