#include "Utility/CVar/CVar.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <mutex>
#include <vector>

CVarManager::CVarManager()
{
	InitializeDefaultIniPaths();
}

CVarManager::~CVarManager()
{
	RegisteredCVars.clear();
}

CVarManager& CVarManager::Get()
{
	static CVarManager Singleton;
	return Singleton;
}

void CVarManager::RegisterCVar(ICVar* cvar)
{
	if (cvar)
	{
		// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
		ATOMIC_LOCK_GUARD(CVarMutex);
		RegisteredCVars[cvar->GetName()] = cvar;
	}
}

ICVar* CVarManager::FindCVar(const FString& name)
{
	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	auto it = RegisteredCVars.find(name);
	return (it != RegisteredCVars.end()) ? it->second : nullptr;
}

bool CVarManager::LoadFromIni(const FString& iniFile)
{
	std::ifstream file(iniFile);
	if (!file.is_open())
	{
		return false;
	}

	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	std::string line;
	std::string currentSection;

	while (std::getline(file, line))
	{
		line.erase(0, line.find_first_not_of(" \t\r\n"));
		line.erase(line.find_last_not_of(" \t\r\n") + 1);

		if (line.empty() || line[0] == ';' || line[0] == '#')
		{
			continue;
		}

		if (line[0] == '[' && line.back() == ']')
		{
			currentSection = line.substr(1, line.length() - 2);
			continue;
		}

		size_t equalPos = line.find('=');
		if (equalPos != std::string::npos)
		{
			std::string key = line.substr(0, equalPos);
			std::string value = line.substr(equalPos + 1);

			key.erase(0, key.find_first_not_of(" \t"));
			key.erase(key.find_last_not_of(" \t") + 1);
			value.erase(0, value.find_first_not_of(" \t"));
			value.erase(value.find_last_not_of(" \t") + 1);

			if (ICVar* cvar = FindCVar(key.c_str()))
			{
				cvar->SetValueFromString(value.c_str());
				cvar->ClearModifiedFlag();
			}
		}
	}

	return true;
}

bool CVarManager::SaveToIni(const FString& iniFile)
{
	std::ofstream file(iniFile);
	if (!file.is_open())
	{
		return false;
	}

	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	file << "[System]\n";

	for (const auto& pair : RegisteredCVars)
	{
		ICVar* cvar = pair.second;
		if (cvar && (static_cast<uint32_t>(cvar->GetFlags()) & static_cast<uint32_t>(EConsoleVariableFlag::Saved)))
		{
			file << cvar->GetName().ToCharCStr() << "=" << cvar->GetValueAsString().ToCharCStr() << "\n";
		}
	}

	return true;
}

void CVarManager::LoadAllFromIni()
{
	TArray<FString> defaultInis = { "Engine.ini", "Game.ini", "System.ini" };

	for (const FString& ini : defaultInis)
	{
		FString fullPath = GetIniFileName(ini);
		if (std::ifstream(fullPath).good())
		{
			LoadFromIni(fullPath);
		}
	}
}

void CVarManager::SaveAllToIni()
{
	TArray<FString> defaultInis = { "Engine.ini", "Game.ini", "System.ini" };

	for (const FString& ini : defaultInis)
	{
		FString fullPath = GetIniFileName(ini);
		SaveToIni(fullPath);
	}
}

bool CVarManager::SetCVarValue(const FString& name, const FString& value)
{
	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	if (ICVar* cvar = FindCVar(name))
	{
		if (!(static_cast<uint32_t>(cvar->GetFlags()) & static_cast<uint32_t>(EConsoleVariableFlag::ReadOnly)))
		{
			cvar->SetValueFromString(value);
			return true;
		}
	}
	return false;
}

FString CVarManager::GetCVarValue(const FString& name)
{
	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	if (ICVar* cvar = FindCVar(name))
	{
		return cvar->GetValueAsString();
	}
	return "";
}

void CVarManager::ResetCVar(const FString& name)
{
	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	if (ICVar* cvar = FindCVar(name))
	{
		if (!(static_cast<uint32_t>(cvar->GetFlags()) & static_cast<uint32_t>(EConsoleVariableFlag::ReadOnly)))
		{
			cvar->ResetToDefault();
		}
	}
}

void CVarManager::ResetAllCVars()
{
	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	for (const auto& pair : RegisteredCVars)
	{
		ICVar* cvar = pair.second;
		if (cvar && !(static_cast<uint32_t>(cvar->GetFlags()) & static_cast<uint32_t>(EConsoleVariableFlag::ReadOnly)))
		{
			cvar->ResetToDefault();
		}
	}
}

bool CVarManager::ProcessConsoleCommand(const FString& command)
{
	std::istringstream iss(command.ToCharCStr());
	std::string		   cmd;
	iss >> cmd;

	if (cmd == "set")
	{
		std::string varName;
		std::string value;
		iss >> varName >> value;
		if (!varName.empty() && !value.empty())
		{
			return SetCVarValue(varName.c_str(), value.c_str());
		}
	}
	else if (cmd == "get")
	{
		std::string varName;
		iss >> varName;
		if (!varName.empty())
		{
			std::string value = GetCVarValue(varName.c_str()).ToCharCStr();
			if (!value.empty())
			{
				std::cout << varName << " = " << value << std::endl;
				return true;
			}
		}
	}
	else if (cmd == "dump")
	{
		std::string category;
		iss >> category;
		if (category.empty())
		{
			DumpAllCVars();
		}
		else
		{
			DumpCVarsByCategory(category.c_str());
		}
		return true;
	}
	else if (cmd == "reset")
	{
		std::string varName;
		iss >> varName;
		if (!varName.empty())
		{
			ResetCVar(varName.c_str());
			return true;
		}
	}
	else if (cmd == "resetAll")
	{
		ResetAllCVars();
		return true;
	}

	return false;
}

void CVarManager::DumpAllCVars()
{
	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	for (const auto& pair : RegisteredCVars)
	{
		ICVar* cvar = pair.second;
		if (cvar)
		{
			std::cout << cvar->GetName().ToCharCStr() << " = " << cvar->GetValueAsString().ToCharCStr();
			if (cvar->IsModified())
			{
				std::cout << " (modified)";
			}
			std::cout << std::endl;
		}
	}
}

void CVarManager::DumpCVarsByCategory(const FString& category)
{
	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	for (const auto& pair : RegisteredCVars)
	{
		ICVar* cvar = pair.second;
		if (cvar && cvar->GetName().find(category) == 0)
		{
			std::cout << cvar->GetName().ToCharCStr() << " = " << cvar->GetValueAsString().ToCharCStr();
			if (cvar->IsModified())
			{
				std::cout << " (modified)";
			}
			std::cout << std::endl;
		}
	}
}

TArray<ICVar*> CVarManager::GetAllCVars() const
{
	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	TArray<ICVar*> result;
	result.reserve(RegisteredCVars.size());

	for (const auto& pair : RegisteredCVars)
	{
		result.push_back(pair.second);
	}

	return result;
}

TArray<ICVar*> CVarManager::GetCVarsByFlag(EConsoleVariableFlags flag) const
{
	// std::lock_guard<std::recursive_mutex> lock(CVarMutex);
	ATOMIC_LOCK_GUARD(CVarMutex);
	TArray<ICVar*> result;

	for (const auto& pair : RegisteredCVars)
	{
		ICVar* cvar = pair.second;
		if (cvar && (static_cast<uint32_t>(cvar->GetFlags()) & static_cast<uint32_t>(flag)))
		{
			result.push_back(cvar);
		}
	}

	return result;
}

void CVarManager::AddIniSearchPath(const FString& path)
{
	IniSearchPaths.push_back(path);
}

FString CVarManager::GetIniFileName(const FString& iniName) const
{
	for (const FString& path : IniSearchPaths)
	{
		FString fullPath = path + TXT("/") + iniName;
		if (std::ifstream(fullPath).good())
		{
			return fullPath;
		}
	}

	return IniSearchPaths.empty() ? iniName : FString{ IniSearchPaths[0] + TXT("/") + iniName };
}

void CVarManager::InitializeDefaultIniPaths()
{
	IniSearchPaths.push_back(".");

	if (std::ifstream("Config/Engine.ini").good())
	{
		IniSearchPaths.push_back("Config");
	}
}
