#pragma once

#include "Core/String.h"
#include "IniParser.h"

class ICVar;
class CVarManager;

enum class EConsoleVariableFlag : uint32_t
{
	None = 0,
	Cheat = 1 << 0,
	Saved = 1 << 1,
	RequiresRestart = 1 << 2,
	ReadOnly = 1 << 3,
	Developer = 1 << 4,
	Console = 1 << 5,
	Archive = 1 << 6
};
HLVM_ENMU_CLASS_FLAGS(EConsoleVariableFlag, EConsoleVariableFlags)

class ICVar
{
public:
	virtual ~ICVar() = default;
	virtual const FString&		  GetName() const = 0;
	virtual const FString&		  GetHelp() const = 0;
	virtual void				  SetValueFromString(const FString& value) = 0;
	virtual FString				  GetValueAsString() const = 0;
	virtual void				  ResetToDefault() = 0;
	virtual bool				  IsModified() const = 0;
	virtual void				  ClearModifiedFlag() = 0;
	virtual EConsoleVariableFlags GetFlags() const = 0;
};

class CVarManager
{
private:
	TMap<FString, ICVar*> RegisteredCVars;
	TArray<FString>		  IniSearchPaths;
	// mutable std::recursive_mutex CVarMutex;
	mutable FRecursiveAtomicFlag CVarMutex;

	CVarManager();
	~CVarManager();

public:
	static CVarManager& Get();

	void   RegisterCVar(ICVar* cvar);
	ICVar* FindCVar(const FString& name);

	bool LoadFromIni(const FString& iniFile);
	bool SaveToIni(const FString& iniFile);
	void LoadAllFromIni();
	void SaveAllToIni();

	bool	SetCVarValue(const FString& name, const FString& value);
	FString GetCVarValue(const FString& name);
	void	ResetCVar(const FString& name);
	void	ResetAllCVars();

	bool ProcessConsoleCommand(const FString& command);
	void DumpAllCVars();
	void DumpCVarsByCategory(const FString& category);

	TArray<ICVar*> GetAllCVars() const;
	TArray<ICVar*> GetCVarsByFlag(EConsoleVariableFlags flag) const;

	void				   AddIniSearchPath(const FString& path);
	const TArray<FString>& GetIniSearchPaths() const { return IniSearchPaths; }

private:
	FString GetIniFileName(const FString& iniName) const;
	void	InitializeDefaultIniPaths();
};

inline CVarManager& GetCVarManager()
{
	return CVarManager::Get();
}

#define GET_CVAR_VALUE(varName) GetCVarManager().GetCVarValue(TXT(#varName))
#define SET_CVAR_VALUE(varName, value) GetCVarManager().SetCVarValue(TXT(#varName), TXT(value))
#define RESET_CVAR(varName) GetCVarManager().ResetCVar(TXT(#varName))

template <typename T>
class FAutoConsoleVariableRegistrar
{
public:
	FAutoConsoleVariableRegistrar(T* cvar)
	{
		CVarManager::Get().RegisterCVar(static_cast<ICVar*>(cvar));
	}
};

template <typename T>
class FAutoConsoleVariableRef : public ICVar
{
private:
	FString				  Name;
	FString				  Help;
	T*					  ExternalValue;
	T					  DefaultValue;
	EConsoleVariableFlags Flags;
	bool				  bModified;

public:
	FAutoConsoleVariableRef(const TCHAR* name, T& refVar, const TCHAR* help, EConsoleVariableFlags flags = EConsoleVariableFlag::None);

	const FString&		  GetName() const override { return Name; }
	const FString&		  GetHelp() const override { return Help; }
	EConsoleVariableFlags GetFlags() const override { return Flags; }

	void	SetValueFromString(const FString& value) override;
	FString GetValueAsString() const override;

	void ResetToDefault() override;
	bool IsModified() const override { return bModified; }
	void ClearModifiedFlag() override { bModified = false; }

	const T& GetValue() const { return *ExternalValue; }
	operator T() const { return *ExternalValue; }
	void SetValue(const T& newValue);
};

template <typename T>
FAutoConsoleVariableRef<T>::FAutoConsoleVariableRef(const TCHAR* name, T& refVar, const TCHAR* help, EConsoleVariableFlags flags)
	: Name(name)
	, Help(help)
	, ExternalValue(&refVar)
	, DefaultValue(refVar)
	, Flags(flags)
	, bModified(false)
{
	GetCVarManager().RegisterCVar(this);
}

template <typename T>
void FAutoConsoleVariableRef<T>::SetValueFromString(const FString& value)
{
	if (ExternalValue)
	{
		std::istringstream iss(value.ToCharCStr());
		T				   newValue;
		iss >> newValue;
		if (*ExternalValue != newValue)
		{
			*ExternalValue = newValue;
			bModified = true;
		}
	}
}

template <typename T>
FString FAutoConsoleVariableRef<T>::GetValueAsString() const
{
	if (ExternalValue)
	{
		std::ostringstream oss;
		oss << *ExternalValue;
		return oss.str().c_str();
	}
	return "";
}

template <typename T>
void FAutoConsoleVariableRef<T>::ResetToDefault()
{
	if (ExternalValue)
	{
		*ExternalValue = DefaultValue;
		bModified = false;
	}
}

template <typename T>
void FAutoConsoleVariableRef<T>::SetValue(const T& newValue)
{
	if (ExternalValue && !(static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EConsoleVariableFlag::ReadOnly)))
	{
		if (*ExternalValue != newValue)
		{
			*ExternalValue = newValue;
			bModified = true;
		}
	}
}

template class FAutoConsoleVariableRef<bool>;
template class FAutoConsoleVariableRef<int32_t>;
template class FAutoConsoleVariableRef<float>;
template class FAutoConsoleVariableRef<std::string>;
