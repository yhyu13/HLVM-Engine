# HLVM Engine Console Variable (CVar) System

This directory contains a UE5-like console variable (cvar) system implementation with ini-based configuration loading and saving for the HLVM Engine.

## Files Overview

- **CVar.h** - Main console variable interface and CVarManager singleton
- **CVar.cpp** - Implementation of console variable system
- **CVarTypes.h** - Type definitions and templates for different CVar types
- **CVarTypes.cpp** - Template instantiations
- **IniParser.h** - Ini file parsing functionality
- **IniParser.cpp** - Ini parser implementation
- **ConsoleCommand.h** - Console command interface and built-in commands
- **ConsoleCommand.cpp** - Console command implementation
- **CVarMacros.h** - Macros for easy variable registration
- **CVarExample.cpp** - Example usage of the CVar system

## Features

### Core Components

1. **Console Variable Types**
   - Bool CVar - Boolean values (true/false, 0/1, on/off)
   - Int CVar - Integer values (32-bit)
   - Float CVar - Floating-point values
   - String CVar - String values

2. **Console Variable Manager**
   - Central registry for all console variables
   - Thread-safe operations
   - Handles registration, lookup, and modification
   - Manages loading from and saving to ini files

3. **Ini Configuration System**
   - Parser for ini file format (section/key=value)
   - Handles multiple ini files with priority hierarchy
   - Writes modified values back to appropriate ini files

4. **Console Command Interface**
   - Command-line interface for setting/getting variables
   - Built-in commands: Set, Get, Dump, Reset, Help, Save, Load
   - Support for custom commands via macros

### Console Variable Flags

- `None` - No special flags
- `Cheat` - Marked as cheat (disabled in shipping)
- `Saved` - Saved to ini file
- `RequiresRestart` - Requires restart to take effect
- `ReadOnly` - Read-only after initialization
- `Developer` - Developer-only variable
- `Console` - Visible in console
- `Archive` - Archived but not necessarily saved

## Usage Examples

### Creating Console Variables

```cpp
// Include the macros header
#include "Utility/CVar/CVarMacros.h"

// Simple boolean CVar
AUTO_CVAR_BOOL(r_VSync, true, "Enable vertical sync", EConsoleVariableFlags::Saved)

// Integer CVar
AUTO_CVAR_INT(r_MaxAnisotropy, 8, "Maximum anisotropic filtering", EConsoleVariableFlags::Saved)

// Float CVar
AUTO_CVAR_FLOAT(r_ScreenPercentage, 100.0f, "Screen percentage scale", EConsoleVariableFlags::Saved)

// String CVar
AUTO_CVAR_STRING(g_GameName, "HLVM Engine", "Name of the game", EConsoleVariableFlags::Saved)
```

### Reference Variables

```cpp
// Reference to an external boolean variable
bool g_bUsePostProcessing = true;
AUTO_CVAR_REF_BOOL(r_PostProcessing, g_bUsePostProcessing, "Enable post-processing effects", EConsoleVariableFlags::Saved)

// Reference to an external integer variable
int32_t g_MaxParticles = 1000;
AUTO_CVAR_REF_INT(g_MaxParticles, g_MaxParticles, "Maximum number of particles", EConsoleVariableFlags::Saved)
```

### Runtime Usage

```cpp
// Get values (implicit conversion)
if (CVar_r_VSync) {
    // VSync is enabled
}

int32_t maxAniso = CVar_r_MaxAnisotropy;
float screenPercent = CVar_r_ScreenPercentage;
FString gameName = CVar_g_GameName;

// Direct getter methods
bool vsync = CVar_r_VSync.GetValue();
int32_t maxAniso2 = CVar_r_MaxAnisotropy.GetValue();

// Set values
CVar_r_VSync.SetValue(false);
CVar_r_MaxAnisotropy.SetValue(16);
CVar_r_ScreenPercentage.SetValue(150.0f);

// Using helper macros
SET_CVAR_VALUE(r_VSync, "false");
SET_CVAR_VALUE(r_MaxAnisotropy, "16");

// Reset to default
CVar_r_VSync.ResetToDefault();
RESET_CVAR(r_VSync);

// Check if modified
if (CVar_r_VSync.IsModified()) {
    // Value was changed from default
}
```

### Console Commands

```cpp
// Execute console commands
ConsoleCommandManager::Get().ExecuteCommand("Set r_VSync 1");
ConsoleCommandManager::Get().ExecuteCommand("Get r_MaxAnisotropy");
ConsoleCommandManager::Get().ExecuteCommand("Dump r");
ConsoleCommandManager::Get().ExecuteCommand("Reset r_ScreenPercentage");
ConsoleCommandManager::Get().ExecuteCommand("Help Set");
ConsoleCommandManager::Get().ExecuteCommand("Save Engine.ini");
ConsoleCommandManager::Get().ExecuteCommand("Load Config/Game.ini");
```

### Custom Commands

```cpp
DEFINE_CONSOLE_COMMAND(TestCmd, 
    [](const std::vector<FString>& args) -> bool {
        std::cout << "Test command executed!" << std::endl;
        return true;
    },
    "Test command for demonstration purposes")
```

### Ini File Integration

```cpp
// Load all CVars from default ini files
GetCVarManager().LoadAllFromIni();

// Load from specific ini file
GetCVarManager().LoadFromIni("Config/Custom.ini");

// Save all CVars to ini files
GetCVarManager().SaveAllToIni();

// Save to specific ini file
GetCVarManager().SaveToIni("Config/Custom.ini");
```

### Ini File Format

```ini
[/Script/Engine.Renderer]
r.VSync=1
r.GBufferFormat=2
r.MaxAnisotropy=8

[/Script/Engine.Engine]
bUseOnScreenDebugMessages=True
FrameRateLimit=60

[/Script/Game.BaseGame]
bShowFPS=False
DifficultyLevel=1
PlayerSpeedMultiplier=1.2
```

## Implementation Notes

- The system uses standard C++ library features and avoids engine-specific dependencies
- All string operations use std::string (aliased as FString)
- Thread safety is implemented using std::mutex
- The CVarManager is a singleton pattern for global access
- Auto-registration happens at startup through static initializers
- Boolean CVars support multiple string representations: true/false, 0/1, on/off, yes/no

## Integration with Engine

To integrate this CVar system into the HLVM Engine:

1. Include `CVar.h` and `CVarMacros.h` in relevant engine modules
2. Create CVars using the provided macros for engine configuration
3. Initialize the CVarManager early in engine startup
4. Load ini files before subsystem initialization
5. Save modified CVars on engine shutdown
6. Add console input handling to process commands

The system is designed to be lightweight and performant, with minimal overhead for variable access (inline getters) and thread-safe operations for runtime modifications.