// Copyright 2024 HLVM Engine
// 
// MIT License
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#include <nvrhi/nvrhi.h>

struct FShaderMacro
{
    std::string Name;
    std::string Definition;

    FShaderMacro() = default;
    FShaderMacro(std::string inName, std::string inDefinition)
        : Name(std::move(inName))
        , Definition(std::move(inDefinition))
    {}
};

struct FStaticShader
{
    const void* Bytecode = nullptr;
    size_t Size = 0;

    FStaticShader() = default;
    FStaticShader(const void* inBytecode, size_t inSize)
        : Bytecode(inBytecode)
        , Size(inSize)
    {}
};

class FShaderFactory
{
public:
    virtual ~FShaderFactory() = default;

    virtual nvrhi::ShaderHandle CreateShader(
        const std::filesystem::path& fileName,
        const std::string& entryName,
        const std::vector<FShaderMacro>* defines,
        const nvrhi::ShaderDesc& desc) = 0;

    virtual nvrhi::ShaderHandle CreateStaticShader(
        const FStaticShader& shader,
        const std::vector<FShaderMacro>* defines,
        const nvrhi::ShaderDesc& desc) = 0;

    virtual nvrhi::ShaderHandle CreateAutoShader(
        const std::filesystem::path& fileName,
        const std::string& entryName,
        const FStaticShader& dxbc,
        const FStaticShader& dxil,
        const FStaticShader& spirv,
        const std::vector<FShaderMacro>* defines,
        const nvrhi::ShaderDesc& desc) = 0;
};

class FShaderFactoryImpl : public FShaderFactory
{
public:
    bool Initialize(nvrhi::IDevice* device);

    nvrhi::ShaderHandle CreateShader(
        const std::filesystem::path& fileName,
        const std::string& entryName,
        const std::vector<FShaderMacro>* defines,
        const nvrhi::ShaderDesc& desc) override;

    nvrhi::ShaderHandle CreateStaticShader(
        const FStaticShader& shader,
        const std::vector<FShaderMacro>* defines,
        const nvrhi::ShaderDesc& desc) override;

    nvrhi::ShaderHandle CreateAutoShader(
        const std::filesystem::path& fileName,
        const std::string& entryName,
        const FStaticShader& dxbc,
        const FStaticShader& dxil,
        const FStaticShader& spirv,
        const std::vector<FShaderMacro>* defines,
        const nvrhi::ShaderDesc& desc) override;

private:
    nvrhi::DeviceHandle Device;
};
