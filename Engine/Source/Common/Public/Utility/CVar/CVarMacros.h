#pragma once

#include "CVarTypes.h"
#include "ConsoleCommand.h"

#include <functional>

#define AUTO_CVAR_BOOL(name, defaultValue, help, flags)                                                 \
	static CBoolCVar						   CVar_##name(TXT(#name), defaultValue, TXT(help), flags); \
	static FAutoConsoleVariableRegistrar<CBoolCVar> Registrar_##name(&CVar_##name);

#define AUTO_CVAR_INT(name, defaultValue, help, flags)                                                     \
	static CIntCVar								  CVar_##name(TXT(#name), defaultValue, TXT(help), flags); \
	static FAutoConsoleVariableRegistrar<CIntCVar> Registrar_##name(&CVar_##name);

#define AUTO_CVAR_FLOAT(name, defaultValue, help, flags)                                                 \
	static CFloatCVar							CVar_##name(TXT(#name), defaultValue, TXT(help), flags); \
	static FAutoConsoleVariableRegistrar<CFloatCVar> Registrar_##name(&CVar_##name);

#define AUTO_CVAR_STRING(name, defaultValue, help, flags)                                                      \
	static CStringCVar								  CVar_##name(TXT(#name), defaultValue, TXT(help), flags); \
	static FAutoConsoleVariableRegistrar<CStringCVar> Registrar_##name(&CVar_##name);

#define AUTO_CVAR_REF_BOOL(name, refVar, help, flags) \
	static FAutoConsoleVariableRef<bool> CVarRef_##name(TXT(#name), refVar, TXT(help), flags);

#define AUTO_CVAR_REF_INT(name, refVar, help, flags) \
	static FAutoConsoleVariableRef<int32_t> CVarRef_##name(TXT(#name), refVar, TXT(help), flags);

#define AUTO_CVAR_REF_FLOAT(name, refVar, help, flags) \
	static FAutoConsoleVariableRef<float> CVarRef_##name(TXT(#name), refVar, TXT(help), flags);

#define AUTO_CVAR_REF_STRING(name, refVar, help, flags) \
	static FAutoConsoleVariableRef<std::string> CVarRef_##name(TXT(#name), refVar, TXT(help), flags);

#define DEFINE_CONSOLE_COMMAND(name, executor, help)                                      \
	class ConsoleCommand_##name : public IConsoleCommand                                  \
	{                                                                                     \
	public:                                                                               \
		bool Execute(const std::vector<FString>& _args) override                           \
		{                                                                                 \
			return executor(_args);                                                        \
		}                                                                                 \
		FString GetHelp() const override                                                  \
		{                                                                                 \
			return TXT(help);                                                             \
		}                                                                                 \
		FString GetName() const override                                                  \
		{                                                                                 \
			return TXT(#name);                                                            \
		}                                                                                 \
	};                                                                                    \
	static ConsoleCommand_##name ConsoleCommandInstance_##name;                           \
	static struct ConsoleCommandRegistrar_##name                                          \
	{                                                                                     \
		ConsoleCommandRegistrar_##name()                                                  \
		{                                                                                 \
			ConsoleCommandManager::Get().RegisterCommand(&ConsoleCommandInstance_##name); \
		}                                                                                 \
	} StaticConsoleCommandRegistrar_##name;

#define CVAR_GETTER(type, name)         \
	const type& Get##name##CVar() const \
	{                                   \
		return CVar_##name.GetValue();  \
	}

#define CVAR_SETTER(type, name)             \
	void Set##name##CVar(const type& value) \
	{                                       \
		CVar_##name.SetValue(value);        \
	}

#define CVAR_ACCESSORS(type, name) \
	CVAR_GETTER(type, name)        \
	CVAR_SETTER(type, name)
