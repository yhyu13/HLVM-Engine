# HLVM Engine MCP Server

An MCP (Model Context Protocol) server for HLVM Engine. The first supported capability is discovering and running Runtime engine tests through MCP tools.

## Features

- `list_hlvm_tests` — list all CTest-registered Runtime tests.
- `run_hlvm_test` — build and run a single Runtime test by name.
- `run_hlvm_tests_by_module` — run a filtered set of Runtime tests.

> **Note:** Common tests are not supported in v1 because they are not currently built as standalone executables.

## Build

```bash
cd Engine/Source/Plugin/hlvm-engine-mcp
npm install
npm run build
```

The `mcp-framework` dependency is resolved from the local `file:../mcp-framework` package, which is also built automatically via its `prepare` script.

## Run

```bash
npm start
```

The server uses stdio transport, suitable for Claude Desktop and other MCP clients.

## Claude Desktop Configuration

Add this to your Claude Desktop config file:

**macOS:** `~/Library/Application Support/Claude/claude_desktop_config.json`

**Windows:** `%APPDATA%/Claude/claude_desktop_config.json`

```json
{
  "mcpServers": {
    "hlvm-engine": {
      "command": "node",
      "args": [
        "/absolute/path/to/HLVM-Engine/Engine/Source/Plugin/hlvm-engine-mcp/dist/index.js"
      ],
      "env": {
        "HLVM_REPO_ROOT": "/absolute/path/to/HLVM-Engine"
      }
    }
  }
}
```

If `HLVM_REPO_ROOT` is not set, the server walks up from its working directory looking for `Build.sh`.

## Development

```bash
npm run watch     # TypeScript watch mode
npm run smoke     # Run a quick stdio handshake + list_hlvm_tests smoke test
```

## Troubleshooting

- **Long-running tests time out in the MCP client:** Use `skipBuild: true` after the test binary is compiled, or run the test directly via `./Build.sh`.
- **`ctest -N` errors:** Make sure the project has been configured at least once for the requested config (e.g., `Debug`).
- **Missing `.env`:** `Build.sh` sources `Binary/GNULinux-x64/.env`. Ensure it exists.
