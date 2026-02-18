#pragma once

#include "CVar.h"

#include <sstream>
#include <algorithm>

template <typename T>
class TTypedCVar : public ICVar
{
private:
	FString				  Name;
	FString				  Help;
	T					  DefaultValue;
	T					  CurrentValue;
	T					  CachedValue;
	EConsoleVariableFlags Flags;
	bool				  bModified;

public:
	TTypedCVar(const TCHAR* name, const T& defaultValue, const TCHAR* help, EConsoleVariableFlags flags = EConsoleVariableFlag::None)
		: Name(name)
		, Help(help)
		, DefaultValue(defaultValue)
		, CurrentValue(defaultValue)
		, CachedValue(defaultValue)
		, Flags(flags)
		, bModified(false)
	{
		GetCVarManager().RegisterCVar(this);
	}

	const FString&		  GetName() const override { return Name; }
	const FString&		  GetHelp() const override { return Help; }
	EConsoleVariableFlags GetFlags() const override { return Flags; }

	const T& GetValue() const { return CurrentValue; }
	operator T() const { return CurrentValue; }

	void SetValue(const T& newValue)
	{
		if (!(Flags & EConsoleVariableFlag::ReadOnly))
		{
			if (CurrentValue != newValue)
			{
				CurrentValue = newValue;
				bModified = true;
			}
		}
	}

	void SetValueFromString(const FString& value) override
	{
		if (!(Flags & EConsoleVariableFlag::ReadOnly))
		{
			std::istringstream iss(value.ToCharCStr());
			T				   newValue;
			iss >> newValue;
			SetValue(newValue);
		}
	}

	FString GetValueAsString() const override
	{
		std::ostringstream oss;
		oss << CurrentValue;
		return oss.str().c_str();
	}

	void ResetToDefault() override
	{
		CurrentValue = DefaultValue;
		bModified = false;
	}

	bool IsModified() const override { return bModified; }
	void ClearModifiedFlag() override { bModified = false; }
};

template <>
class TTypedCVar<bool> : public ICVar
{
private:
	FString				  Name;
	FString				  Help;
	bool				  DefaultValue;
	bool				  CurrentValue;
	EConsoleVariableFlags Flags;
	bool				  bModified;

public:
	TTypedCVar(const TCHAR* name, const bool& defaultValue, const TCHAR* help, EConsoleVariableFlags flags = EConsoleVariableFlag::None)
		: Name(name)
		, Help(help)
		, DefaultValue(defaultValue)
		, CurrentValue(defaultValue)
		, Flags(flags)
		, bModified(false)
	{
		GetCVarManager().RegisterCVar(this);
	}

	const FString&		  GetName() const override { return Name; }
	const FString&		  GetHelp() const override { return Help; }
	EConsoleVariableFlags GetFlags() const override { return Flags; }

	const bool& GetValue() const { return CurrentValue; }
	operator bool() const { return CurrentValue; }

	void SetValue(const bool& newValue)
	{
		if (!(static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EConsoleVariableFlag::ReadOnly)))
		{
			if (CurrentValue != newValue)
			{
				CurrentValue = newValue;
				bModified = true;
			}
		}
	}

	void SetValueFromString(const FString& value) override
	{
		if (!(static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EConsoleVariableFlag::ReadOnly)))
		{
			std::string lowerValue = value.ToCharCStr();
			std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);

			bool newValue = CurrentValue;
			if (lowerValue == "true" || lowerValue == "1" || lowerValue == "on" || lowerValue == "yes")
			{
				newValue = true;
			}
			else if (lowerValue == "false" || lowerValue == "0" || lowerValue == "off" || lowerValue == "no")
			{
				newValue = false;
			}

			SetValue(newValue);
		}
	}

	FString GetValueAsString() const override
	{
		return CurrentValue ? TXT("true") : TXT("false");
	}

	void ResetToDefault() override
	{
		CurrentValue = DefaultValue;
		bModified = false;
	}

	bool IsModified() const override { return bModified; }
	void ClearModifiedFlag() override { bModified = false; }
};

template <>
class TTypedCVar<FString> : public ICVar
{
private:
	FString				  Name;
	FString				  Help;
	std::string			  DefaultValue;
	std::string			  CurrentValue;
	EConsoleVariableFlags Flags;
	bool				  bModified;

public:
	TTypedCVar(const TCHAR* name, const std::string& defaultValue, const TCHAR* help, EConsoleVariableFlags flags = EConsoleVariableFlag::None)
		: Name(name)
		, Help(help)
		, DefaultValue(defaultValue)
		, CurrentValue(defaultValue)
		, Flags(flags)
		, bModified(false)
	{
		GetCVarManager().RegisterCVar(this);
	}

	const FString&		  GetName() const override { return Name; }
	const FString&		  GetHelp() const override { return Help; }
	EConsoleVariableFlags GetFlags() const override { return Flags; }

	const std::string& GetValue() const { return CurrentValue; }
	operator const std::string&() const { return CurrentValue; }

	void SetValue(const std::string& newValue)
	{
		if (!(static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EConsoleVariableFlag::ReadOnly)))
		{
			if (CurrentValue != newValue)
			{
				CurrentValue = newValue;
				bModified = true;
			}
		}
	}

	void SetValueFromString(const FString& value) override
	{
		if (!(static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EConsoleVariableFlag::ReadOnly)))
		{
			SetValue(value.ToCharCStr());
		}
	}

	FString GetValueAsString() const override
	{
		return CurrentValue.c_str();
	}

	void ResetToDefault() override
	{
		CurrentValue = DefaultValue;
		bModified = false;
	}

	bool IsModified() const override { return bModified; }
	void ClearModifiedFlag() override { bModified = false; }
};

using CBoolCVar = TTypedCVar<bool>;
using CIntCVar = TTypedCVar<int32_t>;
using CFloatCVar = TTypedCVar<float>;
using CStringCVar = TTypedCVar<std::string>;
