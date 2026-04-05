// Copyright 2025 HLVM Contributors
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
#include <cstdint>

struct ImFont;

class FRegisteredFont
{
    friend class FImgui_Renderer;

protected:
    ImFont* m_imFont = nullptr;
    float m_sizeAtDefaultScale = 0.f;
    bool m_isDefault = false;

public:
    // Creates an invalid font that will not add any ImGui fonts
    FRegisteredFont()
    { }

    // Creates a default font with the given size
    FRegisteredFont(float size)
        : m_sizeAtDefaultScale(size)
        , m_isDefault(true)
    { }

    FRegisteredFont(void* data, size_t dataSize, float fontSize)
        : m_sizeAtDefaultScale(fontSize)
        , m_isDefault(false)
    {
        (void)data;
        (void)dataSize;
    }

    // Returns true if custom font data exists
    bool HasFontData() const
    {
        return !m_isDefault && m_imFont != nullptr;
    }

    // Returns the ImFont object that can be used with ImGui.
    // The returned pointer may be NULL if the font has failed to load.
    ImFont* GetScaledFont()
    {
        return m_imFont;
    }
};
