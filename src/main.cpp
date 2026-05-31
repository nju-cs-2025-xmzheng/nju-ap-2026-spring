#include "gui/GameApp.hpp"
#include <iostream>

int main() {
    std::cout << "Starting Synera: Slime Tactics..." << std::endl;
    Synera::gui::GameApp app;
    app.Run();
    return 0;
}
