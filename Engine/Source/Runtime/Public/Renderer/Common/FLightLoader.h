// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Renderer/Common/FLight.h"

#include <nvrhi/nvrhi.h>

#include <string>
#include <vector>

namespace Renderer
{
    // Parse a JSON light-list file into a vector of FLight values.
    //
    // Expected schema (array of light objects):
    // [
    //   { "type": "directional", "direction": [x,y,z], "color": [r,g,b], "intensity": 1.0 },
    //   { "type": "point",       "position": [x,y,z], "range": 10.0, ... },
    //   { "type": "spot",        "position": [x,y,z], "direction": [x,y,z], "range": 10.0,
    //                              "innerConeAngle": 0.5, "outerConeAngle": 0.8, ... },
    //   { "type": "area",        "position": [x,y,z], "direction": [x,y,z],
    //                              "width": 2.0, "height": 1.0, ... }
    // ]
    //
    // Returns true on success; on failure OutError contains a message and OutLights is left empty.
    bool LoadLightsFromJSON(const std::string&    JsonText,
                            std::vector<FLight>&  OutLights,
                            std::string&          OutError);

    // Convenience: read the file from disk then parse it.
    bool LoadLightsFromJSONFile(const std::string&   FilePath,
                                std::vector<FLight>& OutLights,
                                std::string&         OutError);

    // Convenience: parse JSON and upload directly to a GPU structured buffer.
    nvrhi::BufferHandle UploadLightsFromJSONFile(nvrhi::IDevice*      Device,
                                                 const std::string&   FilePath,
                                                 std::string&         OutError,
                                                 uint32_t&            OutCount);
} // namespace Renderer
