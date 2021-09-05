#include <cstdlib>
#include <cstdint>

#include "imtui/imtui.h"
#include "imtui/imtui-impl-ncurses.h"
#include "imtui/imtui-demo.h"
#include "imtui/imgui_memory_editor.h"

uint32_t* vmem;
uint32_t* regs;
uint32_t pc = 0;
static MemoryEditor mem_edit;

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();

    bool demo = true;

    // set up virtual memory, we'll treat this as the base DMA address
    vmem = (uint32_t*) malloc(0x10000/4);
    regs = (uint32_t*) malloc(8*sizeof(uint32_t));

    while (true) {
        ImTui_ImplNcurses_NewFrame();
        ImTui_ImplText_NewFrame();

        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Reset")) {
                // TODO: reset the simulation
            }
            if (ImGui::BeginMenu("Help")) {
                // TODO: show help
            }
            if (ImGui::BeginMenu("Quit")) {
                break;
            }
            ImGui::EndMainMenuBar();
        }

        // ImTui::ShowDemoWindow(&demo);
        mem_edit.DrawContents(vmem, 0x10000/4, (size_t) 0x38A00000);

        ImGui::Render();

        ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
        ImTui_ImplNcurses_DrawScreen();
    }

    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();

    return 0;
}
