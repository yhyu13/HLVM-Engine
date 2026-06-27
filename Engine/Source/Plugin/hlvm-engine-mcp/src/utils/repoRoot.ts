import { existsSync } from "fs";
import { resolve, dirname } from "path";

export const HLVM_REPO_ROOT_ENV = "HLVM_REPO_ROOT";

/**
 * Resolve the HLVM Engine repository root.
 * 1. Use HLVM_REPO_ROOT env var if set and contains Build.sh.
 * 2. Walk up from process.cwd() looking for Build.sh.
 * 3. Throw a clear error if not found.
 */
export function resolveRepoRoot(override?: string): string {
  const candidates: string[] = [];

  if (override) {
    candidates.push(resolve(override));
  }

  const envRoot = process.env[HLVM_REPO_ROOT_ENV];
  if (envRoot) {
    candidates.push(resolve(envRoot));
  }

  for (const candidate of candidates) {
    if (isRepoRoot(candidate)) {
      return candidate;
    }
  }

  let current = resolve(process.cwd());
  for (let i = 0; i < 10; i++) {
    if (isRepoRoot(current)) {
      return current;
    }
    const parent = dirname(current);
    if (parent === current) {
      break;
    }
    current = parent;
  }

  throw new Error(
    `Could not locate HLVM Engine repository root. ` +
      `Set ${HLVM_REPO_ROOT_ENV} to the directory containing Build.sh, or run from inside the repo.`
  );
}

function isRepoRoot(dir: string): boolean {
  return existsSync(resolve(dir, "Build.sh"));
}

export function getRuntimeBuildDir(repoRoot: string, config: string): string {
  return resolve(repoRoot, "Engine/Source/Runtime/Build", config);
}

export function getCommonBuildDir(repoRoot: string, config: string): string {
  return resolve(repoRoot, "Engine/Source/Common/Build", config);
}
