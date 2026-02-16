#pragma once

#include "Core/String.h"
#include "Core/Container/ContainerDefinition.h"

class IniParser
{
public:
    struct IniSection
    {
        FString Name;
        TMapSmall<FString, FString> KeyValues;
    };
    
private:
    std::vector<IniSection> Sections;
    
public:
    bool ParseFile(const FString& filename);
    bool SaveToFile(const FString& filename);
    
    const std::vector<IniSection>& GetSections() const { return Sections; }
    
    const IniSection* FindSection(const FString& sectionName) const
    {
        for (const auto& section : Sections)
        {
            if (section.Name == sectionName)
            {
                return &section;
            }
        }
        return nullptr;
    }
    
    FString GetValue(const FString& section, const FString& key, const FString& defaultValue = "") const
    {
        const IniSection* sec = FindSection(section);
        if (sec)
        {
            auto it = sec->KeyValues.find(key);
            if (it != sec->KeyValues.end())
            {
                return it->second;
            }
        }
        return defaultValue;
    }
    
    void SetValue(const FString& section, const FString& key, const FString& value)
    {
        IniSection* sec = nullptr;
        
        for (auto& s : Sections)
        {
            if (s.Name == section)
            {
                sec = &s;
                break;
            }
        }
        
        if (!sec)
        {
            Sections.push_back({section, {}});
            sec = &Sections.back();
        }
        
        sec->KeyValues[key] = value;
    }
    
    void ClearSection(const FString& section)
    {
        Sections.erase(
            std::remove_if(Sections.begin(), Sections.end(),
                [&section](const IniSection& sec) { return sec.Name == section; }),
            Sections.end());
    }
    
    void ClearKey(const FString& section, const FString& key)
    {
        for (auto& sec : Sections)
        {
            if (sec.Name == section)
            {
                sec.KeyValues.erase(key);
                break;
            }
        }
    }
};
