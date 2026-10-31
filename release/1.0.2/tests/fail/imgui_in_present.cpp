#include <imgui.h>

int Probe()
{
    return ImGui::GetIO().WantTextInput ? 1 : 0;
}
