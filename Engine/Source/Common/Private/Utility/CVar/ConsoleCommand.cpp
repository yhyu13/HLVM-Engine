#include "Utility/CVar/ConsoleCommand.h"

#include <sstream>
#include <iostream>
#include <algorithm>

ConsoleCommandManager* ConsoleCommandManager::Singleton = nullptr;

ConsoleCommandManager::ConsoleCommandManager()
{
	RegisterCommand(new CVarSetCommand());
	RegisterCommand(new CVarGetCommand());
	RegisterCommand(new CVarDumpCommand());
	RegisterCommand(new CVarResetCommand());
	RegisterCommand(new CVarHelpCommand());
	RegisterCommand(new CVarSaveCommand());
	RegisterCommand(new CVarLoadCommand());
}

ConsoleCommandManager& ConsoleCommandManager::Get()
{
	if (!Singleton)
	{
		Singleton = new ConsoleCommandManager();
	}
	return *Singleton;
}

void ConsoleCommandManager::RegisterCommand(IConsoleCommand* command)
{
	if (command)
	{
		Commands[command->GetName()] = command;
	}
}

IConsoleCommand* ConsoleCommandManager::FindCommand(const FString& name)
{
	auto it = Commands.find(name);
	return (it != Commands.end()) ? it->second : nullptr;
}

bool ConsoleCommandManager::ExecuteCommand(const FString& commandLine)
{
	std::istringstream iss(commandLine.ToCharCStr());
	std::string		   commandName;
	iss >> commandName;

	if (commandName.empty())
	{
		return false;
	}

	IConsoleCommand* command = FindCommand(commandName.c_str());
	if (!command)
	{
		std::cout << "Unknown command: " << commandName << std::endl;
		std::cout << "Type 'Help' for available commands" << std::endl;
		return false;
	}

	std::vector<FString> args;
	std::string			 arg;
	while (iss >> arg)
	{
		args.push_back(arg.c_str());
	}

	return command->Execute(args);
}

std::vector<FString> ConsoleCommandManager::GetCommandNames() const
{
	std::vector<FString> names;
	names.reserve(Commands.size());

	for (const auto& pair : Commands)
	{
		names.push_back(pair.first);
	}

	return names;
}

void ConsoleCommandManager::ListCommands() const
{
	std::cout << "Available commands:" << std::endl;
	for (const auto& pair : Commands)
	{
		std::cout << "  " << pair.first.ToCharCStr() << " - " << pair.second->GetHelp().ToCharCStr() << std::endl;
	}
}

bool CVarSetCommand::Execute(const std::vector<FString>& args)
{
	if (args.size() < 2)
	{
		std::cout << "Usage: Set <CVarName> <value>" << std::endl;
		return false;
	}

	return GetCVarManager().SetCVarValue(args[0], args[1]);
}

bool CVarGetCommand::Execute(const std::vector<FString>& args)
{
	if (args.size() < 1)
	{
		std::cout << "Usage: Get <CVarName>" << std::endl;
		return false;
	}

	FString value = GetCVarManager().GetCVarValue(args[0]);
	if (value.empty())
	{
		std::cout << "Console variable '" << args[0].ToCharCStr() << "' not found" << std::endl;
		return false;
	}

	std::cout << args[0].ToCharCStr() << " = " << value.ToCharCStr() << std::endl;
	return true;
}

bool CVarDumpCommand::Execute(const std::vector<FString>& args)
{
	if (args.empty())
	{
		GetCVarManager().DumpAllCVars();
	}
	else
	{
		GetCVarManager().DumpCVarsByCategory(args[0]);
	}
	return true;
}

bool CVarResetCommand::Execute(const std::vector<FString>& args)
{
	if (args.size() < 1)
	{
		std::cout << "Usage: Reset <CVarName>" << std::endl;
		return false;
	}

	GetCVarManager().ResetCVar(args[0]);
	std::cout << "Console variable '" << args[0].ToCharCStr() << "' reset to default" << std::endl;
	return true;
}

bool CVarHelpCommand::Execute(const std::vector<FString>& args)
{
	if (args.empty())
	{
		ConsoleCommandManager::Get().ListCommands();
		return true;
	}

	IConsoleCommand* command = ConsoleCommandManager::Get().FindCommand(args[0]);
	if (!command)
	{
		std::cout << "Unknown command: " << args[0].ToCharCStr() << std::endl;
		return false;
	}

	std::cout << command->GetName() << " - " << command->GetHelp() << std::endl;
	return true;
}

bool CVarSaveCommand::Execute(const std::vector<FString>& args)
{
	if (args.empty())
	{
		GetCVarManager().SaveAllToIni();
		std::cout << "Saved all console variables to ini files" << std::endl;
	}
	else
	{
		if (GetCVarManager().SaveToIni(args[0]))
		{
			std::cout << "Saved console variables to " << args[0] << std::endl;
		}
		else
		{
			std::cout << "Failed to save to " << args[0] << std::endl;
			return false;
		}
	}
	return true;
}

bool CVarLoadCommand::Execute(const std::vector<FString>& args)
{
	if (args.size() < 1)
	{
		std::cout << "Usage: Load <filename>" << std::endl;
		return false;
	}

	if (GetCVarManager().LoadFromIni(args[0]))
	{
		std::cout << "Loaded console variables from " << args[0] << std::endl;
		return true;
	}
	else
	{
		std::cout << "Failed to load from " << args[0] << std::endl;
		return false;
	}
}
