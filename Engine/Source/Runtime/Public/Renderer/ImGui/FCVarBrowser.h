#pragma once

#include "Core/String.h"
#include "Core/Container/ContainerDefinition.h"

#include <imgui.h>

/**
 * @brief ImGui-based CVar browser and editor
 *
 * Displays all registered console variables in a searchable table.
 * Supports in-place editing for non-ReadOnly CVars.
 */
class FCVarBrowser
{
public:
    FCVarBrowser() = default;

    // Call inside an active ImGui::NewFrame context.
    void DrawUI();

    void SetOpen(bool bOpen) { bShowUI = bOpen; }
    [[nodiscard]] bool IsOpen() const { return bShowUI; }

private:
    bool bShowUI = true;
    char SearchBuffer[256] = {};
    bool bShowSavedOnly = false;
    bool bShowModifiedOnly = false;
};
