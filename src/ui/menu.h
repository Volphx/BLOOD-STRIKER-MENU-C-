#pragma once
#include "../game/game_reader.h"

inline void menuHint(const char *text) {
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", text);
}
inline void visualCheckbox(const char *label, bool &enabled, const char *hint = nullptr) {
    ImGui::Checkbox(label, &enabled);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s: %s%s%s", label, enabled ? "ligado" : "desligado", hint ? "\n" : "",
                          hint ? hint : "");
}

inline void renderCleanMenu(GameReader &game, Options &options, bool valid, bool &done) {
    ImGui::SetNextWindowSize(ImVec2(650, 460), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(60, 80), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.065f, 0.09f, 0.12f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.15f, 0.20f, 0.25f, 0.65f));
    const bool expanded =
        ImGui::Begin("VZX CHEAT BS###VZXCheatBS", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    if (expanded) {
        ImGui::TextUnformatted("VZX CHEAT BS");
        ImGui::SameLine(ImGui::GetWindowWidth() - 125);
        ImGui::TextColored(game.discoveryBusy() ? ImVec4(0.9f, 0.75f, 0.4f, 1)
                           : valid              ? ImVec4(0.4f, 0.85f, 0.68f, 1)
                                                : ImVec4(0.65f, 0.69f, 0.74f, 1),
                           "%s",
                           game.discoveryBusy() ? "Atualizando..."
                           : valid              ? "Conectado"
                                                : "Sem conexao");
        ImGui::Separator();
        if (ImGui::BeginTabBar("sections")) {
            if (ImGui::BeginTabItem("Visual")) {
                ImGui::Checkbox("Ativar ESP", &options.esp);
                if (ImGui::BeginTable("visual-cards", 2,
                                      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
                    ImGui::TableNextColumn();
                    ImGui::PushID("boxes");
                    if (ImGui::BeginChild("panel", ImVec2(0, 260), ImGuiChildFlags_Borders)) {
                        ImGui::TextDisabled("CAIXAS E LINHAS");
                        ImGui::Separator();
                        visualCheckbox("Box", options.boxes);
                        ImGui::SameLine(100);
                        ImGui::SetNextItemWidth(140);
                        ImGui::BeginDisabled(!options.boxes);
                        ImGui::Combo("##box-style", &options.boxStyle, "Completa\0Corner box\0");
                        ImGui::EndDisabled();
                        visualCheckbox("Linha", options.lines);
                        ImGui::SameLine(100);
                        ImGui::SetNextItemWidth(140);
                        ImGui::BeginDisabled(!options.lines);
                        ImGui::Combo("##line-origin", &options.lineOrigin, "Inferior\0Centro\0Superior\0");
                        ImGui::EndDisabled();
                        ImGui::SetNextItemWidth(145);
                        ImGui::SliderFloat("Espessura", &options.thickness, 0.8f, 3, "%.1f px");
                        ImGui::ColorEdit3("Cor geral", &options.color.x, ImGuiColorEditFlags_NoInputs);
                        ImGui::BeginDisabled(!options.boxes);
                        ImGui::Checkbox("Preenchimento", &options.filled);
                        ImGui::EndDisabled();
                        ImGui::Checkbox("Ponto superior", &options.headDot);
                    }
                    ImGui::EndChild();
                    ImGui::PopID();
                    ImGui::TableNextColumn();
                    ImGui::PushID("skeleton");
                    if (ImGui::BeginChild("panel", ImVec2(0, 260), ImGuiChildFlags_Borders)) {
                        ImGui::TextDisabled("ESQUELETO");
                        ImGui::Separator();
                        visualCheckbox("Esqueleto", options.skeleton);
                        ImGui::BeginDisabled(!options.skeleton);
                        ImGui::SetNextItemWidth(145);
                        ImGui::SliderFloat("Espessura", &options.skeletonThickness, 0.6f, 2.5f, "%.1f px");
                        ImGui::ColorEdit3("Cor", &options.skeletonColor.x, ImGuiColorEditFlags_NoInputs);
                        ImGui::EndDisabled();
                        ImGui::Spacing();
                        ImGui::TextDisabled("INFORMACOES");
                        visualCheckbox("Nome", options.names,
                                       game.isSnapshotMode() ? "Exibido quando houver nome disponivel."
                                                             : nullptr);
                        ImGui::SameLine(140);
                        visualCheckbox("Distancia", options.distances);
                        visualCheckbox("Vida", options.healthBar,
                                       game.isSnapshotMode() ? "Exibida quando a vida estiver disponivel."
                                                             : nullptr);
                        ImGui::SameLine(100);
                        ImGui::SetNextItemWidth(140);
                        ImGui::BeginDisabled(!options.healthBar);
                        ImGui::Combo("##health-position", &options.healthPosition,
                                     "Esquerda\0Direita\0Acima\0Abaixo\0");
                        ImGui::EndDisabled();
                        ImGui::Checkbox("Identificadores", &options.showIds);
                    }
                    ImGui::EndChild();
                    ImGui::PopID();
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Mira")) {
                if (ImGui::BeginChild("aim-card", ImVec2(0, 320), ImGuiChildFlags_Borders)) {
                    ImGui::TextDisabled("CONTROLE DA MIRA");
                    ImGui::Separator();
                    ImGui::Checkbox("Ativar mira", &options.aim);
                    ImGui::SetNextItemWidth(330);
                    ImGui::Combo("Aim Target", &options.aimTarget, "Cabeca\0Pescoco\0Peito\0Pelve\0");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Seleciona o osso real da regiao. Regioes nao identificadas nao "
                                          "sao selecionadas.");
                    menuHint("Acione segurando o botao direito com o menu fechado.");
                    ImGui::SetNextItemWidth(330);
                    ImGui::SliderFloat("Raio", &options.radius, 10, 500, "%.0f px");
                    ImGui::SetNextItemWidth(330);
                    ImGui::SliderFloat("Suavizacao", &options.smoothing, 1, 30, "%.1f");
                    ImGui::Spacing();
                    ImGui::TextDisabled("RECUO");
                    ImGui::Separator();
                    ImGui::Checkbox("Controle de recuo", &options.recoil);
                    ImGui::BeginDisabled(!options.recoil);
                    ImGui::SetNextItemWidth(330);
                    ImGui::SliderFloat("Vertical", &options.recoilVertical, 0, 500, "%.0f /s");
                    ImGui::SetNextItemWidth(330);
                    ImGui::SliderFloat("Horizontal", &options.recoilHorizontal, -200, 200, "%.0f /s");
                    ImGui::EndDisabled();
                    menuHint("Segure mirar + atirar com o menu fechado. Ajuste conforme a arma.");
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Ajustes")) {
                if (ImGui::BeginTable("settings-cards", 2,
                                      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("screen-card", ImVec2(0, 260), ImGuiChildFlags_Borders)) {
                        ImGui::TextDisabled("EXIBICAO");
                        ImGui::Separator();
                        ImGui::Checkbox("Sincronizar quadros", &options.vsync);
                        ImGui::SetNextItemWidth(145);
                        ImGui::SliderInt("Limite", &options.frameLimit, 60, 360, "%d FPS");
                        ImGui::Checkbox("Indicador de desempenho", &options.performanceHud);
                    }
                    ImGui::EndChild();
                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("session-card", ImVec2(0, 260), ImGuiChildFlags_Borders)) {
                        ImGui::TextDisabled("SESSAO");
                        ImGui::Separator();
                        ImGui::BeginDisabled(game.discoveryBusy() || !game.isSnapshotMode());
                        if (ImGui::Button("Atualizar alvos", ImVec2(-1, 30))) {
                            options.aim = false;
                            game.requestDiscovery();
                        }
                        ImGui::EndDisabled();
                        if (ImGui::Button("Recarregar configuracao", ImVec2(-1, 30))) {
                            options.aim = false;
                            game.reload();
                        }
                        ImGui::Spacing();
                        ImGui::TextDisabled("FILTROS");
                        ImGui::Checkbox("Ignorar mortos", &options.ignoreDead);
                        ImGui::BeginDisabled(game.trainingMode);
                        ImGui::Checkbox("Ignorar equipe", &options.teamFilter);
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip(
                                game.trainingMode
                                    ? "Sem filtro de equipe no campo de treino."
                                    : "Exibe apenas adversarios com equipe identificada.\nAlvos sem equipe "
                                      "conhecida ficam ocultos com este filtro.");
                        ImGui::SetNextItemWidth(145);
                        ImGui::SliderFloat("Alcance", &options.maxDistance, 5, 500, "%.0f m");
                    }
                    ImGui::EndChild();
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::SetCursorPosY((std::max)(ImGui::GetCursorPosY(), ImGui::GetWindowHeight() - 46));
        ImGui::Separator();
        ImGui::TextDisabled("INSERT  menu   |   END  sair");
        ImGui::SameLine(ImGui::GetWindowWidth() - 92);
        if (ImGui::Button("Sair", ImVec2(75, 25)))
            done = true;
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}
