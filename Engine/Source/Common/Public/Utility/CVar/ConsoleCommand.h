#pragma once

#include "CVar.h"

#include <string>
#include <vector>
#include <functional>

class IConsoleCommand
{
public:
	virtual ~IConsoleCommand() = default;
	virtual bool	Execute(const std::vector<FString>& args) = 0;
	virtual FString GetHelp() const = 0;
	virtual FString GetName() const = 0;
};

class ConsoleCommandManager
{
private:
	static ConsoleCommandManager*				  Singleton;
	std::unordered_map<FString, IConsoleCommand*> Commands;

	ConsoleCommandManager();

public:
	static ConsoleCommandManager& Get();

	void			 RegisterCommand(IConsoleCommand* command);
	IConsoleCommand* FindCommand(const FString& name);

	bool				 ExecuteCommand(const FString& commandLine);
	std::vector<FString> GetCommandNames() const;
	void				 ListCommands() const;
};

class CVarSetCommand : public IConsoleCommand
{
public:
	bool	Execute(const std::vector<FString>& args) override;
	FString GetHelp() const override { return "Set a console variable value: Set <CVarName> <value>"; }
	FString GetName() const override { return "Set"; }
};

class CVarGetCommand : public IConsoleCommand
{
public:
	bool	Execute(const std::vector<FString>& args) override;
	FString GetHelp() const override { return "Get a console variable value: Get <CVarName>"; }
	FString GetName() const override { return "Get"; }
};

class CVarDumpCommand : public IConsoleCommand
{
public:
	bool	Execute(const std::vector<FString>& args) override;
	FString GetHelp() const override { return "Dump all console variables or filter by category: Dump [category]"; }
	FString GetName() const override { return "Dump"; }
};

class CVarResetCommand : public IConsoleCommand
{
public:
	bool	Execute(const std::vector<FString>& args) override;
	FString GetHelp() const override { return "Reset a console variable to default: Reset <CVarName>"; }
	FString GetName() const override { return "Reset"; }
};

class CVarHelpCommand : public IConsoleCommand
{
public:
	bool	Execute(const std::vector<FString>& args) override;
	FString GetHelp() const override { return "Show help for commands: Help [command]"; }
	FString GetName() const override { return "Help"; }
};

class CVarSaveCommand : public IConsoleCommand
{
public:
	bool	Execute(const std::vector<FString>& args) override;
	FString GetHelp() const override { return "Save console variables to ini file: Save [filename]"; }
	FString GetName() const override { return "Save"; }
};

class CVarLoadCommand : public IConsoleCommand
{
public:
	bool	Execute(const std::vector<FString>& args) override;
	FString GetHelp() const override { return "Load console variables from ini file: Load <filename>"; }
	FString GetName() const override { return "Load"; }
};
