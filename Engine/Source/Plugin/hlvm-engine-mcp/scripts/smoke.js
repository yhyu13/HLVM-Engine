#!/usr/bin/env node
import { spawn } from "child_process";
import { fileURLToPath } from "url";
import { dirname, resolve } from "path";

const __dirname = dirname(fileURLToPath(import.meta.url));
const serverPath = resolve(__dirname, "../dist/index.js");

function sendMessage(proc, msg) {
  proc.stdin.write(JSON.stringify(msg) + "\n");
}

function readMessages(stream, onMessage) {
  let buffer = "";

  stream.on("data", (chunk) => {
    buffer += chunk.toString("utf-8");
    const lines = buffer.split("\n");
    buffer = lines.pop() ?? "";

    for (const line of lines) {
      if (!line.trim()) continue;
      try {
        onMessage(JSON.parse(line));
      } catch (err) {
        console.error("Failed to parse MCP message:", err, line);
      }
    }
  });
}

async function smokeTest() {
  console.log("Starting smoke test...");
  console.log("Server:", serverPath);

  const proc = spawn("node", [serverPath], {
    stdio: ["pipe", "pipe", "pipe"],
  });

  let initialized = false;
  let toolsListed = false;
  let testListed = false;
  let requestId = 1;

  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      proc.kill();
      reject(new Error("Smoke test timed out"));
    }, 30000);

    proc.stderr.on("data", (chunk) => {
      const text = chunk.toString("utf-8");
      if (process.env.DEBUG) {
        console.error("[server stderr]", text);
      }
    });

    readMessages(proc.stdout, (msg) => {
      if (msg.id === 1) {
        if (msg.result?.protocolVersion) {
          initialized = true;
          console.log("✅ initialize handshake succeeded");
        } else {
          reject(new Error(`initialize failed: ${JSON.stringify(msg)}`));
        }

        sendMessage(proc, {
          jsonrpc: "2.0",
          id: 2,
          method: "tools/list",
          params: {},
        });
      } else if (msg.id === 2) {
        const tools = msg.result?.tools ?? [];
        const names = tools.map((t) => t.name).sort();
        console.log("Tools:", names);

        const expected = ["list_hlvm_tests", "run_hlvm_test", "run_hlvm_tests_by_module"];
        if (JSON.stringify(names) !== JSON.stringify(expected.sort())) {
          reject(new Error(`Unexpected tools: ${JSON.stringify(names)}`));
        }
        toolsListed = true;
        console.log("✅ tools/list returned expected tools");

        sendMessage(proc, {
          jsonrpc: "2.0",
          id: 3,
          method: "tools/call",
          params: {
            name: "list_hlvm_tests",
            arguments: { config: "Debug" },
          },
        });
      } else if (msg.id === 3) {
        const content = msg.result?.content ?? [];
        const text = content.find((c) => c.type === "text")?.text ?? "";
        let parsed;
        try {
          parsed = JSON.parse(text);
        } catch {
          reject(new Error(`list_hlvm_tests returned non-JSON: ${text}`));
          return;
        }

        if (!Array.isArray(parsed.tests) || parsed.tests.length === 0) {
          reject(new Error(`list_hlvm_tests returned no tests: ${text}`));
          return;
        }

        testListed = true;
        console.log(`✅ list_hlvm_tests returned ${parsed.tests.length} tests`);
        console.log("First 5 tests:", parsed.tests.slice(0, 5).map((t) => t.name));

        proc.kill();
      }
    });

    proc.on("exit", (code) => {
      clearTimeout(timeout);
      if (initialized && toolsListed && testListed) {
        console.log("✅ Smoke test passed");
        resolve();
      } else {
        reject(new Error(`Smoke test incomplete. initialized=${initialized}, toolsListed=${toolsListed}, testListed=${testListed}, exitCode=${code}`));
      }
    });

    proc.on("error", reject);

    sendMessage(proc, {
      jsonrpc: "2.0",
      id: 1,
      method: "initialize",
      params: {
        protocolVersion: "2024-11-05",
        capabilities: {},
        clientInfo: { name: "hlvm-smoke-test", version: "0.1.0" },
      },
    });
  });
}

smokeTest().catch((err) => {
  console.error("Smoke test failed:", err);
  process.exit(1);
});
