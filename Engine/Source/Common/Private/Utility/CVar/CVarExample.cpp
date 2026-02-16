#include "Utility/CVar/CVarMacros.h"

#include <iostream>

AUTO_CVAR_BOOL(r_VSync, true, "Enable vertical sync", EConsoleVariableFlag::Saved)
AUTO_CVAR_INT(r_MaxAnisotropy, 8, "Maximum anisotropic filtering", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ScreenPercentage, 100.0f, "Screen percentage scale", EConsoleVariableFlag::Saved)
AUTO_CVAR_STRING(g_GameName, "HLVM Engine", "Name of the game", EConsoleVariableFlag::Saved)

static bool g_bUsePostProcessing = true;
AUTO_CVAR_REF_BOOL(r_PostProcessing, g_bUsePostProcessing, "Enable post-processing effects", EConsoleVariableFlag::Saved)

static int32_t g_MaxParticles = 1000;
AUTO_CVAR_REF_INT(g_MaxParticles, g_MaxParticles, "Maximum number of particles", EConsoleVariableFlag::Saved)

DEFINE_CONSOLE_COMMAND(TestCmd, [](const std::vector<FString>& args) -> bool {
        std::cout << "Test command executed with " << args.size() << " arguments:" << std::endl;
        for (size_t i = 0; i < args.size(); ++i)
        {
            std::cout << "  " << i << ": " << args[i].ToCharCStr() << std::endl;
        }
        return true; }, "Test command for demonstration purposes")

static void ExampleUsage()
{
	bool		vsyncEnabled = CVar_r_VSync;
	int32_t		maxAniso = CVar_r_MaxAnisotropy;
	float		screenPercent = CVar_r_ScreenPercentage;
	std::string gameName = CVar_g_GameName;

	if (vsyncEnabled)
	{
		std::cout << "VSync is enabled" << std::endl;
	}

	std::cout << "Max anisotropy: " << maxAniso << std::endl;
	std::cout << "Screen percentage: " << screenPercent << "%" << std::endl;
	std::cout << "Game name: " << gameName << std::endl;

	if (g_bUsePostProcessing)
	{
		std::cout << "Post-processing is enabled" << std::endl;
	}

	std::cout << "Max particles: " << g_MaxParticles << std::endl;

	SET_CVAR_VALUE(r_VSync, "false");
	SET_CVAR_VALUE(r_MaxAnisotropy, "16");

	ConsoleCommandManager::Get().ExecuteCommand("Help");
	ConsoleCommandManager::Get().ExecuteCommand("Dump r");
	ConsoleCommandManager::Get().ExecuteCommand("Set r_ScreenPercentage 150.0");

	GetCVarManager().SaveAllToIni();
}

// Example Test method
// int main()
//{
//	ExampleUsage();
//	return 0;
//}
