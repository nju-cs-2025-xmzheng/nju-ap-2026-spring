#include "gui/GameApp.hpp"
#include <iostream>

int main() {
    std::cout << "Starting Synera: Synergy Auto-Arena (Raylib 3D)..." << std::endl;
    Synera::gui::GameApp app;
    app.Run();
    return 0;
}
