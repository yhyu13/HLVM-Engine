import { spawn } from "child_process";
import { getRuntimeBuildDir } from "./repoRoot.js";

export type HLVMTestModule = "Runtime";

export interface HLVMTestInfo {
  name: string;
  module: HLVMTestModule;
}

const CTEST_TEST_LINE = /Test\s+#\d+:\s+(Test\w+)/;

/**
 * List CTest-registered Runtime tests for the given config.
 * Warnings about missing executables are ignored.
 */
export async function listRuntimeTests(
  repoRoot: string,
  config: string,
  pattern?: string
): Promise<HLVMTestInfo[]> {
  const buildDir = getRuntimeBuildDir(repoRoot, config);
  const regex = pattern ? new RegExp(pattern, "i") : null;

  return new Promise((resolve, reject) => {
    const proc = spawn("ctest", ["-N"], {
      cwd: buildDir,
      stdio: ["ignore", "pipe", "pipe"],
    });

    let stdout = "";
    let stderr = "";

    proc.stdout.on("data", (chunk: Buffer) => {
      stdout += chunk.toString("utf-8");
    });

    proc.stderr.on("data", (chunk: Buffer) => {
      stderr += chunk.toString("utf-8");
    });

    proc.on("error", (err) => {
      reject(new Error(`Failed to spawn ctest -N: ${err.message}`));
    });

    proc.on("close", (code) => {
      const tests: HLVMTestInfo[] = [];
      for (const line of stdout.split("\n")) {
        const match = CTEST_TEST_LINE.exec(line);
        if (match) {
          const name = match[1];
          if (regex && !regex.test(name)) {
            continue;
          }
          tests.push({ name, module: "Runtime" });
        }
      }

      if (tests.length === 0 && code !== 0) {
        reject(
          new Error(
            `ctest -N failed (exit ${code}) in ${buildDir}. ` +
              `Has the project been configured? stderr: ${stderr.slice(0, 500)}`
          )
        );
        return;
      }

      resolve(tests);
    });
  });
}

/**
 * Check whether a Runtime test name exists in CTest.
 */
export async function runtimeTestExists(
  repoRoot: string,
  config: string,
  target: string
): Promise<boolean> {
  const tests = await listRuntimeTests(repoRoot, config, `^${escapeRegex(target)}$`);
  return tests.length > 0;
}

function escapeRegex(value: string): string {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
