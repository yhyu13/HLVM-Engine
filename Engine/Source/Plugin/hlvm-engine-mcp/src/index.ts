import { MCPServer } from "mcp-framework";

const server = new MCPServer({
  name: "hlvm-engine-mcp",
  version: "0.1.0",
  devMode: process.env.NODE_ENV !== "production",
});

await server.start();
