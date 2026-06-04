#include "gui/GameApp.hpp"
#include "common/Serialization.hpp"
#include "raymath.h"
#include "rlgl.h"
#include "unit/Synergy.hpp"
#include "unit/UnitImpl.hpp" // IWYU pragma: keep
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace Synera::gui {

using namespace Synera::engine;
using namespace Synera::unit;

namespace {

constexpr const char *kDefaultMultiplayerHost = "0.0.0.0";
constexpr std::uint16_t kDefaultMultiplayerPort = 39090;
constexpr const char *kDefaultMultiplayerAddress = "0.0.0.0:39090";

constexpr Rectangle MultiplayerAddressBounds() {
    return Rectangle{430.0f, 205.0f, 420.0f, 42.0f};
}

// Shop anchored to the bottom-right corner: five recruit cards plus a single
// action block (same footprint as one card) in one row. The cards float freely
// (no backing panel / title); ShopPanelBounds is only the logical hit zone used
// for "drag a unit here to sell".
constexpr float kShopCardW = 118.0f;
constexpr float kShopCardH = 132.0f;
constexpr float kShopGap = 8.0f;
constexpr float kShopPad = 10.0f;
constexpr int kShopCardCount = 5;

constexpr Rectangle ShopPanelBounds() {
    float content_w =
        (kShopCardCount + 1) * kShopCardW + kShopCardCount * kShopGap;
    float w = content_w + kShopPad * 2.0f;
    float h = kShopCardH + kShopPad * 2.0f;
    return Rectangle{1280.0f - w - 16.0f, 720.0f - h - 16.0f, w, h};
}

// Top-left X of the p-th block (0..4 are recruit cards, 5 is the action block).
constexpr float ShopBlockX(int p) {
    return ShopPanelBounds().x + kShopPad + p * (kShopCardW + kShopGap);
}

constexpr float ShopRowY() { return ShopPanelBounds().y + kShopPad; }

engine::ConnectionConfig ParseMultiplayerAddress(const std::string &input) {
    std::string host = kDefaultMultiplayerHost;
    std::string port_text = std::to_string(kDefaultMultiplayerPort);

    std::string address =
        input.empty() ? std::string(kDefaultMultiplayerAddress) : input;
    size_t colon = address.rfind(':');
    if (colon == std::string::npos) {
        if (!address.empty()) {
            host = address;
        }
    } else {
        host = address.substr(0, colon);
        port_text = address.substr(colon + 1);
        if (host.empty()) {
            host = kDefaultMultiplayerHost;
        }
    }

    int port = port_text.empty() ? kDefaultMultiplayerPort
                                 : std::atoi(port_text.c_str());
    if (port < 1 || port > 65535) {
        port = kDefaultMultiplayerPort;
    }

    return engine::ConnectionConfig{host, static_cast<std::uint16_t>(port)};
}

std::string FormatMultiplayerAddress(const engine::ConnectionConfig &config) {
    return config.host + ":" + std::to_string(config.port);
}

bool IsMultiplayerAddressChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' ||
           ch == '-' || ch == ':';
}

} // namespace

struct SaveMetadata {
    bool exists = false;
    int hp = 0;
    int gold = 0;
    int level = 0;
    int round = 0;
};

static SaveMetadata GetSaveMetadata(int slot) {
    SaveMetadata meta;
    std::string filepath = std::string(GetApplicationDirectory()) +
                           "save_slot_" + std::to_string(slot) + ".txt";
    std::ifstream in(filepath);
    if (!in) {
        meta.exists = false;
        return meta;
    }
    meta.exists = true;
    std::string line;
    std::string section = "";
    while (std::getline(in, line)) {
        if (line.empty())
            continue;
        if (line == "[player]") {
            section = "player";
            continue;
        } else if (line.starts_with("[")) {
            break; // only need player info
        }
        if (section == "player") {
            std::istringstream iss(line);
            std::string key;
            int val;
            if (iss >> key >> val) {
                if (key == "hp")
                    meta.hp = val;
                else if (key == "gold")
                    meta.gold = val;
                else if (key == "level")
                    meta.level = val;
                else if (key == "round")
                    meta.round = val;
            }
        }
    }
    return meta;
}

Color GetElementColor(unit::Element elem) {
    switch (elem) {
    case unit::Element::Pyro:
        return RED;
    case unit::Element::Hydro:
        return BLUE;
    case unit::Element::Anemo:
        return SKYBLUE;
    case unit::Element::Geo:
        return GOLD;
    case unit::Element::Electro:
        return PURPLE;
    case unit::Element::Cryo:
        return Color{180, 220, 255, 255};
    }
    return WHITE;
}

std::string GetElementName(unit::Element elem) {
    switch (elem) {
    case unit::Element::Pyro:
        return "Pyro";
    case unit::Element::Hydro:
        return "Hydro";
    case unit::Element::Anemo:
        return "Anemo";
    case unit::Element::Geo:
        return "Geo";
    case unit::Element::Electro:
        return "Electro";
    case unit::Element::Cryo:
        return "Cryo";
    }
    return "Unknown";
}

// Element a piece of equipment is themed after (for tooltip accent color)
unit::Element EquipmentElement(const unit::Equipment &eq) {
    if (std::holds_alternative<unit::HydroDrop>(eq))
        return unit::Element::Hydro;
    if (std::holds_alternative<unit::AnemoDrop>(eq))
        return unit::Element::Anemo;
    if (std::holds_alternative<unit::GeoDrop>(eq))
        return unit::Element::Geo;
    if (std::holds_alternative<unit::ElectroDrop>(eq))
        return unit::Element::Electro;
    if (std::holds_alternative<unit::CryoDrop>(eq))
        return unit::Element::Cryo;
    return unit::Element::Pyro;
}

// Human-readable stat bonus / effect granted by a piece of equipment
std::string EquipmentEffect(const unit::Equipment &eq) {
    if (std::holds_alternative<unit::PyroDrop>(eq))
        return "ATK +15";
    if (std::holds_alternative<unit::HydroDrop>(eq))
        return "Max HP +150";
    if (std::holds_alternative<unit::AnemoDrop>(eq))
        return "Max Mana -30 (faster skills)";
    if (std::holds_alternative<unit::GeoDrop>(eq))
        return "Shield +100";
    if (std::holds_alternative<unit::ElectroDrop>(eq))
        return "Normal attack strikes twice";
    if (std::holds_alternative<unit::CryoDrop>(eq))
        return "Takes reduced damage";
    return "";
}

GameApp::GameApp() {
    // 1. Initialize Raylib window
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "Synera: Slime Tactics");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    // 2. Initialize Camera
    camera_.position = {0.0f, 80.0f, -10.0f};
    camera_.target = {0.0f, 0.0f, -20.0f};
    camera_.up = {0.0f, 1.0f, 0.0f};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;

    target_cam_pos_ = camera_.position;
    target_cam_target_ = camera_.target;

    // 3. Load Hexagon Mesh (pointy-topped hexagonal cylinder)
    // 6 slices gives a hexagon cylinder. Radius 0.47f gives a nice gap with
    // spacing 0.88f.
    Mesh hexMesh = GenMeshCylinder(0.47f, 0.15f, 6);
    hex_model_ = LoadModelFromMesh(hexMesh);

    // 4. Load Exo 2 Fonts (high-res load using LoadFontEx)
    font_regular_ = LoadFontEx("assets/fonts/Exo2-Regular.ttf", 96, NULL, 0);
    if (font_regular_.texture.id == 0) {
        font_regular_ =
            LoadFontEx("../assets/fonts/Exo2-Regular.ttf", 96, NULL, 0);
    }
    font_bold_ = LoadFontEx("assets/fonts/Exo2-Bold.ttf", 96, NULL, 0);
    if (font_bold_.texture.id == 0) {
        font_bold_ = LoadFontEx("../assets/fonts/Exo2-Bold.ttf", 96, NULL, 0);
    }

    if (font_regular_.texture.id != 0) {
        SetTextureFilter(font_regular_.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (font_bold_.texture.id != 0) {
        SetTextureFilter(font_bold_.texture, TEXTURE_FILTER_BILINEAR);
    }

    // Initialize Multiplayer Address InputBox
    InputBoxStyle input_style;
    input_style.font = font_regular_;
    input_style.fontSize = 16.0f;
    input_style.fontSpacing = 1.0f;
    input_style.paddingLeft = 14.0f;
    input_style.paddingRight = 14.0f;
    input_style.paddingTop = 12.0f;
    input_style.paddingBottom = 12.0f;
    input_style.borderWidth = 1.0f;
    input_style.borderRadius = 0.0f;
    input_style.bgNormal = Color{28, 28, 34, 255};
    input_style.bgHover = Color{28, 28, 34, 255};
    input_style.bgActive = Color{42, 48, 64, 255};
    input_style.borderNormal = Color{110, 110, 120, 255};
    input_style.borderHover = GOLD;
    input_style.borderActive = GOLD;
    input_style.textNormal = WHITE;
    input_style.textPlaceholder = Color{150, 150, 160, 255};
    input_style.cursorColor = GOLD;

    multiplayer_address_input_ =
        InputBox(MultiplayerAddressBounds(), kDefaultMultiplayerAddress)
            .SetStyle(input_style)
            .SetCharValidator(IsMultiplayerAddressChar);

    // 4.5. Load custom alpha-discard shader with directional lighting to
    // prevent black backgrounds on transparent textures and add depth/shading
    const char *alphaVs = R"(
        #version 330
        in vec3 vertexPosition;
        in vec3 vertexNormal;
        in vec2 vertexTexCoord;
        in vec4 vertexColor;
        out vec2 fragTexCoord;
        out vec4 fragColor;
        out vec3 fragNormal;
        uniform mat4 mvp;
        uniform mat4 matModel;
        void main() {
            fragTexCoord = vertexTexCoord;
            fragColor = vertexColor;
            fragNormal = normalize(vec3(matModel * vec4(vertexNormal, 0.0)));
            gl_Position = mvp * vec4(vertexPosition, 1.0);
        }
    )";

    const char *alphaFs = R"(
        #version 330
        in vec2 fragTexCoord;
        in vec4 fragColor;
        in vec3 fragNormal;
        out vec4 finalColor;
        uniform sampler2D texture0;
        uniform vec4 colDiffuse;
        void main() {
            vec4 texelColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
            if (texelColor.a < 0.1) discard;
            
            // Simple directional light (from top-right-front)
            vec3 lightDir = normalize(vec3(0.4, 0.9, 0.3));
            float diff = max(dot(normalize(fragNormal), lightDir), 0.0);
            
            // Ambient light (base visibility)
            float ambient = 0.35;
            // Combined light intensity factor
            float light = min(ambient + diff * 0.65, 1.0);
            
            finalColor = vec4(texelColor.rgb * light, texelColor.a);
        }
    )";
    alpha_shader_ = LoadShaderFromMemory(alphaVs, alphaFs);
    hex_model_.materials[0].shader = alpha_shader_;

    // 5. Load Arena Model (Minecraft-style landscape GLB)
    if (FileExists("assets/scenes/arena.glb")) {
        arena_model_ = LoadModel("assets/scenes/arena.glb");
    } else if (FileExists("../assets/scenes/arena.glb")) {
        arena_model_ = LoadModel("../assets/scenes/arena.glb");
    } else {
        arena_model_ = {0};
    }

    // 6. Load elemental icons (indexed by unit::Element: Anemo, Geo, Electro,
    // Cryo, Pyro, Hydro)
    const char *icon_names[6] = {"Anemo", "Geo",  "Electro",
                                 "Cryo",  "Pyro", "Hydro"};
    for (int i = 0; i < 6; ++i) {
        std::string base =
            std::string("assets/elements/Element_") + icon_names[i] + "_White";
        std::string path = base + ".png";
        if (!FileExists(path.c_str()))
            path = std::string("../") + path;
        if (FileExists(path.c_str())) {
            element_icons_[i] = LoadTexture(path.c_str());
            SetTextureFilter(element_icons_[i], TEXTURE_FILTER_BILINEAR);
        }
    }
    {
        std::string path = "assets/elements/Element_Canopy_White.png";
        if (!FileExists(path.c_str()))
            path = std::string("../") + path;
        if (FileExists(path.c_str())) {
            canopy_icon_ = LoadTexture(path.c_str());
            SetTextureFilter(canopy_icon_, TEXTURE_FILTER_BILINEAR);
        }
    }

    // Set texture filter to POINT for pixelated/voxel look and apply alpha
    // discard shader
    if (arena_model_.meshCount > 0 && arena_model_.materials != nullptr) {
        for (int i = 0; i < arena_model_.materialCount; ++i) {
            arena_model_.materials[i].shader = alpha_shader_;
            if (arena_model_.materials[i].maps != nullptr &&
                arena_model_.materials[i].maps[MATERIAL_MAP_ALBEDO].texture.id >
                    0) {
                SetTextureFilter(
                    arena_model_.materials[i].maps[MATERIAL_MAP_ALBEDO].texture,
                    TEXTURE_FILTER_POINT);
            }
        }
    }
}

GameApp::~GameApp() {
    UnloadModel(hex_model_);
    if (arena_model_.meshCount > 0) {
        UnloadModel(arena_model_);
    }
    UnloadShader(alpha_shader_);
    UnloadFont(font_regular_);
    UnloadFont(font_bold_);
    for (auto &icon : element_icons_) {
        if (icon.id != 0)
            UnloadTexture(icon);
    }
    if (canopy_icon_.id != 0)
        UnloadTexture(canopy_icon_);
    CloseWindow();
}

void GameApp::Run() {
    while (!WindowShouldClose() && !exit_flag_) {
        Update();
        Draw();
    }
}

void GameApp::ApplyModeUpdate(const engine::ModeUpdate &update) {
    if (!update.status.empty()) {
        status_msg_ = update.status;
        status_msg_timer_ = update.status_timer;
        if (state_ == GameState::MultiplayerMenu &&
            multiplayer_menu_state_ != MultiplayerMenuState::Setup) {
            multiplayer_status_ = update.status;
        }
    }
    if (update.clear_visuals) {
        slimes_.clear();
        projectiles_.clear();
    }
    if (update.enter_gameplay) {
        game_in_progress_ = true;
        multiplayer_menu_state_ = MultiplayerMenuState::Setup;
        multiplayer_status_.clear();
        multiplayer_address_input_.SetFocused(false);
        state_ = GameState::Gameplay;
    }
    if (update.enter_settlement) {
        state_ = GameState::Settlement;
        player_won_game_ = update.player_won_game;
    }
}

Vector3 GameApp::GetHexWorldPos(engine::HexCoord coord) {
    float spacing = 1.0f;
    float vertical_spacing = spacing * 0.866f; // spacing * sin(60)

    float cx = (coord.c + (coord.r & 1 ? 0.5f : 0.0f) - 3.5f) * spacing;
    float cz = (coord.r - 3.5f) * vertical_spacing;
    return {cx, 0.0f, cz};
}

Vector3 GameApp::GetBenchWorldPos(int slot) {
    float spacing = 1.0f;
    float vertical_spacing = spacing * 0.866f;
    float gap = 0.3f;
    float cx = (slot - 3.5f) * spacing;
    float cz = 4.5f * vertical_spacing + gap;
    return {cx, 0.0f, cz};
}

Vector3 GameApp::GetEquipWorldPos(int slot) {
    float spacing = 1.0f;
    float vertical_spacing = spacing * 0.866f;
    float gap = 0.3f;
    float cx = (slot + 0.5f - 3.5f) * spacing;
    float cz = 5.5f * vertical_spacing + gap;
    return {cx, 0.0f, cz};
}

Vector3 GameApp::GetWorldPos(engine::Coord coord) {
    if (std::holds_alternative<engine::HexCoord>(coord)) {
        return GetHexWorldPos(std::get<engine::HexCoord>(coord));
    } else {
        return GetBenchWorldPos(std::get<engine::LinearCoord>(coord).x);
    }
}

std::pair<engine::Coord, bool> GameApp::GetCellUnderMouse() {
    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera_);

    // Find intersection with the horizontal grid plane (Y = 0)
    if (ray.direction.y >= 0) {
        return {engine::LinearCoord{0}, false};
    }

    float t = -ray.position.y / ray.direction.y;
    Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));

    float min_dist = 0.6f; // click threshold
    engine::Coord best_coord;
    bool found = false;

    // 1. Search Board grid cells
    int start_row = engine::is_combat(mode_) ? 0 : 4;
    for (int r = start_row; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            engine::HexCoord coord{r, c};
            Vector3 pos = GetHexWorldPos(coord);
            float dist = Vector2Distance({hit.x, hit.z}, {pos.x, pos.z});
            if (dist < min_dist) {
                min_dist = dist;
                best_coord = coord;
                found = true;
            }
        }
    }

    // 2. Search Bench cells
    for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
        engine::LinearCoord coord{i};
        Vector3 pos = GetBenchWorldPos(i);
        float dist = Vector2Distance({hit.x, hit.z}, {pos.x, pos.z});
        if (dist < min_dist) {
            min_dist = dist;
            best_coord = coord;
            found = true;
        }
    }

    return {best_coord, found};
}

bool GameApp::IsMouseOverUI() {
    Vector2 m = GetMousePosition();
    if (CheckCollisionPointRec(m, ShopPanelBounds()))
        return true;
    if (m.y <= 60)
        return true;
    if (m.x >= 20 && m.x <= 240 && m.y >= 120 && m.y <= 480)
        return true;
    if (engine::result_announced(mode_))
        return true;
    return false;
}

void GameApp::Update() {
    ApplyModeUpdate(engine::poll_mode(mode_));

    // 1. Smoothly interpolate camera position and target depending on
    // state/phase
    if (state_ == GameState::StartMenu) {
        target_cam_pos_ = {0.0f, 50.0f, 30.0f};
        target_cam_target_ = {0.0f, 0.0f, 0.0f};
    } else if (state_ == GameState::MainMenu) {
        target_cam_pos_ = {0.0f, 3.0f, 30.0f};
        target_cam_target_ = {0.0f, 0.0f, 0.0f};
    } else if (state_ == GameState::MultiplayerMenu) {
        target_cam_pos_ = {0.0f, 3.0f, 30.0f};
        target_cam_target_ = {0.0f, 0.0f, 0.0f};
    } else if (state_ == GameState::Settlement) {
        if (player_won_game_) {
            target_cam_pos_ = {0.0f, 50.0f, 30.0f}; // same as StartMenu
            target_cam_target_ = {0.0f, 0.0f, 0.0f};
        } else {
            target_cam_pos_ = {0.0f, 15.0f, 3.0f}; // same as Combat
            target_cam_target_ = {0.0f, 0.0f, 1.4f};
        }
    } else { // GameState::Gameplay
        if (engine::is_combat(mode_)) {
            target_cam_pos_ = {0.0f, 15.0f, 3.0f};
            target_cam_target_ = {0.0f, 0.0f, 1.4f};
        } else {
            target_cam_pos_ = {0.0f, 8.5f, 9.0f};
            target_cam_target_ = {0.0f, 0.0f, 4.2f};
        }
    }
    camera_.position =
        Vector3Lerp(camera_.position, target_cam_pos_, GetFrameTime() * 4.0f);
    camera_.target =
        Vector3Lerp(camera_.target, target_cam_target_, GetFrameTime() * 4.0f);

    // 2. Decrement status message timer and menu cooldowns
    if (status_msg_timer_ > 0.0f) {
        status_msg_timer_ -= GetFrameTime();
    }
    if (menu_transition_cooldown_ > 0.0f) {
        menu_transition_cooldown_ -= GetFrameTime();
    }

    // 3. Handle inputs depending on state
    if (state_ == GameState::StartMenu) {
        UpdateStartMenu();
    } else if (state_ == GameState::MainMenu) {
        UpdateMainMenu();
    } else if (state_ == GameState::MultiplayerMenu) {
        UpdateMultiplayerMenu();
    } else if (state_ == GameState::Settlement) {
        UpdateSettlement();
    } else {
        if (engine::can_prepare(mode_)) {
            HandleInputs();
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            state_ = GameState::MainMenu;
            menu_transition_cooldown_ = 0.2f;
            is_slot_menu_ = false;
            // Clear selection and dragging states to avoid visual artifacts
            has_selection_ = false;
            is_dragging_ = false;
            has_hover_ = false;
            hovered_equip_index_ = -1;
            selected_equip_index_ = -1;
            is_dragging_equip_ = false;
            drag_equip_source_index_ = -1;
        }
    }

    // 4. Combat loop update
    if (state_ == GameState::Gameplay && engine::is_combat(mode_) &&
        !engine::result_announced(mode_)) {
        combat_tick_timer += GetFrameTime();
        while (combat_tick_timer >= combat_tick_interval) {
            combat_tick_timer -= combat_tick_interval;
            ProcessCombatTick();
            if (engine::result_announced(mode_))
                break;
        }
    }

    // 5. Update animation progress for visual slimes
    float dt = GetFrameTime();
    for (auto &pair : slimes_) {
        VisualSlime &v = pair.second;
        if (v.is_dead) {
            if (v.death_time < 1.0f) {
                v.death_time += dt * 2.0f;
            }
            continue;
        }

        // Idle bounce phase
        v.bounce_phase += dt * 5.0f;

        // Slide movement animation
        if (v.move_time < 1.3f) {
            v.move_time += dt * 3.5f;
            if (v.move_time > 1.3f)
                v.move_time = 1.3f;
        }

        // Attack lunge animation
        if (v.attack_time < 1.0f) {
            v.attack_time += dt * 8.0f;
            if (v.attack_time > 1.0f)
                v.attack_time = 1.0f;
        }

        // Hurt wobble animation
        if (v.hurt_time < 1.0f) {
            v.hurt_time += dt * 6.0f;
            if (v.hurt_time > 1.0f)
                v.hurt_time = 1.0f;
        }

        // Cast inflation animation
        if (v.cast_time < 1.0f) {
            v.cast_time += dt * 4.0f;
            if (v.cast_time > 1.0f)
                v.cast_time = 1.0f;
        }
    }

    // 6. Monitor Board to update/synchronize VisualSlime states
    engine::Board &active_board = engine::active_board(mode_);

    // Add or update active units
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            engine::HexCoord coord{r, c};
            if (auto u_ptr = get_unit(active_board, coord)) {
                auto &stats = unit::stats(*u_ptr);
                Vector3 wpos = GetHexWorldPos(coord);

                if (slimes_.find(u_ptr) == slimes_.end()) {
                    // New unit visual state
                    VisualSlime v;
                    if (stats.owner == unit::Owner::EnemyCtrl) {
                        v.current_pos = {wpos.x, 0.0f,
                                         -6.0f}; // Slide/jump in from top
                    } else {
                        v.current_pos = wpos;
                    }
                    v.target_pos = wpos;
                    v.move_time = 0.0f; // Slide-in jump
                    v.last_coord = coord;
                    v.last_hp = stats.hp;
                    v.last_mana = stats.mana;
                    v.initialized = true;
                    slimes_[u_ptr] = v;
                } else {
                    VisualSlime &v = slimes_[u_ptr];

                    // Slide movement trigger
                    if (!(v.last_coord == Coord{coord})) {
                        if (v.was_dragged) {
                            v.was_dragged = false;
                        } else {
                            v.current_pos = GetWorldPos(v.last_coord);
                        }
                        v.target_pos = wpos;
                        v.move_time = 0.0f;
                        v.last_coord = Coord{coord};
                    }

                    // Hurt trigger
                    if (stats.hp < v.last_hp) {
                        v.hurt_time = 0.0f;
                        v.last_hp = stats.hp;
                    } else {
                        v.last_hp = stats.hp;
                    }

                    // Attack trigger
                    if (stats.state == unit::State::Attacking &&
                        stats.attack_cooldown == stats.attack_interval - 1) {
                        if (v.attack_time >= 1.0f) {
                            v.attack_time = 0.0f;
                            // Find direction to target
                            if (auto target_opt = select_target(
                                    active_board, *u_ptr, coord)) {
                                Vector3 target_pos =
                                    GetHexWorldPos(*target_opt);
                                v.attack_dir = Vector3Normalize(
                                    Vector3Subtract(target_pos, wpos));
                            } else {
                                v.attack_dir = {0.0f, 0.0f, -1.0f};
                            }
                        }
                    }

                    // Cast skill trigger
                    if (stats.mana < v.last_mana &&
                        v.last_mana >= stats.max_mana) {
                        v.cast_time = 0.0f;
                        // Spawn skill particle
                        if (auto target_opt =
                                select_target(active_board, *u_ptr, coord)) {
                            VisualProjectile proj;
                            proj.current_pos = wpos;
                            proj.current_pos.y += 0.4f;
                            proj.target_pos = GetHexWorldPos(*target_opt);
                            proj.target_pos.y += 0.4f;
                            proj.speed = 4.0f;
                            proj.color = GetElementColor(unit::element(*u_ptr));
                            proj.radius = 0.15f;
                            proj.is_area =
                                (unit::element(*u_ptr) == unit::Element::Pyro ||
                                 unit::element(*u_ptr) == unit::Element::Cryo);
                            projectiles_.push_back(proj);
                        }
                    }
                    v.last_mana = stats.mana;
                }
            }
        }
    }

    // Add or update bench units
    for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
        engine::LinearCoord coord{i};
        if (auto u_ptr = get_unit(active_board, coord)) {
            auto &stats = unit::stats(*u_ptr);
            Vector3 wpos = GetBenchWorldPos(i);

            if (slimes_.find(u_ptr) == slimes_.end()) {
                VisualSlime v;
                v.current_pos = {wpos.x, 0.0f,
                                 6.0f}; // Slide/jump in from bottom
                v.target_pos = wpos;
                v.move_time = 0.0f; // Slide-in jump
                v.last_coord = coord;
                v.last_hp = stats.hp;
                v.last_mana = stats.mana;
                v.initialized = true;
                slimes_[u_ptr] = v;
            } else {
                VisualSlime &v = slimes_[u_ptr];
                if (!(v.last_coord == Coord{coord})) {
                    if (v.was_dragged) {
                        v.was_dragged = false;
                    } else {
                        v.current_pos = GetWorldPos(v.last_coord);
                    }
                    v.target_pos = wpos;
                    v.move_time = 0.0f;
                    v.last_coord = Coord{coord};
                }
                v.last_hp = stats.hp;
                v.last_mana = stats.mana;
            }
        }
    }

    // Handle dead units (mark VisualSlimes as dead)
    for (auto &pair : slimes_) {
        auto u_ptr = pair.first;
        VisualSlime &v = pair.second;
        if (v.is_dead)
            continue;

        // Check if unit is still present on the board/bench
        bool present = false;
        for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
            for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                if (get_unit(active_board, engine::HexCoord{r, c}) == u_ptr)
                    present = true;
            }
        }
        for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
            if (get_unit(active_board, engine::LinearCoord{i}) == u_ptr)
                present = true;
        }

        if (!present || unit::stats(*u_ptr).hp <= 0) {
            v.is_dead = true;
            v.death_time = 0.0f;
        }
    }

    // 7. Update active projectiles
    for (auto it = projectiles_.begin(); it != projectiles_.end();) {
        it->progress += dt * it->speed;
        if (it->progress >= 1.0f) {
            if (it->is_area) {
                // Trigger area explosion ring animation
                it->current_pos = it->target_pos;
                it->progress = 0.99f; // stays at target
                it->is_area = false;  // complete traveling
                it->speed = 2.0f;     // expansion speed
                it->radius = 0.1f;    // start size
                // we'll let it expand for a few frames
            }
            if (it->progress >= 1.0f) {
                it = projectiles_.erase(it);
                continue;
            }
        }

        if (!it->is_area && it->progress < 1.0f) {
            it->current_pos =
                Vector3Lerp(it->current_pos, it->target_pos, it->progress);
        } else {
            // Expand ring
            it->radius += dt * it->speed * it->max_radius;
            if (it->radius >= it->max_radius) {
                it = projectiles_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void GameApp::UpdateStartMenu() {
    if (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        state_ = GameState::MainMenu;
        menu_transition_cooldown_ = 0.25f;
    }
}

void GameApp::DrawStartMenu() {
    DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.4f));

    const char *title = "Synera: Slime Tactics";
    int fontSize = 64;
    int textWidth = MeasureGameText(title, fontSize, true);
    int posX = 640 - textWidth / 2;
    int posY = 280;

    // Glowing border/shadow effect
    DrawGameText(title, posX - 2, posY - 2, fontSize, BLACK, true);
    DrawGameText(title, posX + 2, posY - 2, fontSize, BLACK, true);
    DrawGameText(title, posX - 2, posY + 2, fontSize, BLACK, true);
    DrawGameText(title, posX + 2, posY + 2, fontSize, BLACK, true);
    DrawGameText(title, posX, posY, fontSize, GOLD, true);

    // Pulse prompt
    float pulse = sin(GetTime() * 3.0f) * 0.5f + 0.5f;
    Color promptColor = Fade(LIGHTGRAY, 0.3f + pulse * 0.7f);
    const char *prompt = "PRESS ANY KEY TO START";
    int promptSize = 22;
    int promptWidth = MeasureGameText(prompt, promptSize, false);
    DrawGameText(prompt, 640 - promptWidth / 2, 420, promptSize, promptColor,
                 false);
}

void GameApp::UpdateMainMenu() {
    if (game_in_progress_ && IsKeyPressed(KEY_ESCAPE)) {
        state_ = GameState::Gameplay;
    }
}

void GameApp::DrawMainMenu() {
    DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.55f));

    const char *title = "Synera: Slime Tactics";
    int fontSize = 48;
    int textWidth = MeasureGameText(title, fontSize, true);
    DrawGameText(title, 640 - textWidth / 2, 100, fontSize, GOLD, true);

    if (game_in_progress_) {
        const char *status = "GAME PAUSED";
        int statusWidth = MeasureGameText(status, 16, false);
        DrawGameText(status, 640 - statusWidth / 2, 160, 16, GRAY, false);
    }

    auto draw_menu_btn = [&](const char *text, int y, bool enabled) -> bool {
        int x = 640 - 140;
        int w = 280;
        int h = 45;
        bool hover = enabled && CheckCollisionPointRec(
                                    GetMousePosition(),
                                    {(float)x, (float)y, (float)w, (float)h});

        Color base_color =
            hover ? Color{50, 60, 90, 255} : Color{30, 30, 38, 255};
        Color border_color = enabled ? (hover ? GOLD : Color{80, 80, 100, 255})
                                     : Color{50, 50, 55, 255};
        Color text_color = enabled ? (hover ? WHITE : LIGHTGRAY) : GRAY;

        DrawRectangle(x, y, w, h, base_color);
        DrawRectangleLines(x, y, w, h, border_color);

        int text_w = MeasureGameText(text, 18, true);
        DrawGameText(text, x + w / 2 - text_w / 2, y + 13, 18, text_color,
                     true);

        return hover && (menu_transition_cooldown_ <= 0.0f) &&
               IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    };

    if (is_slot_menu_) {
        const char *subTitle =
            is_saving_mode_ ? "SELECT SAVE SLOT" : "SELECT LOAD SLOT";
        int subWidth = MeasureGameText(subTitle, 24, true);
        DrawGameText(subTitle, 640 - subWidth / 2, 180, 24, GOLD, true);

        for (int i = 1; i <= 3; ++i) {
            SaveMetadata meta = GetSaveMetadata(i);
            std::string label;
            if (meta.exists) {
                label = "Slot " + std::to_string(i) + ": Rd " +
                        std::to_string(meta.round) + " (Lvl " +
                        std::to_string(meta.level) + ", " +
                        std::to_string(meta.gold) + "G)";
            } else {
                label = "Slot " + std::to_string(i) + ": [Empty Slot]";
            }

            int slot_y = 230 + (i - 1) * 65;
            int slot_x = 640 - 140;
            int slot_w = meta.exists ? 210 : 280;
            int slot_h = 45;

            // Draw slot button
            bool slot_hover = CheckCollisionPointRec(
                GetMousePosition(),
                {(float)slot_x, (float)slot_y, (float)slot_w, (float)slot_h});
            Color slot_base =
                slot_hover ? Color{50, 60, 90, 255} : Color{30, 30, 38, 255};
            Color slot_border = slot_hover ? GOLD : Color{80, 80, 100, 255};
            Color slot_text_color = slot_hover ? WHITE : LIGHTGRAY;

            DrawRectangle(slot_x, slot_y, slot_w, slot_h, slot_base);
            DrawRectangleLines(slot_x, slot_y, slot_w, slot_h, slot_border);

            int text_w = MeasureGameText(label.c_str(), 14, true);
            DrawGameText(label.c_str(), slot_x + slot_w / 2 - text_w / 2,
                         slot_y + 15, 14, slot_text_color, true);

            if (slot_hover && (menu_transition_cooldown_ <= 0.0f) &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                std::string file = std::string(GetApplicationDirectory()) +
                                   "save_slot_" + std::to_string(i) + ".txt";
                if (is_saving_mode_) {
                    bool ok = save(engine::session(mode_), file);
                    if (ok) {
                        status_msg_ = "Game saved to Slot " +
                                      std::to_string(i) + " successfully!";
                    } else {
                        status_msg_ = "Failed to save game!";
                    }
                    status_msg_timer_ = 2.5f;
                    is_slot_menu_ = false;
                } else {
                    if (meta.exists) {
                        bool ok = load(engine::session(mode_), file);
                        if (ok) {
                            has_selection_ = false;
                            is_dragging_ = false;
                            selected_equip_index_ = -1;
                            is_dragging_equip_ = false;
                            drag_equip_source_index_ = -1;
                            ApplyModeUpdate(engine::on_session_loaded(mode_));
                            status_msg_ = "Game loaded from Slot " +
                                          std::to_string(i) + " successfully!";
                            status_msg_timer_ = 2.5f;
                        } else {
                            status_msg_ = "Failed to load game!";
                        }
                    } else {
                        status_msg_ = "Slot is empty!";
                    }
                    status_msg_timer_ = 2.5f;
                    is_slot_menu_ = false;
                }
            }

            // Draw delete button if slot exists
            if (meta.exists) {
                int del_x = slot_x + slot_w + 10;
                int del_w = 60;
                bool del_hover = CheckCollisionPointRec(
                    GetMousePosition(),
                    {(float)del_x, (float)slot_y, (float)del_w, (float)slot_h});
                Color del_base = del_hover ? Color{180, 50, 50, 255}
                                           : Color{120, 30, 30, 255};
                Color del_border = del_hover ? RED : Color{160, 50, 50, 255};

                DrawRectangle(del_x, slot_y, del_w, slot_h, del_base);
                DrawRectangleLines(del_x, slot_y, del_w, slot_h, del_border);

                int del_text_w = MeasureGameText("DEL", 14, true);
                DrawGameText("DEL", del_x + del_w / 2 - del_text_w / 2,
                             slot_y + 15, 14, WHITE, true);

                if (del_hover && (menu_transition_cooldown_ <= 0.0f) &&
                    IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    std::string file = std::string(GetApplicationDirectory()) +
                                       "save_slot_" + std::to_string(i) +
                                       ".txt";
                    std::remove(file.c_str());
                    status_msg_ =
                        "Slot " + std::to_string(i) + " deleted successfully.";
                    status_msg_timer_ = 2.0f;
                }
            }
        }

        if (draw_menu_btn("Back", 440, true)) {
            is_slot_menu_ = false;
        }
    } else {
        // Button 1: Single Player or Continue Game
        const char *btn1_text =
            game_in_progress_ ? "Continue Game" : "Single Player";
        if (draw_menu_btn(btn1_text, 220, true)) {
            if (game_in_progress_) {
                state_ = GameState::Gameplay;
            } else {
                ApplyModeUpdate(engine::start_single_player(mode_));
            }
        }

        // Button 2: Multiplayer or Exit Game
        const char *btn2_text = game_in_progress_ ? "Exit Game" : "Multiplayer";
        bool btn2_enabled = true;
        if (draw_menu_btn(btn2_text, 280, btn2_enabled)) {
            if (game_in_progress_) {
                ApplyModeUpdate(engine::leave_game(mode_));
                game_in_progress_ = false;
            } else {
                multiplayer_menu_state_ = MultiplayerMenuState::Setup;
                multiplayer_status_.clear();
                multiplayer_address_input_.SetFocused(false);
                state_ = GameState::MultiplayerMenu;
                menu_transition_cooldown_ = 0.2f;
            }
        }

        // Button 3: Save Game (enabled only when in progress)
        if (draw_menu_btn("Save Game", 340,
                          game_in_progress_ && engine::can_save(mode_))) {
            is_slot_menu_ = true;
            is_saving_mode_ = true;
        }

        // Button 4: Load Game (always enabled)
        if (draw_menu_btn("Load Game", 400, true)) {
            is_slot_menu_ = true;
            is_saving_mode_ = false;
        }

        // Button 5: Quit
        if (draw_menu_btn("Quit", 460, true)) {
            exit_flag_ = true;
        }
    }
}

void GameApp::UpdateMultiplayerMenu() {
    Rectangle address_bounds = MultiplayerAddressBounds();

    auto cancel_connection = [&]() {
        ApplyModeUpdate(engine::leave_game(mode_));
        multiplayer_menu_state_ = MultiplayerMenuState::Setup;
        multiplayer_status_.clear();
        multiplayer_address_input_.SetFocused(false);
        state_ = GameState::MultiplayerMenu;
        menu_transition_cooldown_ = 0.2f;
    };

    auto leave_multiplayer_menu = [&]() {
        ApplyModeUpdate(engine::leave_game(mode_));
        multiplayer_menu_state_ = MultiplayerMenuState::Setup;
        multiplayer_status_.clear();
        multiplayer_address_input_.SetFocused(false);
        state_ = GameState::MainMenu;
        menu_transition_cooldown_ = 0.2f;
    };

    if (multiplayer_menu_state_ != MultiplayerMenuState::Setup) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            cancel_connection();
        }
        return;
    }

    multiplayer_address_input_.Update();

    if (IsKeyPressed(KEY_ESCAPE)) {
        leave_multiplayer_menu();
    }
}

void GameApp::DrawMultiplayerMenu() {
    DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.55f));

    const char *title = "MULTIPLAYER";
    int title_w = MeasureGameText(title, 48, true);
    DrawGameText(title, 640 - title_w / 2, 86, 48, GOLD, true);

    auto draw_btn = [&](const char *text, int x, int y, int w,
                        bool enabled) -> bool {
        int h = 44;
        bool hover = enabled && CheckCollisionPointRec(
                                    GetMousePosition(),
                                    {(float)x, (float)y, (float)w, (float)h});
        DrawRectangle(
            x, y, w, h,
            enabled ? (hover ? Color{50, 60, 90, 255} : Color{30, 30, 38, 255})
                    : Color{35, 35, 40, 255});
        DrawRectangleLines(x, y, w, h,
                           enabled ? (hover ? GOLD : GRAY) : DARKGRAY);
        int text_w = MeasureGameText(text, 18, true);
        DrawGameText(text, x + w / 2 - text_w / 2, y + 12, 18,
                     enabled ? WHITE : GRAY, true);
        return hover && menu_transition_cooldown_ <= 0.0f &&
               IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    };

    auto leave_multiplayer_menu = [&]() {
        ApplyModeUpdate(engine::leave_game(mode_));
        multiplayer_menu_state_ = MultiplayerMenuState::Setup;
        multiplayer_status_.clear();
        multiplayer_address_input_.SetFocused(false);
        state_ = GameState::MainMenu;
        menu_transition_cooldown_ = 0.2f;
    };

    auto cancel_connection = [&]() {
        ApplyModeUpdate(engine::leave_game(mode_));
        multiplayer_menu_state_ = MultiplayerMenuState::Setup;
        multiplayer_status_.clear();
        multiplayer_address_input_.SetFocused(false);
        state_ = GameState::MultiplayerMenu;
        menu_transition_cooldown_ = 0.2f;
    };

    bool connection_screen =
        multiplayer_menu_state_ != MultiplayerMenuState::Setup;

    // Draw address field using InputBox
    Rectangle addr_bounds = MultiplayerAddressBounds();
    DrawGameText("Address", (int)addr_bounds.x, (int)addr_bounds.y - 30, 18,
                 LIGHTGRAY, false);
    multiplayer_address_input_.SetReadOnly(connection_screen);
    multiplayer_address_input_.Draw();

    auto draw_connection_status = [&]() {
        std::string text =
            !multiplayer_status_.empty()
                ? multiplayer_status_
                : (multiplayer_menu_state_ == MultiplayerMenuState::Hosting
                       ? "Waiting... (1/2)"
                       : "Joining...");
        constexpr int font_size = 24;
        constexpr int max_width = 640;
        while (!text.empty() &&
               MeasureGameText(text.c_str(), font_size, true) > max_width) {
            text.pop_back();
        }

        Color color =
            text.starts_with("Failed") || text.starts_with("Network error")
                ? Color{255, 120, 120, 255}
                : LIGHTGRAY;
        int text_w = MeasureGameText(text.c_str(), font_size, true);
        DrawGameText(text.c_str(), 640 - text_w / 2, 300, font_size, color,
                     true);
    };

    if (connection_screen) {
        draw_connection_status();

        if (draw_btn("BACK", 570, 374, 140, true)) {
            cancel_connection();
        }
        return;
    }

    if (draw_btn("HOST", 430, 315, 195, true)) {
        engine::ModeUpdate update = engine::host_multiplayer(
            mode_,
            ParseMultiplayerAddress(multiplayer_address_input_.GetValue()));
        ApplyModeUpdate(update);
        multiplayer_menu_state_ = MultiplayerMenuState::Hosting;
        multiplayer_status_ = update.status.starts_with("Failed")
                                  ? update.status
                                  : "Waiting... (1/2)";
        multiplayer_address_input_.SetFocused(false);
    }
    if (draw_btn("JOIN", 655, 315, 195, true)) {
        engine::ModeUpdate update = engine::join_multiplayer(
            mode_,
            ParseMultiplayerAddress(multiplayer_address_input_.GetValue()));
        ApplyModeUpdate(update);
        multiplayer_menu_state_ = MultiplayerMenuState::Joining;
        multiplayer_status_ =
            update.status.empty() ? "Joining..." : update.status;
        multiplayer_address_input_.SetFocused(false);
    }
    if (draw_btn("BACK", 500, 380, 280, true)) {
        leave_multiplayer_menu();
    }
}

void GameApp::UpdateSettlement() {
    if (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        state_ = GameState::MainMenu;
        game_in_progress_ = false;
        ApplyModeUpdate(engine::leave_game(mode_));
        menu_transition_cooldown_ = 0.25f;
    }
}

void GameApp::DrawSettlement() {
    // 1. Dim the background/viewpoint
    DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.65f));

    // 2. Draw card/box background
    int box_w = 560;
    int box_h = 320;
    int box_x = 640 - box_w / 2;
    int box_y = 360 - box_h / 2;

    // Sleek glassmorphism look
    DrawRectangle(box_x, box_y, box_w, box_h, Color{20, 20, 25, 230});
    Color border_color = player_won_game_ ? GOLD : RED;
    DrawRectangleLinesEx(
        {(float)box_x, (float)box_y, (float)box_w, (float)box_h}, 2.0f,
        border_color);

    // Glowing outline
    DrawRectangleLinesEx({(float)box_x - 1, (float)box_y - 1, (float)box_w + 2,
                          (float)box_h + 2},
                         1.0f, Fade(border_color, 0.5f));

    // Title
    const char *title = player_won_game_ ? "VICTORY!" : "DEFEAT!";
    int titleFontSize = 48;
    int titleWidth = MeasureGameText(title, titleFontSize, true);
    int titleX = 640 - titleWidth / 2;
    int titleY = box_y + 40;

    // Text shadow
    DrawGameText(title, titleX + 2, titleY + 2, titleFontSize, BLACK, true);
    DrawGameText(title, titleX, titleY, titleFontSize, border_color, true);

    // Description text
    std::string desc1;
    std::string desc2;
    bool multiplayer =
        engine::mode_kind(mode_) != engine::ModeKind::SinglePlayer;
    if (player_won_game_) {
        if (multiplayer) {
            desc1 = "Victory! Your opponent has been defeated.";
            desc2 = "Return to the menu to start another duel.";
        } else {
            desc1 = "Congratulations! You have successfully survived";
            desc2 = "all 20 rounds of Slime Tactics!";
        }
    } else {
        desc1 = "Your forces fell. You reached Round " +
                std::to_string(engine::session(mode_).round_) + ".";
        desc2 = "Better luck next time!";
    }

    int descFontSize = 18;
    int desc1W = MeasureGameText(desc1.c_str(), descFontSize, false);
    int desc2W = MeasureGameText(desc2.c_str(), descFontSize, false);

    DrawGameText(desc1.c_str(), 640 - desc1W / 2, box_y + 130, descFontSize,
                 LIGHTGRAY, false);
    DrawGameText(desc2.c_str(), 640 - desc2W / 2, box_y + 165, descFontSize,
                 LIGHTGRAY, false);

    // Click/key Prompt
    float pulse = sin(GetTime() * 3.5f) * 0.5f + 0.5f;
    Color promptColor = Fade(GRAY, 0.4f + pulse * 0.6f);
    const char *prompt = "PRESS ANY KEY OR CLICK TO RETURN TO MAIN MENU";
    int promptFontSize = 14;
    int promptW = MeasureGameText(prompt, promptFontSize, true);
    DrawGameText(prompt, 640 - promptW / 2, box_y + 245, promptFontSize,
                 promptColor, true);
}

void GameApp::HandleInputs() {
    bool over_ui = IsMouseOverUI();

    // 1. Hover updates
    hovered_equip_index_ = -1;
    has_hover_ = false;

    if (!over_ui) {
        Ray ray = GetScreenToWorldRay(GetMousePosition(), camera_);
        if (ray.direction.y < 0) {
            float t = -ray.position.y / ray.direction.y;
            Vector3 hit =
                Vector3Add(ray.position, Vector3Scale(ray.direction, t));
            for (int i = 0; i < 8; ++i) {
                Vector3 pos = GetEquipWorldPos(i);
                float dist = Vector2Distance({hit.x, hit.z}, {pos.x, pos.z});
                if (dist < 0.5f) {
                    hovered_equip_index_ = i;
                    break;
                }
            }
        }

        auto [cell, found] = GetCellUnderMouse();
        if (found) {
            hovered_coord_ = cell;
            has_hover_ = true;
        }
    }

    // 2. Click logic
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (!over_ui) {
            if (hovered_equip_index_ != -1 &&
                hovered_equip_index_ <
                    (int)engine::session(mode_).equip_pool_.size()) {
                is_dragging_equip_ = true;
                drag_equip_source_index_ = hovered_equip_index_;
                status_msg_ = "Dragging " +
                              GetElementName(static_cast<unit::Element>(
                                  drag_equip_source_index_ % 6)) +
                              " Drop.";
                status_msg_timer_ = 1.5f;
                has_selection_ = false;
                return;
            }

            if (has_hover_) {
                selected_coord_ = hovered_coord_;
                has_selection_ = true;

                if (get_unit(engine::session(mode_).board_, hovered_coord_)) {
                    is_dragging_ = true;
                    drag_source_ = hovered_coord_;
                }
            } else {
                has_selection_ = false;
            }
        }
    }

    // 3. Drag release logic (allow drag-releasing on Sell zone even if over UI)
    if (is_dragging_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        is_dragging_ = false;
        Vector2 mouse_pos = GetMousePosition();
        if (CheckCollisionPointRec(mouse_pos, ShopPanelBounds())) {
            if (get_unit(engine::session(mode_).board_, drag_source_)) {
                ApplyModeUpdate(engine::act_sell(mode_, drag_source_));
                if (has_selection_ && selected_coord_ == drag_source_) {
                    has_selection_ = false;
                }
            }
            return;
        }

        if (!over_ui) {
            auto [drop_cell, drop_found] = GetCellUnderMouse();
            if (drop_found && !(drag_source_ == drop_cell)) {
                // Visually snap the dragged slime toward the drop cell; if the
                // move is rejected, the per-frame board sync pulls it back.
                auto src_u =
                    get_unit(engine::session(mode_).board_, drag_source_);
                if (src_u && slimes_.find(src_u) != slimes_.end()) {
                    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera_);
                    if (ray.direction.y < 0) {
                        float t = -ray.position.y / ray.direction.y;
                        Vector3 hit = Vector3Add(
                            ray.position, Vector3Scale(ray.direction, t));
                        slimes_[src_u].current_pos = hit;
                        slimes_[src_u].was_dragged = true;
                    }
                }
                ApplyModeUpdate(
                    engine::act_move(mode_, drag_source_, drop_cell));
            }
        }
    }

    if (is_dragging_equip_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        is_dragging_equip_ = false;
        if (!over_ui && has_hover_) {
            auto u_ptr =
                get_unit(engine::session(mode_).board_, hovered_coord_);
            if (u_ptr) {
                ApplyModeUpdate(engine::act_equip(
                    mode_, hovered_coord_,
                    static_cast<std::size_t>(drag_equip_source_index_)));
            } else {
                status_msg_ =
                    "Preparation Phase - Drag and drop units to position them.";
                status_msg_timer_ = 0.0f;
            }
        }
        drag_equip_source_index_ = -1;
    }
}

void GameApp::StartCombatPhase() {
    ApplyModeUpdate(engine::start_combat(mode_));
    combat_tick_timer = 0.0f;
}

void GameApp::ProcessCombatTick() {
    ApplyModeUpdate(engine::process_combat_tick(mode_));
}

void GameApp::Draw() {
    BeginDrawing();
    ClearBackground(Color{24, 24, 28, 255}); // modern dark theme background

    // 1. Draw 3D elements
    BeginMode3D(camera_);
    DrawGame3D();
    EndMode3D();

    // 2. Draw 2D UI overlay based on state
    if (state_ == GameState::StartMenu) {
        DrawStartMenu();
    } else if (state_ == GameState::MainMenu) {
        DrawMainMenu();
    } else if (state_ == GameState::MultiplayerMenu) {
        DrawMultiplayerMenu();
    } else if (state_ == GameState::Settlement) {
        DrawSettlement();
    } else {
        DrawGame2D();
    }

    EndDrawing();
}

void GameApp::DrawGame3D() {
    // Draw Arena Background Scene
    if (arena_model_.meshCount > 0) {
        rlDisableBackfaceCulling();
        DrawModelEx(arena_model_, {0.3f, -3.2f, -5.0f}, {0.0f, 1.0f, 0.0f},
                    0.0f, {0.6f, 0.6f, 0.6f}, WHITE);
        rlEnableBackfaceCulling();
    }

    engine::Board &active_board = engine::active_board(mode_);

    // 1. Draw Board hexagonal cells
    int r_start = engine::is_combat(mode_) ? 0 : 4;
    for (int r = r_start; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            engine::HexCoord coord{r, c};
            Vector3 pos = GetHexWorldPos(coord);

            // Set element coloring
            Color cell_color =
                (r >= 4) ? Color{50, 110, 80, 255}
                         : Color{110, 50, 50, 255}; // Player area vs Enemy area

            // Highlight if hovered
            bool highlight = false;
            if (has_hover_ &&
                std::holds_alternative<engine::HexCoord>(hovered_coord_)) {
                auto hex = std::get<engine::HexCoord>(hovered_coord_);
                if (hex.r == r && hex.c == c)
                    highlight = true;
            }

            DrawHexCell(pos, cell_color, highlight);
        }
    }

    // 2. Draw Bench Slots (hidden during combat)
    if (!engine::is_combat(mode_)) {
        for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
            Vector3 pos = GetBenchWorldPos(i);
            bool highlight = false;
            if (has_hover_ &&
                std::holds_alternative<engine::LinearCoord>(hovered_coord_)) {
                auto linear = std::get<engine::LinearCoord>(hovered_coord_);
                if (linear.x == i)
                    highlight = true;
            }
            DrawHexCell(pos, Color{60, 60, 75, 255}, highlight);
        }
    }

    // 3. Draw Equipment Slots (hidden during combat)
    if (!engine::is_combat(mode_)) {
        for (int i = 0; i < 8; ++i) {
            Vector3 pos = GetEquipWorldPos(i);
            bool eq_highlight = (hovered_equip_index_ == i);
            DrawHexCell(pos, Color{45, 45, 55, 255}, eq_highlight);

            // Draw equipment sphere floating in slot if present
            if (i < (int)engine::session(mode_).equip_pool_.size()) {
                // Float sphere
                Vector3 sphere_pos = pos;
                if (is_dragging_equip_ && drag_equip_source_index_ == i) {
                    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera_);
                    if (ray.direction.y < 0) {
                        float t = -ray.position.y / ray.direction.y;
                        sphere_pos = Vector3Add(ray.position,
                                                Vector3Scale(ray.direction, t));
                        sphere_pos.y = 0.35f;
                    } else {
                        sphere_pos.y += 0.35f;
                    }
                } else {
                    sphere_pos.y += 0.35f + sin(GetTime() * 3.0f + i) * 0.08f;
                }

                unit::Equipment eq = engine::session(mode_).equip_pool_[i];
                unit::Element elem = unit::Element::Pyro;
                if (std::holds_alternative<unit::PyroDrop>(eq))
                    elem = unit::Element::Pyro;
                else if (std::holds_alternative<unit::HydroDrop>(eq))
                    elem = unit::Element::Hydro;
                else if (std::holds_alternative<unit::AnemoDrop>(eq))
                    elem = unit::Element::Anemo;
                else if (std::holds_alternative<unit::GeoDrop>(eq))
                    elem = unit::Element::Geo;
                else if (std::holds_alternative<unit::ElectroDrop>(eq))
                    elem = unit::Element::Electro;
                else if (std::holds_alternative<unit::CryoDrop>(eq))
                    elem = unit::Element::Cryo;

                Color item_color = GetElementColor(elem);
                // Inner solid core
                DrawSphere(sphere_pos, 0.09f, item_color);
                // Outer translucent shell
                DrawSphere(sphere_pos, 0.13f, Fade(item_color, 0.4f));
            }
        }
    }

    // 4. Draw Slimes with squash/stretch/bounce animations
    for (auto &pair : slimes_) {
        auto u_ptr = pair.first;
        VisualSlime &v = pair.second;

        if (v.is_dead && v.death_time >= 1.0f)
            continue;

        // Hide enemy units in Prep phase
        if (!engine::is_combat(mode_)) {
            if (std::holds_alternative<HexCoord>(v.last_coord)) {
                auto hex = std::get<HexCoord>(v.last_coord);
                if (hex.r < 4)
                    continue;
            }
        } else {
            // Hide bench units in combat phase
            if (std::holds_alternative<LinearCoord>(v.last_coord)) {
                continue;
            }
        }

        // Base visual scale based on unit star level
        auto stats = unit::stats(*u_ptr);
        float base_scale = 0.15f;
        if (stats.level == 2)
            base_scale = 0.21f;
        else if (stats.level == 3)
            base_scale = 0.28f;
        else if (stats.level == 4)
            base_scale = 0.36f;

        // Position interpolation
        Vector3 render_pos = v.current_pos;
        if (is_dragging_ && drag_source_ == v.last_coord) {
            Ray ray = GetScreenToWorldRay(GetMousePosition(), camera_);
            if (ray.direction.y < 0) {
                float t = -ray.position.y / ray.direction.y;
                render_pos =
                    Vector3Add(ray.position, Vector3Scale(ray.direction, t));
            }
        } else if (v.move_time < 1.0f) {
            render_pos = Vector3Lerp(v.current_pos, v.target_pos, v.move_time);
            // vertical jump sine-wave
            render_pos.y += sin(v.move_time * PI) * 0.35f;
        } else {
            render_pos = v.target_pos;
        }

        // Apply high-frequency hurt wobble (proportional to base scale)
        if (v.hurt_time < 1.0f) {
            float wobble = sin(v.hurt_time * PI * 3.0f) * base_scale * 0.2f;
            render_pos.x += wobble;
        }

        // Apply lunge attack offset
        if (v.attack_time < 1.0f) {
            float attack_offset = sin(v.attack_time * PI) * 0.2f;
            render_pos = Vector3Add(render_pos,
                                    Vector3Scale(v.attack_dir, attack_offset));
        }

        // Apply scale factors (MC style squash and stretch)
        Vector3 visual_scale = {base_scale, base_scale, base_scale};

        // Idle bounce scale (proportional to base scale)
        float bounce = sin(v.bounce_phase) * base_scale * 0.08f;
        visual_scale.y += bounce;
        visual_scale.x -= bounce * 0.5f;
        visual_scale.z -= bounce * 0.5f;

        // Jump stretch or landing squash (proportional to base scale)
        if (v.move_time < 1.0f) {
            float jump_factor = sin(v.move_time * PI);
            visual_scale.y += jump_factor * base_scale * 0.25f;
            visual_scale.x -= jump_factor * base_scale * 0.125f;
            visual_scale.z -= jump_factor * base_scale * 0.125f;
        } else if (v.move_time < 1.3f) {
            // Landing squash phase: t_land goes from 0.0 to 1.0
            float t_land = (v.move_time - 1.0f) / 0.3f;
            float squash = sin(t_land * PI) * base_scale * 0.2f;
            visual_scale.y -= squash;
            visual_scale.x += squash * 0.5f;
            visual_scale.z += squash * 0.5f;
        }

        // Cast inflation
        if (v.cast_time < 1.0f) {
            float cast_factor = sin(v.cast_time * PI);
            visual_scale =
                Vector3Scale(visual_scale, 1.0f + cast_factor * 0.15f);
        }

        // Death shrink
        if (v.is_dead) {
            float death_factor = std::min(1.0f, v.death_time);
            visual_scale = Vector3Scale(visual_scale, 1.0f - death_factor);
            render_pos.y -= death_factor * 0.2f;
        }

        // Stunned status check
        bool is_stunned = (stats.stun_ticks > 0);

        // Apply render height offset so it sits on top of hex cell
        render_pos.y += (0.075f + visual_scale.y);

        // Draw slime model
        float hurt_val = (v.hurt_time < 1.0f) ? (1.0f - v.hurt_time) : 0.0f;
        float cast_val = (v.cast_time < 1.0f) ? (1.0f - v.cast_time) : 0.0f;

        DrawSlime(render_pos, visual_scale.y, unit::element(*u_ptr),
                  stats.owner, hurt_val, cast_val, is_stunned);

        // Draw small floating sphere for equipped item
        if (!v.is_dead &&
            !std::holds_alternative<std::monostate>(stats.equipped)) {
            Vector3 item_pos = render_pos;
            item_pos.y += visual_scale.y + sin(GetTime() * 4.0f) * 0.06f;

            unit::Element elem = unit::Element::Pyro;
            if (std::holds_alternative<unit::PyroDrop>(stats.equipped))
                elem = unit::Element::Pyro;
            else if (std::holds_alternative<unit::HydroDrop>(stats.equipped))
                elem = unit::Element::Hydro;
            else if (std::holds_alternative<unit::AnemoDrop>(stats.equipped))
                elem = unit::Element::Anemo;
            else if (std::holds_alternative<unit::GeoDrop>(stats.equipped))
                elem = unit::Element::Geo;
            else if (std::holds_alternative<unit::ElectroDrop>(stats.equipped))
                elem = unit::Element::Electro;
            else if (std::holds_alternative<unit::CryoDrop>(stats.equipped))
                elem = unit::Element::Cryo;

            Color item_color = GetElementColor(elem);
            // Inner solid core
            DrawSphere(item_pos, 0.05f, item_color);
            // Outer translucent shell
            DrawSphere(item_pos, 0.08f, Fade(item_color, 0.4f));
            // Outer wire outline
            DrawSphereWires(item_pos, 0.081f, 8, 8, Fade(item_color, 0.8f));
        }
    }

    // 5. Draw active Projectiles and VFX rings
    for (const auto &proj : projectiles_) {
        if (!proj.is_area) {
            DrawSphere(proj.current_pos, proj.radius, proj.color);
            DrawSphereWires(proj.current_pos, proj.radius + 0.02f, 8, 8,
                            Fade(proj.color, 0.4f));
        } else {
            // Draw skill blast expansion ring
            DrawCylinderWires(
                proj.current_pos, proj.radius, proj.radius, 0.05f, 16,
                Fade(proj.color,
                     0.8f * (1.0f - proj.radius / proj.max_radius)));
        }
    }
}

void GameApp::DrawHexCell(const Vector3 &pos, Color color, bool highlight) {
    Vector3 rotationAxis = {0.0f, 1.0f, 0.0f};
    float rotationAngle = 0.0f;

    Color render_color = color;
    if (highlight) {
        // Blend the cell color with a semi-transparent GOLD overlay for a
        // premium glowing highlight
        render_color = ColorAlphaBlend(color, Fade(GOLD, 0.2f), WHITE);
    }

    DrawModelEx(hex_model_, pos, rotationAxis, rotationAngle,
                {1.0f, 1.0f, 1.0f}, render_color);
}

void GameApp::DrawSlime(const Vector3 &pos, float scale, unit::Element elem,
                        unit::Owner owner, float hurt_val, float cast_val,
                        bool is_stunned) {
    Color base_color = GetElementColor(elem);

    // Minecraft-style composite slime
    // Solid Inner core (cubical)
    float inner_size = scale * 1.5f;
    Color render_color = base_color;

    // Flash red when hurt, or cyan when casting
    if (hurt_val > 0.0f) {
        render_color =
            ColorAlphaBlend(render_color, RED, Fade(RED, hurt_val * 0.7f));
    } else if (cast_val > 0.0f) {
        render_color =
            ColorAlphaBlend(render_color, WHITE, Fade(WHITE, cast_val * 0.6f));
    }

    DrawCube(pos, inner_size, inner_size, inner_size, render_color);

    // Translucent Outer body (slightly larger)
    float outer_size = scale * 2.0f;
    Color outer_color = Fade(base_color, 0.4f);
    DrawCube(pos, outer_size, outer_size, outer_size, outer_color);
    DrawCubeWires(pos, outer_size, outer_size, outer_size,
                  Fade(base_color, 0.8f));

    // Eyes: 2 small black cubes facing forward (negative Z for Player, positive
    // Z for Enemy)
    float eye_size = scale * 0.3f;
    float eye_height = pos.y + scale * 0.2f;
    float eye_offset_x = scale * 0.4f;
    float eye_offset_z = scale * 0.99f;

    if (owner == unit::Owner::PlayerCtrl) {
        Vector3 left_eye = {pos.x - eye_offset_x, eye_height,
                            pos.z - eye_offset_z};
        Vector3 right_eye = {pos.x + eye_offset_x, eye_height,
                             pos.z - eye_offset_z};
        DrawCube(left_eye, eye_size, eye_size, eye_size, BLACK);
        DrawCube(right_eye, eye_size, eye_size, eye_size, BLACK);
    } else {
        Vector3 left_eye = {pos.x + eye_offset_x, eye_height,
                            pos.z + eye_offset_z};
        Vector3 right_eye = {pos.x - eye_offset_x, eye_height,
                             pos.z + eye_offset_z};
        DrawCube(left_eye, eye_size, eye_size, eye_size, BLACK);
        DrawCube(right_eye, eye_size, eye_size, eye_size, BLACK);
    }

    // Mouth: small black cube below eyes
    float mouth_size_x = scale * 0.4f;
    float mouth_size_y = scale * 0.15f;
    float mouth_size_z = scale * 0.2f;
    float mouth_height = pos.y - scale * 0.2f;

    if (owner == unit::Owner::PlayerCtrl) {
        Vector3 mouth_pos = {pos.x, mouth_height, pos.z - eye_offset_z};
        DrawCube(mouth_pos, mouth_size_x, mouth_size_y, mouth_size_z, BLACK);
    } else {
        Vector3 mouth_pos = {pos.x, mouth_height, pos.z + eye_offset_z};
        DrawCube(mouth_pos, mouth_size_x, mouth_size_y, mouth_size_z, BLACK);
    }

    // Stunned indicator (floating small stars/rings)
    if (is_stunned) {
        Vector3 stun_pos = pos;
        stun_pos.y += scale + 0.15f;
        DrawCylinderWires(stun_pos, scale * 0.7f, scale * 0.7f, 0.02f, 8,
                          YELLOW);
    }
}

static void DrawStar2D(float cx, float cy, float outerRadius, float innerRadius,
                       Color color) {
    Vector2 points[10];
    for (int i = 0; i < 10; ++i) {
        float angle = -PI / 2.0f + i * (PI / 5.0f);
        float r = (i % 2 == 0) ? outerRadius : innerRadius;
        points[i] = Vector2{cx + r * cosf(angle), cy + r * sinf(angle)};
    }
    Vector2 center = {cx, cy};
    for (int i = 0; i < 10; ++i) {
        DrawTriangle(center, points[(i + 1) % 10], points[i], color);
    }
}

void GameApp::DrawGame2D() {
    bool can_prepare = engine::can_prepare(mode_);
    bool is_multiplayer =
        engine::mode_kind(mode_) != engine::ModeKind::SinglePlayer;
    bool local_ready = is_multiplayer && mode_.multiplayer_.local_ready_;

    // 1. Draw HUD top stats bar
    DrawRectangle(0, 0, 1280, 60, Color{16, 16, 20, 240});
    DrawRectangleLines(0, 0, 1280, 60, Color{40, 40, 50, 255});

    // Player HP
    DrawGameText("PLAYER HP:", 30, 20, 22, LIGHTGRAY);
    DrawRectangle(150, 20, 200, 20, DARKGRAY);
    float hp_pct = engine::session(mode_).player_.hp / 100.0f;
    DrawRectangle(150, 20, (int)(200 * hp_pct), 20,
                  hp_pct > 0.4f ? GREEN : RED);
    std::string hp_text =
        std::to_string(engine::session(mode_).player_.hp) + "/100";
    DrawGameText(hp_text.c_str(), 220, 22, 18, WHITE);

    // Player Gold
    DrawGameText("GOLD:", 400, 20, 22, LIGHTGRAY);
    std::string gold_text =
        std::to_string(engine::session(mode_).player_.gold) + " G";
    DrawGameText(gold_text.c_str(), 470, 20, 22, GOLD);

    // Player Level
    DrawGameText("LEVEL:", 580, 20, 22, LIGHTGRAY);
    std::string level_text =
        std::to_string(engine::session(mode_).player_.level);
    DrawGameText(level_text.c_str(), 660, 20, 22, SKYBLUE);

    // Board population count (only count PlayerCtrl units)
    int board_units = 0;
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            if (auto u = get_unit(engine::is_combat(mode_)
                                      ? engine::active_board(mode_)
                                      : engine::session(mode_).board_,
                                  engine::HexCoord{r, c})) {
                if (unit::stats(*u).owner == unit::Owner::PlayerCtrl) {
                    board_units++;
                }
            }
        }
    }
    std::string pop_text =
        "(" + std::to_string(board_units) + "/" +
        std::to_string(engine::session(mode_).player_.level) + ")";
    DrawGameText(pop_text.c_str(), 690, 22, 18, LIGHTGRAY);

    // Round number
    DrawGameText("ROUND:", 800, 20, 22, LIGHTGRAY);
    std::string round_text = std::to_string(engine::session(mode_).round_);
    DrawGameText(round_text.c_str(), 890, 20, 22, WHITE);

    // Phase Indicator / Start Combat Button
    if (engine::is_combat(mode_)) {
        DrawRectangle(980, 15, 200, 30, RED);
        DrawRectangleLines(980, 15, 200, 30, Color{100, 30, 30, 255});
        int text_w = MeasureGameText("COMBAT", 16, true);
        DrawGameText("COMBAT", 980 + 200 / 2 - text_w / 2, 22, 16, WHITE, true);
    } else {
        std::string button_text = "START COMBAT";
        if (is_multiplayer) {
            int ready_count = 0;
            if (mode_.multiplayer_.local_ready_)
                ready_count++;
            if (mode_.multiplayer_.remote_ready_)
                ready_count++;
            // When already readied the button becomes a CANCEL toggle so the
            // player can keep editing while waiting for the opponent.
            button_text = (local_ready ? "CANCEL (" : "READY (") +
                          std::to_string(ready_count) + "/2)";
        }

        bool hover =
            CheckCollisionPointRec(GetMousePosition(), {980, 15, 200, 30});
        Color btn_color;
        if (local_ready) {
            // Amber "cancel" state, distinct from the green ready/start action.
            btn_color =
                hover ? Color{200, 140, 40, 255} : Color{160, 110, 30, 255};
        } else {
            btn_color =
                hover ? Color{40, 140, 55, 255} : Color{30, 110, 45, 255};
        }

        DrawRectangle(980, 15, 200, 30, btn_color);
        DrawRectangleLines(980, 15, 200, 30, Color{60, 60, 70, 255});
        int text_w = MeasureGameText(button_text.c_str(), 16, true);
        DrawGameText(button_text.c_str(), 980 + 200 / 2 - text_w / 2, 22, 16,
                     WHITE, true);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (local_ready) {
                ApplyModeUpdate(engine::cancel_ready(mode_));
            } else {
                StartCombatPhase();
            }
        }
    }

    // 2. Draw Status Message banner
    if (status_msg_timer_ > 0.0f) {
        int width = MeasureGameText(status_msg_.c_str(), 22);
        DrawRectangle(640 - width / 2 - 20, 80, width + 40, 40,
                      Color{20, 20, 25, 220});
        DrawRectangleLines(640 - width / 2 - 20, 80, width + 40, 40, GOLD);
        DrawGameText(status_msg_.c_str(), 640 - width / 2, 90, 20, GOLD);
    }

    // Hover tooltip state (filled below, drawn last so it sits on top)
    bool tip_show = false;
    std::string tip_title;
    std::vector<std::string> tip_lines;
    Color tip_accent = WHITE;
    Vector2 mouse = GetMousePosition();

    // 3. Draw Active Synergies as circular elemental icons with ring progress
    auto active_synergies = unit::compute_synergies(
        engine::is_combat(mode_) ? engine::active_board(mode_)
                                 : engine::session(mode_).board_);

    // Vertical column of circular icons on the left edge
    float sy_y = 150.0f;
    auto draw_synergy_item = [&](Texture2D icon, const char *name,
                                 const char *desc, int count, int threshold,
                                 bool active, Color col) {
        Vector2 center{44.0f, sy_y + 22.0f};
        const float outer = 20.0f;
        const float inner = 17.0f;

        float progress = threshold > 0
                             ? std::min(1.0f, (float)count / (float)threshold)
                             : 0.0f;

        // Soft outer glow when active
        if (active)
            DrawCircleGradient((int)center.x, (int)center.y, outer + 6.0f,
                               Fade(col, 0.35f), Fade(col, 0.0f));

        // Inner disc background
        DrawCircle((int)center.x, (int)center.y, outer,
                   active ? Fade(col, 0.30f) : Color{24, 24, 30, 230});

        // Background track ring (full circle)
        DrawRing(center, inner, outer, 0.0f, 360.0f, 64,
                 Color{38, 38, 46, 255});

        // Progress ring, filling clockwise from the top
        if (progress > 0.0f)
            DrawRing(center, inner, outer, -90.0f, -90.0f + 360.0f * progress,
                     64, active ? col : Fade(col, 0.5f));

        // Elemental icon, tinted by state
        if (icon.id != 0) {
            float icon_sz = inner * 1.5f;
            Rectangle src{0, 0, (float)icon.width, (float)icon.height};
            Rectangle dst{center.x - icon_sz / 2.0f, center.y - icon_sz / 2.0f,
                          icon_sz, icon_sz};
            Color tint = active ? WHITE : Color{120, 120, 132, 220};
            DrawTexturePro(icon, src, dst, {0, 0}, 0.0f, tint);
        }

        // Hover -> capture an enlarged tooltip describing this resonance
        if (CheckCollisionPointCircle(mouse, center, outer)) {
            tip_show = true;
            tip_accent = col;
            tip_title = name;
            tip_lines = {
                desc,
                "Units: " + std::to_string(count) + " / " +
                    std::to_string(threshold),
                active ? "Status: ACTIVE"
                       : "Status: inactive (need " + std::to_string(threshold) +
                             ")",
            };
        }

        sy_y += 52.0f;
    };

    draw_synergy_item(
        element_icons_[(int)unit::Element::Pyro], "Fervent Flames",
        "Pyro units gain +25% ATK.", active_synergies.pyro_count, 2,
        active_synergies.fervent_flames, Color{255, 110, 70, 255});
    draw_synergy_item(
        element_icons_[(int)unit::Element::Hydro], "Soothing Water",
        "Hydro units gain +25% Max HP.", active_synergies.hydro_count, 2,
        active_synergies.soothing_water, Color{70, 150, 255, 255});
    draw_synergy_item(
        element_icons_[(int)unit::Element::Anemo], "Impetuous Winds",
        "Anemo units get -15 Max Mana (faster skills).",
        active_synergies.anemo_count, 2, active_synergies.impetuous_winds,
        Color{90, 220, 180, 255});
    draw_synergy_item(element_icons_[(int)unit::Element::Geo], "Enduring Rock",
                      "Geo units gain +15% Shield and +15% DMG while shielded.",
                      active_synergies.geo_count, 2,
                      active_synergies.enduring_rock, Color{240, 190, 70, 255});
    draw_synergy_item(element_icons_[(int)unit::Element::Electro],
                      "High Voltage",
                      "Electro units gain +5 mana on normal attacks.",
                      active_synergies.electro_count, 2,
                      active_synergies.high_voltage, Color{200, 130, 255, 255});
    draw_synergy_item(
        element_icons_[(int)unit::Element::Cryo], "Shattering Ice",
        "Cryo normal attacks have a 20% chance to freeze.",
        active_synergies.cryo_count, 2, active_synergies.shattering_ice,
        Color{160, 230, 255, 255});

    // Four-element bond: count distinct elements present on the board
    int unique_elements =
        (active_synergies.pyro_count > 0) + (active_synergies.hydro_count > 0) +
        (active_synergies.anemo_count > 0) + (active_synergies.geo_count > 0) +
        (active_synergies.electro_count > 0) +
        (active_synergies.cryo_count > 0);
    draw_synergy_item(canopy_icon_, "Protective Canopy",
                      "4+ distinct elements: your team takes -15% damage.",
                      unique_elements, 4, active_synergies.protective_canopy,
                      Color{120, 220, 130, 255});

    // 3b. Hovered equipment -> tooltip with its bonus
    if (!tip_show && hovered_equip_index_ >= 0 &&
        hovered_equip_index_ < (int)engine::session(mode_).equip_pool_.size()) {
        const unit::Equipment &eq =
            engine::session(mode_).equip_pool_[hovered_equip_index_];
        tip_show = true;
        tip_accent = GetElementColor(EquipmentElement(eq));
        tip_title = unit::name(eq);
        tip_lines = {"Equipment", EquipmentEffect(eq),
                     "Drag onto a unit to equip."};
    }

    // 3c. Hovered unit -> tooltip with current (synergy + equipment) stats
    if (!tip_show && has_hover_) {
        auto u_ptr = engine::get_unit(engine::is_combat(mode_)
                                          ? engine::active_board(mode_)
                                          : engine::session(mode_).board_,
                                      hovered_coord_);
        if (u_ptr) {
            unit::Element elem = unit::element(*u_ptr);
            // Stored stats already include equipment bonuses. Synergy stat
            // bonuses are only baked in during combat, so preview them here
            // for the preparation phase to show effective values.
            unit::UnitStats s = unit::stats(*u_ptr);
            auto syn = unit::compute_synergies(
                engine::is_combat(mode_) ? engine::active_board(mode_)
                                         : engine::session(mode_).board_,
                s.owner);
            if (!engine::is_combat(mode_)) {
                if (syn.fervent_flames)
                    s.atk = int(s.atk * 1.25);
                if (syn.soothing_water) {
                    s.max_hp = int(s.max_hp * 1.25);
                    s.hp = int(s.hp * 1.25);
                }
                if (syn.impetuous_winds)
                    s.max_mana = std::max(10, s.max_mana - 15);
                if (syn.enduring_rock && s.shield > 0)
                    s.shield = int(s.shield * 1.15);
            }

            tip_show = true;
            tip_accent = GetElementColor(elem);
            tip_title =
                GetElementName(elem) + " Slime  Lv." + std::to_string(s.level);
            tip_lines.push_back("HP: " + std::to_string(s.hp) + " / " +
                                std::to_string(s.max_hp));
            tip_lines.push_back("ATK: " + std::to_string(s.atk));
            if (s.shield > 0)
                tip_lines.push_back("Shield: " + std::to_string(s.shield));
            tip_lines.push_back("Mana: " + std::to_string(s.mana) + " / " +
                                std::to_string(s.max_mana));
            tip_lines.push_back("Range: " + std::to_string(s.range));
            std::string eq_name = unit::name(s.equipped);
            if (eq_name == "None")
                tip_lines.push_back("Equip: None");
            else
                tip_lines.push_back("Equip: " + eq_name + " (" +
                                    EquipmentEffect(s.equipped) + ")");
        }
    }

    // 5. Draw the recruitment shop in the bottom-right corner. It collapses the
    // moment combat starts. The cards float on their own (no backing panel or
    // title); ShopPanelBounds() is just the sell drop zone.
    if (!engine::is_combat(mode_)) {
        Rectangle shop_panel = ShopPanelBounds();
        bool shop_frozen = engine::session(mode_).shop_frozen_;
        // While frozen the shop is locked for the next round: no buy, no refresh.
        bool shop_buyable = can_prepare && !shop_frozen;
        float row_y = ShopRowY();

        // Collapse empty slots: present cards pack to the right (toward the
        // action block), leaving any gap on the left rather than placeholders.
        std::vector<int> present_slots;
        for (int i = 0; i < kShopCardCount; ++i) {
            if (engine::session(mode_).shop_[i].has_value())
                present_slots.push_back(i);
        }
        int first_pos = kShopCardCount - (int)present_slots.size();

        for (int j = 0; j < (int)present_slots.size(); ++j) {
            int slot = present_slots[j];
            float x = ShopBlockX(first_pos + j);
            float y = row_y;
            Rectangle card{x, y, kShopCardW, kShopCardH};

            DrawRectangleRounded(card, 0.06f, 6, Color{28, 28, 34, 255});
            DrawRectangleRoundedLinesEx(card, 0.06f, 6, 1.0f,
                                        Color{55, 55, 65, 255});

            auto &[unit, cost] = *engine::session(mode_).shop_[slot];
            Color col = GetElementColor(unit::element(unit));
            auto stats = unit::stats(unit);

            // Element color stripe across the top.
            DrawRectangle((int)x + 3, (int)y + 3, (int)kShopCardW - 6, 5, col);

            // Star levels
            for (int s = 0; s < stats.level; ++s) {
                DrawStar2D(x + 14.0f + s * 16.0f, y + 19.0f, 6.5f, 3.0f, YELLOW);
            }

            // Element name
            std::string name = GetElementName(unit::element(unit)) + " Slime";
            DrawGameText(name.c_str(), (int)x + 10, (int)y + 31, 13, WHITE);

            // Stats summary
            std::string stats_lbl = "HP " + std::to_string(stats.max_hp) +
                                    "  ATK " + std::to_string(stats.atk);
            DrawGameText(stats_lbl.c_str(), (int)x + 10, (int)y + 51, 11,
                         LIGHTGRAY);

            // Cost tag
            std::string cost_str = std::to_string(cost) + " G";
            DrawGameText(cost_str.c_str(), (int)x + 10, (int)y + 70, 14, GOLD);

            // BUY button
            Rectangle buy{x + 8, y + kShopCardH - 36, kShopCardW - 16, 28};
            bool buy_hover =
                shop_buyable && CheckCollisionPointRec(GetMousePosition(), buy);
            Color buy_col = shop_buyable ? (buy_hover ? Color{40, 130, 70, 255}
                                                      : Color{30, 90, 50, 255})
                                         : Color{45, 45, 52, 255};
            DrawRectangleRounded(buy, 0.3f, 6, buy_col);
            const char *buy_lbl = shop_frozen ? "LOCKED" : "BUY";
            int buy_w = MeasureGameText(buy_lbl, 13, true);
            DrawGameText(buy_lbl, (int)(buy.x + buy.width / 2 - buy_w / 2),
                         (int)(buy.y + 7), 13, shop_buyable ? WHITE : GRAY,
                         true);

            if (buy_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ApplyModeUpdate(engine::act_buy(mode_, slot));
            }
        }

        // Action block (refresh / freeze / level), same footprint as one card.
        float action_x = ShopBlockX(kShopCardCount);
        float act_btn_h = (kShopCardH - 2.0f * 6.0f) / 3.0f;
        auto draw_action = [&](const char *text, float y, Color col,
                               Color hover_col, bool enabled, auto action_func) {
            Rectangle b{action_x, y, kShopCardW, act_btn_h};
            bool hover =
                enabled && CheckCollisionPointRec(GetMousePosition(), b);
            DrawRectangleRounded(b, 0.18f, 6,
                                 enabled ? (hover ? hover_col : col)
                                         : Color{45, 45, 52, 255});
            int text_w = MeasureGameText(text, 13, true);
            DrawGameText(text, (int)(b.x + kShopCardW / 2 - text_w / 2),
                         (int)(b.y + act_btn_h / 2 - 7), 13,
                         enabled ? WHITE : GRAY, true);
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                action_func();
            }
        };

        // Refresh — disabled while frozen.
        draw_action("REFRESH 1G", row_y, Color{140, 60, 30, 255},
                    Color{180, 80, 40, 255}, shop_buyable,
                    [&]() { ApplyModeUpdate(engine::act_refresh(mode_)); });

        // Freeze toggle — always available so the player can unfreeze.
        draw_action(
            shop_frozen ? "FROZEN" : "FREEZE", row_y + act_btn_h + 6.0f,
            shop_frozen ? Color{30, 144, 255, 255} : Color{20, 100, 130, 255},
            shop_frozen ? Color{50, 190, 240, 255} : Color{30, 130, 170, 255},
            can_prepare, [&]() { ApplyModeUpdate(engine::act_freeze(mode_)); });

        // Level up
        int lvl_cost = engine::session(mode_).player_.level * 5 + 5;
        std::string level_lbl = "LEVEL " + std::to_string(lvl_cost) + "G";
        draw_action(level_lbl.c_str(), row_y + 2.0f * (act_btn_h + 6.0f),
                    Color{30, 60, 140, 255}, Color{40, 80, 180, 255},
                    can_prepare,
                    [&]() { ApplyModeUpdate(engine::act_level(mode_)); });

        // Drag a board unit onto the shop area to sell it.
        if (can_prepare && is_dragging_) {
            bool over_shop =
                CheckCollisionPointRec(GetMousePosition(), shop_panel);
            auto src_u = get_unit(engine::session(mode_).board_, drag_source_);
            int price = 0;
            if (src_u) {
                auto &stats = unit::stats(*src_u);
                price = (stats.level == 1)
                            ? 2
                            : ((stats.level == 2) ? 5
                                                  : ((stats.level == 3) ? 14
                                                                        : 42));
            }
            Color glow = over_shop ? RED : GOLD;
            DrawRectangleRounded(shop_panel, 0.04f, 8,
                                 Fade(glow, over_shop ? 0.22f : 0.10f));
            DrawRectangleRoundedLinesEx(shop_panel, 0.04f, 8, 2.0f, glow);
            std::string sell_text =
                (over_shop ? "RELEASE TO SELL  +" : "DROP HERE TO SELL  +") +
                std::to_string(price) + "G";
            int tw = MeasureGameText(sell_text.c_str(), 18, true);
            DrawRectangle(
                (int)(shop_panel.x + shop_panel.width / 2 - tw / 2 - 12),
                (int)(shop_panel.y + shop_panel.height / 2 - 16), tw + 24, 34,
                Color{15, 15, 20, 235});
            DrawGameText(sell_text.c_str(),
                         (int)(shop_panel.x + shop_panel.width / 2 - tw / 2),
                         (int)(shop_panel.y + shop_panel.height / 2 - 9), 18,
                         glow, true);
        }
    }

    // 5.5 Draw 3D health/mana/shield/star bars on top of 3D slimes in 2D space
    for (auto &pair : slimes_) {
        auto u_ptr = pair.first;
        VisualSlime &v = pair.second;
        if (v.is_dead)
            continue;

        // Skip enemy units in Prep phase
        if (!engine::is_combat(mode_)) {
            if (std::holds_alternative<HexCoord>(v.last_coord)) {
                auto hex = std::get<HexCoord>(v.last_coord);
                if (hex.r < 4)
                    continue;
            }
        } else {
            // Hide bench units HUD in combat phase
            if (std::holds_alternative<LinearCoord>(v.last_coord)) {
                continue;
            }
        }

        auto stats = unit::stats(*u_ptr);

        float base_scale = 0.15f;
        if (stats.level == 2)
            base_scale = 0.21f;
        else if (stats.level == 3)
            base_scale = 0.28f;
        else if (stats.level == 4)
            base_scale = 0.36f;

        Vector3 bar_3d_pos = v.current_pos;
        if (is_dragging_ && drag_source_ == v.last_coord) {
            Ray ray = GetScreenToWorldRay(GetMousePosition(), camera_);
            if (ray.direction.y < 0) {
                float t = -ray.position.y / ray.direction.y;
                bar_3d_pos =
                    Vector3Add(ray.position, Vector3Scale(ray.direction, t));
            }
        } else if (v.move_time < 1.0f) {
            bar_3d_pos = Vector3Lerp(v.current_pos, v.target_pos, v.move_time);
            bar_3d_pos.y += sin(v.move_time * PI) * 0.35f;
        } else {
            bar_3d_pos = v.target_pos;
        }

        bar_3d_pos.y += (0.075f + base_scale * 2.0f) + 0.35f;

        Vector2 screen_pos = GetWorldToScreen(bar_3d_pos, camera_);

        if (screen_pos.x > 0 && screen_pos.x < 1280 && screen_pos.y > 0 &&
            screen_pos.y < 720) {
            int bar_w = 48;
            int bar_h = 4;
            int x = (int)screen_pos.x - bar_w / 2;
            int y = (int)screen_pos.y;

            // Background
            DrawRectangle(x - 1, y - 1, bar_w + 2, bar_h * 2 + 4,
                          Color{15, 15, 15, 220});

            // HP bar
            float hp_ratio =
                stats.max_hp > 0 ? (float)stats.hp / stats.max_hp : 0.0f;
            Color hp_color =
                (stats.owner == unit::Owner::PlayerCtrl) ? GREEN : RED;
            DrawRectangle(x, y, (int)(bar_w * hp_ratio), bar_h, hp_color);

            // Shield overlay
            if (stats.shield > 0) {
                float shield_ratio = stats.max_hp > 0
                                         ? (float)stats.shield / stats.max_hp
                                         : 0.0f;
                DrawRectangle(x, y, (int)(bar_w * std::min(1.0f, shield_ratio)),
                              2, Color{220, 220, 255, 255});
            }

            // Mana bar
            float mana_ratio =
                stats.max_mana > 0 ? (float)stats.mana / stats.max_mana : 0.0f;
            DrawRectangle(x, y + bar_h + 2, (int)(bar_w * mana_ratio), bar_h,
                          BLUE);

            // Level stars
            float star_spacing = 11.0f;
            float total_width = (stats.level - 1) * star_spacing;
            float x_start = x + bar_w / 2.0f - total_width / 2.0f;
            Color star_col = (stats.level == 4) ? ORANGE : YELLOW;
            for (int s = 0; s < stats.level; ++s) {
                float star_x = x_start + s * star_spacing;
                float star_y = y - 6.0f;
                DrawStar2D(star_x, star_y, 5.0f, 2.0f, star_col);
            }
        }
    }

    // 5.7 Dim the board while ready and waiting for the opponent (multiplayer).
    // The top bar stays bright so the CANCEL button remains usable.
    bool mp_waiting =
        local_ready && !engine::is_combat(mode_) && !engine::result_announced(mode_);
    if (mp_waiting) {
        DrawRectangle(0, 60, 1280, 660, Fade(BLACK, 0.55f));
        const char *wt = "WAITING FOR OPPONENT";
        int ww = MeasureGameText(wt, 36, true);
        DrawGameText(wt, 640 - ww / 2, 300, 36, GOLD, true);
        const char *sub = "Click CANCEL in the top bar to keep editing.";
        int sw = MeasureGameText(sub, 18, false);
        DrawGameText(sub, 640 - sw / 2, 350, 18, LIGHTGRAY, false);
    }

    // 5.8 Opponent summary panel (multiplayer only), drawn after the dim so it
    // stays readable while waiting.
    if (is_multiplayer) {
        engine::OpponentInfo opp = engine::opponent_info(mode_);
        if (opp.available) {
            const int pw = 232;
            const int ph = 58;
            const int px = 1280 - pw - 16;
            const int py = 68;
            DrawRectangle(px, py, pw, ph, Color{16, 16, 20, 235});
            DrawRectangleLines(px, py, pw, ph, Color{45, 45, 55, 255});

            DrawGameText("OPPONENT", px + 12, py + 6, 13, LIGHTGRAY);

            // Ready indicator dot + label on the right of the header.
            Color ready_col = opp.ready ? GREEN : Color{90, 90, 100, 255};
            const char *ready_lbl = opp.ready ? "READY" : "PREP";
            int rl_w = MeasureGameText(ready_lbl, 11, true);
            DrawCircle(px + pw - 18 - rl_w - 8, py + 13, 4.0f, ready_col);
            DrawGameText(ready_lbl, px + pw - 12 - rl_w, py + 7, 11, ready_col,
                         true);

            // HP bar
            int hb_x = px + 12;
            int hb_y = py + 30;
            int hb_w = 128;
            DrawRectangle(hb_x, hb_y, hb_w, 14, DARKGRAY);
            float opp_hp_pct = std::clamp(opp.hp / 100.0f, 0.0f, 1.0f);
            DrawRectangle(hb_x, hb_y, (int)(hb_w * opp_hp_pct), 14,
                          opp_hp_pct > 0.4f ? GREEN : RED);
            std::string hp_lbl = std::to_string(opp.hp) + "/100";
            DrawGameText(hp_lbl.c_str(), hb_x + 4, hb_y + 1, 11, WHITE);

            // Level / round
            std::string lv_lbl = "Lv " + std::to_string(opp.level) + "  Rd " +
                                 std::to_string(opp.round);
            DrawGameText(lv_lbl.c_str(), hb_x + hb_w + 10, hb_y + 1, 12,
                         SKYBLUE);
        }
    }

    // 5.9 Draw hover tooltip (resonance / equipment / unit) on top of the HUD
    if (tip_show)
        DrawTooltipBox(tip_title, tip_lines, tip_accent, mouse);

    // 6. Draw Combat Result popup overlay
    if (engine::result_announced(mode_)) {
        engine::CombatResult result = engine::combat_result(mode_);
        bool won = engine::player_won_combat(mode_);
        bool draw = result == engine::CombatResult::Draw;
        Color result_color = won ? GREEN : (draw ? GOLD : RED);

        DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.7f));

        int box_w = 400;
        int box_h = 220;
        int box_x = 640 - box_w / 2;
        int box_y = 360 - box_h / 2;

        DrawRectangle(box_x, box_y, box_w, box_h, Color{25, 25, 30, 255});
        DrawRectangleLines(box_x, box_y, box_w, box_h, result_color);

        std::string title = engine::result_title(result);
        DrawGameText(title.c_str(),
                     box_x + box_w / 2 - MeasureGameText(title.c_str(), 28) / 2,
                     box_y + 30, 28, result_color);

        std::string desc =
            won ? "You cleared the stage. Obtained Gold!"
                : (draw ? "Both teams fell at the same time."
                        : "Your forces fell. Lost player health!");
        if (engine::session(mode_).player_.hp <= 0) {
            desc = "Your health reached 0! GAME OVER!";
        }
        DrawGameText(desc.c_str(),
                     box_x + box_w / 2 - MeasureGameText(desc.c_str(), 16) / 2,
                     box_y + 80, 16, LIGHTGRAY);

        // OK Button to close overlay
        int ok_w = 120;
        int ok_h = 35;
        int ok_x = 640 - ok_w / 2;
        int ok_y = box_y + 140;

        bool hover = CheckCollisionPointRec(
            GetMousePosition(),
            {(float)ok_x, (float)ok_y, (float)ok_w, (float)ok_h});
        DrawRectangle(ok_x, ok_y, ok_w, ok_h,
                      hover ? Color{50, 50, 60, 255} : Color{40, 40, 50, 255});
        DrawRectangleLines(ok_x, ok_y, ok_w, ok_h, GRAY);
        DrawGameText("OK", ok_x + ok_w / 2 - MeasureGameText("OK", 16) / 2,
                     ok_y + 10, 16, WHITE);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ApplyModeUpdate(engine::acknowledge_result(mode_));
        }
    }
}

void GameApp::DrawGameText(const char *text, int posX, int posY, int fontSize,
                           Color color, bool bold) {
    Font font = bold ? font_bold_ : font_regular_;
    if (font.texture.id == 0) {
        DrawText(text, posX, posY, fontSize,
                 color); // Fallback to Raylib default if font not loaded
    } else {
        DrawTextEx(font, text, {(float)posX, (float)posY}, (float)fontSize,
                   1.0f, color);
    }
}

int GameApp::MeasureGameText(const char *text, int fontSize, bool bold) {
    Font font = bold ? font_bold_ : font_regular_;
    if (font.texture.id == 0) {
        return MeasureText(
            text, fontSize); // Fallback to Raylib default if font not loaded
    } else {
        Vector2 size = MeasureTextEx(font, text, (float)fontSize, 1.0f);
        return (int)size.x;
    }
}

void GameApp::DrawTooltipBox(const std::string &title,
                             const std::vector<std::string> &lines,
                             Color accent, Vector2 anchor) {
    const int title_size = 19;
    const int line_size = 15;
    const int pad = 13;
    const int line_gap = 7;

    int max_w = MeasureGameText(title.c_str(), title_size, true);
    for (const auto &l : lines)
        max_w = std::max(max_w, MeasureGameText(l.c_str(), line_size, false));

    int box_w = max_w + pad * 2;
    int box_h =
        pad * 2 + title_size + 10 + (int)lines.size() * (line_size + line_gap);

    // Position next to the anchor, clamped to the screen
    float bx = anchor.x + 20.0f;
    float by = anchor.y + 20.0f;
    if (bx + box_w > 1280.0f)
        bx = anchor.x - box_w - 20.0f;
    if (bx < 8.0f)
        bx = 8.0f;
    if (by + box_h > 720.0f)
        by = 720.0f - box_h - 8.0f;
    if (by < 8.0f)
        by = 8.0f;

    Rectangle box{bx, by, (float)box_w, (float)box_h};

    // Drop shadow, panel, accent border
    DrawRectangleRounded({bx + 5, by + 6, box.width, box.height}, 0.05f, 8,
                         Fade(BLACK, 0.45f));
    DrawRectangleRounded(box, 0.05f, 8, Color{18, 18, 24, 248});
    DrawRectangleRoundedLinesEx(box, 0.05f, 8, 2.0f, accent);

    int tx = (int)bx + pad;
    int ty = (int)by + pad;
    DrawGameText(title.c_str(), tx, ty, title_size, accent, true);
    int divider_y = ty + title_size + 5;
    DrawLine(tx, divider_y, tx + max_w, divider_y, Fade(accent, 0.45f));

    ty = divider_y + 10;
    for (const auto &l : lines) {
        DrawGameText(l.c_str(), tx, ty, line_size, Color{225, 225, 235, 255},
                     false);
        ty += line_size + line_gap;
    }
}

} // namespace Synera::gui
