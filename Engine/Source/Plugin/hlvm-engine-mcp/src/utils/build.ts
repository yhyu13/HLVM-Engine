import { spawn, SpawnOptions } from "child_process";
import { getRuntimeBuildDir } from "./repoRoot.js";

export interface RunBuildOptions {
  repoRoot: string;
  config: string;
  target?: string;
  skipBuild?: boolean;
  onProgress?: (progress: number, message: string) => Promise<void> | void;
}

export interface BuildResult {
  success: boolean;
  exitCode: number;
  phase: BuildPhase;
  logTail: string;
  durationMs: number;
}

export type BuildPhase = "configure" | "compile" | "test" | "unknown";

const MAX_LOG_LINES = 200;
const PROGRESS_INTERVAL_MS = 3000;
const PROGRESS_INCREMENT = 2;

/**
 * Run a HLVM test via the repo-root Build.sh wrapper (configure + compile + ctest),
 * or run ctest directly if skipBuild is true.
 *
 * Progress notifications start near 0 and monotonically increase to 100.
 * Phase is inferred from Build.sh/ctest output.
 */
export async function runHLVMTest(options: RunBuildOptions): Promise<BuildResult> {
  const { repoRoot, config, target, skipBuild, onProgress } = options;
  const start = Date.now();
  const tracker = new ProgressTracker(onProgress);

  try {
    const result = skipBuild
      ? await runCtestDirectly(repoRoot, config, target, tracker)
      : await runBuildScript(repoRoot, config, target, tracker);

    await tracker.finish(result.phase);
    return result;
  } finally {
    tracker.dispose();
  }
}

async function runBuildScript(
  repoRoot: string,
  config: string,
  target: string | undefined,
  tracker: ProgressTracker
): Promise<BuildResult> {
  const buildArgs = [`--Config=${config}`, "--Test"];
  if (target) {
    buildArgs.push(`--Target=${target}`);
  }

  return runCommand({
    command: "./Build.sh",
    args: buildArgs,
    cwd: repoRoot,
    tracker,
  });
}

async function runCtestDirectly(
  repoRoot: string,
  config: string,
  target: string | undefined,
  tracker: ProgressTracker
): Promise<BuildResult> {
  const buildDir = getRuntimeBuildDir(repoRoot, config);
  const ctestArgs = ["--output-on-failure"];
  if (target) {
    ctestArgs.push("-R", target);
  }

  // Direct ctest starts in the test phase; skip configure/compile.
  tracker.setPhase("test");

  return runCommand({
    command: "ctest",
    args: ctestArgs,
    cwd: buildDir,
    tracker,
  });
}

interface RunCommandOptions {
  command: string;
  args: string[];
  cwd: string;
  tracker: ProgressTracker;
}

function runCommand(options: RunCommandOptions): Promise<BuildResult> {
  const { command, args, cwd, tracker } = options;
  const start = Date.now();

  return new Promise((resolve, reject) => {
    const proc = spawn(command, args, {
      cwd,
      stdio: ["ignore", "pipe", "pipe"],
    } as SpawnOptions);

    const onData = (chunk: Buffer) => {
      const lines = chunk.toString("utf-8").split("\n");
      for (const line of lines) {
        if (line.length > 0) {
          tracker.pushLog(line);
        }
      }
    };

    proc.stdout?.on("data", onData);
    proc.stderr?.on("data", onData);

    proc.on("error", (err) => {
      reject(new Error(`Failed to spawn ${command}: ${err.message}`));
    });

    proc.on("close", (code) => {
      const success = code === 0;
      resolve({
        success,
        exitCode: code ?? -1,
        phase: tracker.currentPhase,
        logTail: tracker.logTail,
        durationMs: Date.now() - start,
      });
    });
  });
}

class ProgressTracker {
  private logBuffer = new RingBuffer<string>(MAX_LOG_LINES);
  private phase: BuildPhase = "unknown";
  private progress = 0;
  private interval?: ReturnType<typeof setInterval>;

  constructor(private onProgress?: RunBuildOptions["onProgress"]) {
    this.interval = setInterval(() => this.report(), PROGRESS_INTERVAL_MS);
  }

  get currentPhase(): BuildPhase {
    return this.phase;
  }

  get logTail(): string {
    return this.logBuffer.toArray().join("\n");
  }

  pushLog(line: string): void {
    this.logBuffer.push(line);
    this.updatePhase(line);
  }

  setPhase(phase: BuildPhase): void {
    this.phase = phase;
  }

  async finish(finalPhase: BuildPhase): Promise<void> {
    this.phase = finalPhase;
    await this.report(100, `${finalPhase} finished`);
  }

  dispose(): void {
    if (this.interval) {
      clearInterval(this.interval);
      this.interval = undefined;
    }
  }

  private updatePhase(line: string): void {
    const lower = line.toLowerCase();

    // Order matters: test output can contain "build" words ("ninja: no work to do"),
    // so check test markers first once we are likely in test phase.
    if (
      lower.includes("test project") ||
      lower.includes("testing ") ||
      lower.includes("ctest")
    ) {
      this.phase = "test";
      return;
    }

    if (
      lower.includes("configuring done") ||
      lower.includes("generating done") ||
      lower.includes("pycmake: find package")
    ) {
      this.phase = "configure";
      return;
    }

    if (
      lower.includes("build cmd:") ||
      lower.includes("compiling") ||
      lower.includes("linking") ||
      lower.includes("ninja: no work to do")
    ) {
      this.phase = "compile";
    }
  }

  private async report(overrideProgress?: number, overrideMessage?: string): Promise<void> {
    if (!this.onProgress) return;

    if (overrideProgress !== undefined) {
      this.progress = overrideProgress;
    } else {
      // Clamp progress within phase-appropriate bands so it always increases
      // but never reaches 100 until finish().
      const cap = phaseProgressCap(this.phase);
      this.progress = Math.min(this.progress + PROGRESS_INCREMENT, cap);
      if (this.progress < 1) this.progress = 1;
    }

    const message = overrideMessage ?? `${this.phase}: ${this.logBuffer.last() ?? "running"}`;

    try {
      const maybePromise = this.onProgress(this.progress, message);
      if (maybePromise) {
        await maybePromise;
      }
    } catch {
      // Don't fail the build because of progress-notification errors.
    }
  }
}

function phaseProgressCap(phase: BuildPhase): number {
  switch (phase) {
    case "configure":
      return 25;
    case "compile":
      return 60;
    case "test":
      return 95;
    default:
      return 10;
  }
}

class RingBuffer<T> {
  private buffer: T[] = [];

  constructor(private capacity: number) {}

  push(item: T): void {
    if (this.buffer.length >= this.capacity) {
      this.buffer.shift();
    }
    this.buffer.push(item);
  }

  last(): T | undefined {
    return this.buffer[this.buffer.length - 1];
  }

  toArray(): T[] {
    return [...this.buffer];
  }
}
