/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * ImGui Console Widget Implementation
 *
 * Displays log messages in an ImGui window.
 * Simplified version - log display only, no command execution.
 */

#include "Renderer/ImGui/FImgui_Console.h"

#include <cstdarg>
#include <cstdio>

ImVec4 FImgui_Console::GetSeverityColor(int severity)
{
	switch (severity)
	{
		case 0: // Info
			return ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
		case 1: // Warning
			return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
		case 2: // Error
			return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		default:
			return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

FImgui_Console::FLogItem FImgui_Console::MakeLogItem(int severity, const char* message)
{
	FLogItem item;
	item.Severity = severity;
	item.TextColor = GetSeverityColor(severity);
	item.Text = message;
	return item;
}

FImgui_Console::FImgui_Console(const FConsoleOptions& options)
	: Options(options)
{
}

FImgui_Console::~FImgui_Console()
{
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
void FImgui_Console::Print(const char* fmt, ...)
{
	char	buffer[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
	va_end(args);
	buffer[sizeof(buffer) - 1] = 0;

	ItemsLog.push_back(MakeLogItem(0, buffer));
}
#pragma clang diagnostic pop

void FImgui_Console::Print(std::string_view line)
{
	ItemsLog.push_back(MakeLogItem(0, std::string(line).c_str()));
}

void FImgui_Console::ClearLog()
{
	ItemsLog.clear();
}

void FImgui_Console::ClearHistory()
{
	HistoryIndex = -1;
	InputBuffer[0] = 0;
}

void FImgui_Console::Render(bool* open)
{
	if (!ImGui::Begin("Console", open, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Close Console"))
		{
			if (open)
				*open = false;
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Clear Log"))
			{
				ClearLog();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	ImGui::BeginChild("Log panel", ImVec2(0, -footerHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::Selectable("Clear"))
		{
			ClearLog();
		}
		ImGui::EndPopup();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

	for (const auto& item : ItemsLog)
	{
		bool showItem = true;
		switch (item.Severity)
		{
			case 0:
				showItem = Options.ShowInfo;
				break;
			case 1:
				showItem = Options.ShowWarnings;
				break;
			case 2:
				showItem = Options.ShowErrors;
				break;
			default:
				break;
		}

		if (showItem)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, item.TextColor);
			ImGui::TextUnformatted(item.Text.c_str());
			ImGui::PopStyleColor();
		}
	}

	if (Options.ScrollToBottom || (Options.AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
	{
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();

	ImGui::Separator();

	ImGui::AlignTextToFramePadding();
	ImGui::Text("Filters: ");
	ImGui::SameLine();
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Border, GetSeverityColor(2));
	ImGui::Checkbox("Errors", &Options.ShowErrors);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Border, GetSeverityColor(1));
	ImGui::Checkbox("Warnings", &Options.ShowWarnings);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Border, GetSeverityColor(0));
	ImGui::Checkbox("Info", &Options.ShowInfo);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	ImGui::End();
}
