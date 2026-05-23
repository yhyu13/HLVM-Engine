/**
 * FCVarBrowser.cpp
 * ImGui-based CVar browser and editor.
 */

#include "Renderer/ImGui/FCVarBrowser.h"
#include "Utility/CVar/CVar.h"
#include "Core/Log.h"

#include <algorithm>

DECLARE_LOG_CATEGORY(LogCVar)

void FCVarBrowser::DrawUI()
{
    if (!bShowUI)
    {
        return;
    }

    if (!ImGui::Begin(CHARSTR("CVar Browser"), &bShowUI))
    {
        ImGui::End();
        return;
    }

    // Search and filters
    ImGui::InputText(CHARSTR("Search"), SearchBuffer, sizeof(SearchBuffer));
    ImGui::SameLine();
    ImGui::Checkbox(CHARSTR("Saved Only"), &bShowSavedOnly);
    ImGui::SameLine();
    ImGui::Checkbox(CHARSTR("Modified Only"), &bShowModifiedOnly);
    ImGui::Separator();

    CVarManager& Manager = GetCVarManager();
    TArray<ICVar*> AllCVars = Manager.GetAllCVars();

    // Sort by name
    std::sort(AllCVars.begin(), AllCVars.end(),
              [](ICVar* A, ICVar* B) { return A->GetName() < B->GetName(); });

    // Count visible
    int VisibleCount = 0;
    for (ICVar* CVar : AllCVars)
    {
        if (!CVar)
            continue;

        const FString& Name = CVar->GetName();
        if (SearchBuffer[0] != '\0')
        {
            if (Name.find(TCHARSTR(SearchBuffer)) == FString::npos)
                continue;
        }
        if (bShowSavedOnly && !(static_cast<uint32_t>(CVar->GetFlags()) &
                                static_cast<uint32_t>(EConsoleVariableFlag::Saved)))
            continue;
        if (bShowModifiedOnly && !CVar->IsModified())
            continue;

        ++VisibleCount;
    }

    ImGui::Text(CHARSTR("Showing %d / %zu CVars"), VisibleCount, AllCVars.size());

    // Table
    ImGuiTableFlags TableFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable(CHARSTR("CVarTable"), 5, TableFlags))
    {
        ImGui::TableSetupColumn(CHARSTR("Name"), ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn(CHARSTR("Value"), ImGuiTableColumnFlags_WidthStretch, 0.20f);
        ImGui::TableSetupColumn(CHARSTR("Default"), ImGuiTableColumnFlags_WidthStretch, 0.20f);
        ImGui::TableSetupColumn(CHARSTR("Flags"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn(CHARSTR("Help"), ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableHeadersRow();

        for (ICVar* CVar : AllCVars)
        {
            if (!CVar)
                continue;

            const FString& Name = CVar->GetName();
            if (SearchBuffer[0] != '\0')
            {
                if (Name.find(TCHARSTR(SearchBuffer)) == FString::npos)
                    continue;
            }
            if (bShowSavedOnly && !(static_cast<uint32_t>(CVar->GetFlags()) &
                                    static_cast<uint32_t>(EConsoleVariableFlag::Saved)))
                continue;
            if (bShowModifiedOnly && !CVar->IsModified())
                continue;

            ImGui::TableNextRow();

            // Name
            ImGui::TableSetColumnIndex(0);
            if (CVar->IsModified())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", CHARSTR(Name.c_str()));
            }
            else
            {
                ImGui::TextUnformatted(CHARSTR(Name.c_str()));
            }

            // Value (editable)
            ImGui::TableSetColumnIndex(1);
            FString ValueStr = CVar->GetValueAsString();
            bool bIsReadOnly =
                static_cast<uint32_t>(CVar->GetFlags()) &
                static_cast<uint32_t>(EConsoleVariableFlag::ReadOnly);

            if (bIsReadOnly)
            {
                ImGui::TextDisabled("%s", CHARSTR(ValueStr.c_str()));
            }
            else
            {
                char EditBuffer[256];
                std::strncpy(EditBuffer, CHARSTR(ValueStr.c_str()), sizeof(EditBuffer) - 1);
                EditBuffer[sizeof(EditBuffer) - 1] = '\0';

                ImGui::PushID(CVar);
                if (ImGui::InputText(CHARSTR("##value"), EditBuffer, sizeof(EditBuffer),
                                     ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    try
                    {
                        CVar->SetValueFromString(TCHARSTR(EditBuffer));
                        HLVM_LOG(LogCVar, info, TXT("CVar '{}' set to '{}'"), *Name,
                                 TCHARSTR(EditBuffer));
                    }
                    catch (...)
                    {
                        HLVM_LOG(LogCVar, warn, TXT("Failed to set CVar '{}' to '{}'"), *Name,
                                 TCHARSTR(EditBuffer));
                    }
                }
                ImGui::PopID();
            }

            // Default
            ImGui::TableSetColumnIndex(2);
            if (ImGui::Button(CHARSTR("Reset")) && !bIsReadOnly)
            {
                CVar->ResetToDefault();
                HLVM_LOG(LogCVar, info, TXT("CVar '{}' reset to default"), *Name);
            }

            // Flags
            ImGui::TableSetColumnIndex(3);
            EConsoleVariableFlags Flags = CVar->GetFlags();
            FString FlagStr;
            if (static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EConsoleVariableFlag::Saved))
                FlagStr += TXT("S");
            if (static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EConsoleVariableFlag::ReadOnly))
                FlagStr += TXT("R");
            if (static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EConsoleVariableFlag::Cheat))
                FlagStr += TXT("C");
            if (static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EConsoleVariableFlag::Developer))
                FlagStr += TXT("D");
            if (static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EConsoleVariableFlag::Console))
                FlagStr += TXT("O");
            ImGui::TextUnformatted(CHARSTR(FlagStr.c_str()));

            // Help
            ImGui::TableSetColumnIndex(4);
            ImGui::TextDisabled("%s", CHARSTR(CVar->GetHelp().c_str()));
        }

        ImGui::EndTable();
    }

    ImGui::End();
}
