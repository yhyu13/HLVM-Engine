#include "Utility/CVar/IniParser.h"

#include <fstream>
#include <sstream>
#include <algorithm>

bool IniParser::ParseFile(const FString& filename)
{
    Sections.clear();
    
    std::ifstream file(filename);
    if (!file.is_open())
    {
        return false;
    }
    
    IniSection* currentSection = nullptr;
    std::string line;
    
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
            std::string sectionName = line.substr(1, line.length() - 2);
            Sections.push_back({sectionName.c_str(), {}});
            currentSection = &Sections.back();
            continue;
        }
        
        size_t equalPos = line.find('=');
        if (equalPos != std::string::npos && currentSection)
        {
			std::string key = line.substr(0, equalPos);
			std::string value = line.substr(equalPos + 1);
            
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            currentSection->KeyValues[key] = value;
        }
    }
    
    return true;
}

bool IniParser::SaveToFile(const FString& filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        return false;
    }
    
    for (const auto& section : Sections)
    {
        file << "[" << section.Name.ToCharCStr() << "]\n";
        
        for (const auto& kv : section.KeyValues)
        {
            file << kv.first.ToCharCStr() << "=" << kv.second.ToCharCStr() << "\n";
        }
        
        file << "\n";
    }
    
    return true;
}
