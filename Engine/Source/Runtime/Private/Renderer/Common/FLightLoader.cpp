// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/Common/FLightLoader.h"

#include "Renderer/Common/FLightBuilder.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <cstdint>
#include <fstream>
#include <sstream>

namespace Renderer
{
    namespace
    {
        bool ReadFileToString(const std::string& FilePath, std::string& OutText, std::string& OutError)
        {
            std::ifstream file(FilePath, std::ios::ate | std::ios::binary);
            if (!file.is_open())
            {
                OutError = "Failed to open file: " + FilePath;
                return false;
            }

            const std::streamsize size = file.tellg();
            if (size < 0)
            {
                OutError = "Failed to determine file size: " + FilePath;
                return false;
            }

            file.seekg(0, std::ios::beg);
            OutText.resize(static_cast<size_t>(size));
            if (!file.read(OutText.data(), size))
            {
                OutError = "Failed to read file: " + FilePath;
                return false;
            }

            return true;
        }

        bool ParseFloat3(const rapidjson::Value& ArrayValue, const char* Name, float Out[3], std::string& OutError)
        {
            if (!ArrayValue.HasMember(Name))
            {
                OutError = std::string("Missing required field: ") + Name;
                return false;
            }

            const rapidjson::Value& arr = ArrayValue[Name];
            if (!arr.IsArray() || arr.Size() < 3)
            {
                OutError = std::string("Field '") + Name + "' must be a 3-element array";
                return false;
            }

            for (rapidjson::SizeType i = 0; i < 3; ++i)
            {
                if (!arr[i].IsNumber())
                {
                    OutError = std::string("Field '") + Name + "' contains non-numeric value";
                    return false;
                }
                Out[i] = arr[i].GetFloat();
            }
            return true;
        }

        float ParseFloatDefault(const rapidjson::Value& Object, const char* Name, float DefaultValue)
        {
            if (!Object.HasMember(Name) || !Object[Name].IsNumber())
            {
                return DefaultValue;
            }
            return Object[Name].GetFloat();
        }

        bool ParseLight(const rapidjson::Value& LightValue, FLight& OutLight, std::string& OutError)
        {
            if (!LightValue.IsObject())
            {
                OutError = "Each light entry must be an object";
                return false;
            }

            if (!LightValue.HasMember("type") || !LightValue["type"].IsString())
            {
                OutError = "Light entry missing 'type' string field";
                return false;
            }

            const std::string type = LightValue["type"].GetString();
            float color[3] = { 1.0f, 1.0f, 1.0f };
            float intensity = 1.0f;

            if (LightValue.HasMember("color"))
            {
                if (!ParseFloat3(LightValue, "color", color, OutError))
                {
                    return false;
                }
            }
            intensity = ParseFloatDefault(LightValue, "intensity", 1.0f);

            if (type == "directional")
            {
                float direction[3] = { 0.577f, 0.577f, 0.577f };
                if (LightValue.HasMember("direction"))
                {
                    if (!ParseFloat3(LightValue, "direction", direction, OutError))
                    {
                        return false;
                    }
                }
                OutLight = MakeDirectionalLight(direction, color, intensity);
                return true;
            }

            if (type == "point")
            {
                float position[3] = { 0.0f, 0.0f, 0.0f };
                if (LightValue.HasMember("position"))
                {
                    if (!ParseFloat3(LightValue, "position", position, OutError))
                    {
                        return false;
                    }
                }
                const float range = ParseFloatDefault(LightValue, "range", 10.0f);
                OutLight = MakePointLight(position, range, color, intensity);
                return true;
            }

            if (type == "spot")
            {
                float position[3] = { 0.0f, 0.0f, 0.0f };
                float direction[3] = { 0.0f, -1.0f, 0.0f };
                if (LightValue.HasMember("position"))
                {
                    if (!ParseFloat3(LightValue, "position", position, OutError))
                    {
                        return false;
                    }
                }
                if (LightValue.HasMember("direction"))
                {
                    if (!ParseFloat3(LightValue, "direction", direction, OutError))
                    {
                        return false;
                    }
                }
                const float range = ParseFloatDefault(LightValue, "range", 10.0f);
                const float inner = ParseFloatDefault(LightValue, "innerConeAngle", 0.0f);
                const float outer = ParseFloatDefault(LightValue, "outerConeAngle", 0.785398f);
                OutLight = MakeSpotLight(position, direction, range, inner, outer, color, intensity);
                return true;
            }

            if (type == "area")
            {
                float position[3] = { 0.0f, 0.0f, 0.0f };
                float direction[3] = { 0.0f, -1.0f, 0.0f };
                if (LightValue.HasMember("position"))
                {
                    if (!ParseFloat3(LightValue, "position", position, OutError))
                    {
                        return false;
                    }
                }
                if (LightValue.HasMember("direction"))
                {
                    if (!ParseFloat3(LightValue, "direction", direction, OutError))
                    {
                        return false;
                    }
                }
                const float width = ParseFloatDefault(LightValue, "width", 1.0f);
                const float height = ParseFloatDefault(LightValue, "height", 1.0f);
                OutLight = MakeAreaLight(position, direction, width, height, color, intensity);
                return true;
            }

            OutError = "Unknown light type: " + type;
            return false;
        }
    } // namespace

    bool LoadLightsFromJSON(const std::string&    JsonText,
                            std::vector<FLight>&  OutLights,
                            std::string&          OutError)
    {
        OutLights.clear();
        OutError.clear();

        rapidjson::Document document;
        rapidjson::ParseResult parseResult = document.Parse(JsonText.c_str());
        if (!parseResult)
        {
            std::ostringstream oss;
            oss << "JSON parse error at offset " << parseResult.Offset() << ": "
                << rapidjson::GetParseError_En(parseResult.Code());
            OutError = oss.str();
            return false;
        }

        if (!document.IsArray())
        {
            OutError = "Light list JSON must be a top-level array";
            return false;
        }

        OutLights.reserve(document.Size());
        for (rapidjson::SizeType i = 0; i < document.Size(); ++i)
        {
            FLight light{};
            if (!ParseLight(document[i], light, OutError))
            {
                OutError = "Light[" + std::to_string(i) + "]: " + OutError;
                OutLights.clear();
                return false;
            }
            OutLights.push_back(light);
        }

        return true;
    }

    bool LoadLightsFromJSONFile(const std::string&   FilePath,
                                std::vector<FLight>& OutLights,
                                std::string&         OutError)
    {
        std::string text;
        if (!ReadFileToString(FilePath, text, OutError))
        {
            return false;
        }
        return LoadLightsFromJSON(text, OutLights, OutError);
    }

    nvrhi::BufferHandle UploadLightsFromJSONFile(nvrhi::IDevice*      Device,
                                                 const std::string&   FilePath,
                                                 std::string&         OutError,
                                                 uint32_t&            OutCount)
    {
        OutCount = 0;
        std::vector<FLight> lights;
        if (!LoadLightsFromJSONFile(FilePath, lights, OutError))
        {
            return nullptr;
        }

        nvrhi::BufferHandle buffer = UploadLightBuffer(Device, lights.data(), lights.size());
        if (buffer)
        {
            OutCount = static_cast<uint32_t>(lights.size());
        }
        else
        {
            OutError = "Failed to upload lights buffer";
        }
        return buffer;
    }
} // namespace Renderer
