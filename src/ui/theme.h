#pragma once
#include "imgui.h"
#include <Windows.h>
inline void configureTheme(ImGuiIO &io) {
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowRounding = 9;
    style.FrameRounding = 5;
    style.GrabRounding = 5;
    style.WindowPadding = ImVec2(16, 14);
    style.ItemSpacing = ImVec2(10, 9);
    style.WindowBorderSize = 1;
    style.FrameBorderSize = 0;
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = false;
    style.AntiAliasedFill = true;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.075f, 0.105f, 0.98f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.11f, 0.15f, 1);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.20f, 0.23f, 1);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.14f, 0.19f, 1);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.90f, 0.75f, 1);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.18f, 0.70f, 0.64f, 1);
    style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.28f, 0.30f, 1);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.13f, 0.40f, 0.40f, 1);
    style.Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.28f, 0.30f, 1);
    if (GetFileAttributesW(L"C:\\Windows\\Fonts\\segoeui.ttf") != INVALID_FILE_ATTRIBUTES) {
        ImFontConfig fontConfig{};
        fontConfig.OversampleH = 3;
        fontConfig.OversampleV = 2;
        fontConfig.PixelSnapH = false;
        static const ImWchar baseGlyphs[] = {0x0020, 0x024f, 0x0370, 0x052f, 0};
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &fontConfig, baseGlyphs);
    }
    if (io.Fonts->Fonts.empty())
        io.Fonts->AddFontDefault();
    ImFontConfig mergedFont{};
    mergedFont.MergeMode = true;
    mergedFont.OversampleH = 1;
    mergedFont.OversampleV = 1;
    static const ImWchar symbolGlyphs[] = {0x2000, 0x2bff, 0};
    if (GetFileAttributesW(L"C:\\Windows\\Fonts\\seguisym.ttf") != INVALID_FILE_ATTRIBUTES)
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisym.ttf", 16.0f, &mergedFont, symbolGlyphs);
    if (GetFileAttributesW(L"C:\\Windows\\Fonts\\msyh.ttc") != INVALID_FILE_ATTRIBUTES)
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f, &mergedFont,
                                     io.Fonts->GetGlyphRangesChineseFull());
    style.Colors[ImGuiCol_Tab] = ImVec4(0.07f, 0.11f, 0.15f, 1);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.12f, 0.32f, 0.33f, 1);
    style.Colors[ImGuiCol_TabSelected] = ImVec4(0.10f, 0.25f, 0.27f, 1);
}
