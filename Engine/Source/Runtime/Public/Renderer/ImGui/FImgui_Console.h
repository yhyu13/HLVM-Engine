/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * ImGui Console Widget
 *
 * Displays log messages in an ImGui window.
 * Simplified version - log display only, no command execution.
 */

#pragma once

#include <imgui.h>

#include <string>
#include <string_view>
#include <vector>



struct FConsoleOptions
{
    bool AutoScroll = true;
    bool ScrollToBottom = false;
    bool ShowInfo = true;
    bool ShowWarnings = true;
    bool ShowErrors = true;
};

class FImgui_Console
{
public:
    FImgui_Console(const FConsoleOptions& options = FConsoleOptions());
    ~FImgui_Console();

    void Print(const char* fmt, ...);
    void Print(std::string_view line);

    void ClearLog();
    void ClearHistory();

    void Render(bool* open = nullptr);

private:
    struct FLogItem
    {
        int Severity = 0;
        ImVec4 TextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        std::string Text;
    };

    static ImVec4 GetSeverityColor(int severity);
    static FLogItem MakeLogItem(int severity, const char* message);

    FConsoleOptions Options;
    std::vector<FLogItem> ItemsLog;
    int HistoryIndex = -1;
    char InputBuffer[256] = { 0 };
};

