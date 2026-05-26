#include "gui/GameApp.hpp"
#include "engine/Serialization.hpp"
#include "raymath.h"
#include "unit/Synergy.hpp"
#include "unit/UnitImpl.hpp" // IWYU pragma: keep
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Synera::gui {

using namespace Synera::engine;
using namespace Synera::unit;

Board clone_board(const Board &src) {
    Board dst;
    init_board(dst);
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            HexCoord coord{r, c};
            if (auto u_ptr = get_unit(src, coord)) {
                set_unit(dst, coord, std::make_shared<Unit>(*u_ptr));
            }
        }
    }
    for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
        LinearCoord coord{i};
        if (auto u_ptr = get_unit(src, coord)) {
            set_unit(dst, coord, std::make_shared<Unit>(*u_ptr));
        }
    }
    return dst;
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

GameApp::GameApp() {
    // 1. Initialize Raylib window
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight,
               "Synera: Synergy Auto-Arena (3D Raylib)");
    SetTargetFPS(60);

    // 2. Initialize Camera
    camera_.position = {0.0f, 15.0f, 10.0f};
    camera_.target = {0.0f, 0.0f, 3.0f};
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
    font_regular_ = LoadFontEx("Exo2-Regular.ttf", 96, NULL, 0);
    if (font_regular_.texture.id == 0) {
        font_regular_ = LoadFontEx("../Exo2-Regular.ttf", 96, NULL, 0);
    }
    font_bold_ = LoadFontEx("Exo2-Bold.ttf", 96, NULL, 0);
    if (font_bold_.texture.id == 0) {
        font_bold_ = LoadFontEx("../Exo2-Bold.ttf", 96, NULL, 0);
    }

    if (font_regular_.texture.id != 0) {
        SetTextureFilter(font_regular_.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (font_bold_.texture.id != 0) {
        SetTextureFilter(font_bold_.texture, TEXTURE_FILTER_BILINEAR);
    }
}

GameApp::~GameApp() {
    UnloadModel(hex_model_);
    UnloadFont(font_regular_);
    UnloadFont(font_bold_);
    CloseWindow();
}

void GameApp::Run() {
    while (!WindowShouldClose()) {
        Update();
        Draw();
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
    // Position Bench in a horizontal line in front of the board (Z-forward)
    float cx = (slot - 3.5f) * spacing;
    float cz = 3.5f * vertical_spacing + 1.2f * spacing;
    return {cx, 0.0f, cz};
}

Vector3 GameApp::GetEquipWorldPos(int slot) {
    float spacing = 1.0f;
    float vertical_spacing = spacing * 0.866f;
    // Position Equipment pool below the bench
    float cx = (slot - 3.5f) * spacing;
    float cz = 3.5f * vertical_spacing + 1.2f * spacing + 1.0f * spacing;
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
    int start_row = is_combat_ ? 0 : 4;
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
    if (m.y >= 500)
        return true;
    if (m.y <= 60)
        return true;
    if (m.x >= 20 && m.x <= 240 && m.y >= 120 && m.y <= 480)
        return true;
    if (combat_result_announced_)
        return true;
    return false;
}

void GameApp::Update() {
    // 1. Smoothly interpolate camera position and target depending on phase
    if (is_combat_) {
        target_cam_pos_ = {0.0f, 15.0f, 3.0f};
        target_cam_target_ = {0.0f, 0.0f, 1.4f};
    } else {
        target_cam_pos_ = {0.0f, 8.5f, 9.0f};
        target_cam_target_ = {0.0f, 0.0f, 4.2f};
    }
    camera_.position =
        Vector3Lerp(camera_.position, target_cam_pos_, GetFrameTime() * 4.0f);
    camera_.target =
        Vector3Lerp(camera_.target, target_cam_target_, GetFrameTime() * 4.0f);

    // 2. Decrement status message timer
    if (status_msg_timer_ > 0.0f) {
        status_msg_timer_ -= GetFrameTime();
    }

    // 3. Handle inputs (clicking/dragging/actions) in Prep phase
    if (!is_combat_) {
        HandleInputs();
    }

    // 4. Combat loop update
    if (is_combat_ && !combat_result_announced_) {
        combat_tick_timer += GetFrameTime();
        while (combat_tick_timer >= combat_tick_interval) {
            combat_tick_timer -= combat_tick_interval;
            ProcessCombatTick();
            if (combat_result_announced_)
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
    engine::Board &active_board = is_combat_ ? combat_board_ : session_.board_;

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
                        v.current_pos = GetWorldPos(v.last_coord);
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
                    v.current_pos = GetWorldPos(v.last_coord);
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
                hovered_equip_index_ < (int)session_.equip_pool_.size()) {
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

                if (get_unit(session_.board_, hovered_coord_)) {
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

        if (mouse_pos.x >= 1055 && mouse_pos.x <= 1230 && mouse_pos.y >= 660 &&
            mouse_pos.y <= 705) {
            auto src_u = get_unit(session_.board_, drag_source_);
            if (src_u) {
                auto &stats = unit::stats(*src_u);
                int price =
                    (stats.level == 1)
                        ? 2
                        : ((stats.level == 2) ? 5
                                              : ((stats.level == 3) ? 14 : 42));
                session_.player_.gold += price;

                if (!std::holds_alternative<std::monostate>(stats.equipped)) {
                    session_.equip_pool_.push_back(stats.equipped);
                }

                remove_unit(session_.board_, drag_source_);
                status_msg_ =
                    "Sold unit for " + std::to_string(price) + " Gold.";
                status_msg_timer_ = 2.0f;
            }
            return;
        }

        if (!over_ui) {
            auto [drop_cell, drop_found] = GetCellUnderMouse();
            if (drop_found && !(drag_source_ == drop_cell)) {
                bool valid = true;
                if (std::holds_alternative<engine::HexCoord>(drop_cell)) {
                    auto hex = std::get<engine::HexCoord>(drop_cell);
                    if (hex.r < 4) {
                        valid = false;
                        status_msg_ =
                            "Cannot place units on Enemy half (rows 0-3)!";
                        status_msg_timer_ = 2.5f;
                    } else {
                        int board_units = 0;
                        for (int r = 4; r < config::engine::BOARD_ROWS; ++r) {
                            for (int c = 0; c < config::engine::BOARD_COLS;
                                 ++c) {
                                if (auto u = get_unit(session_.board_,
                                                      engine::HexCoord{r, c})) {
                                    if (unit::stats(*u).owner ==
                                        unit::Owner::PlayerCtrl) {
                                        board_units++;
                                    }
                                }
                            }
                        }
                        bool is_moving_to_empty =
                            !get_unit(session_.board_, drop_cell);
                        bool is_src_bench =
                            std::holds_alternative<engine::LinearCoord>(
                                drag_source_);

                        if (is_src_bench && is_moving_to_empty &&
                            board_units >= session_.player_.level) {
                            valid = false;
                            status_msg_ = "Board is full! Upgrade level to "
                                          "deploy more units.";
                            status_msg_timer_ = 2.5f;
                        }
                    }
                }

                if (valid) {
                    auto src_u = get_unit(session_.board_, drag_source_);
                    auto dst_u = get_unit(session_.board_, drop_cell);

                    if (src_u && slimes_.find(src_u) != slimes_.end()) {
                        Ray ray =
                            GetScreenToWorldRay(GetMousePosition(), camera_);
                        if (ray.direction.y < 0) {
                            float t = -ray.position.y / ray.direction.y;
                            Vector3 hit = Vector3Add(
                                ray.position, Vector3Scale(ray.direction, t));
                            slimes_[src_u].current_pos = hit;
                        }
                    }

                    set_unit(session_.board_, drag_source_, dst_u);
                    set_unit(session_.board_, drop_cell, src_u);
                    session_.check_and_merge(drop_cell);
                }
            }
        }
    }

    if (is_dragging_equip_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        is_dragging_equip_ = false;
        if (!over_ui && has_hover_) {
            auto u_ptr = get_unit(session_.board_, hovered_coord_);
            if (u_ptr) {
                bool ok = session_.equip_unit(hovered_coord_,
                                              drag_equip_source_index_);
                if (ok) {
                    status_msg_ = "Equipped unit successfully!";
                    status_msg_timer_ = 1.5f;
                } else {
                    status_msg_ = "Failed: unit already has equipment!";
                    status_msg_timer_ = 1.5f;
                }
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
    // 1. Verify we have at least one player unit on the board
    int player_units = 0;
    for (int r = 4; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            if (get_unit(session_.board_, engine::HexCoord{r, c})) {
                player_units++;
            }
        }
    }

    if (player_units == 0) {
        status_msg_ =
            "Deploy at least one unit to the board before starting combat!";
        status_msg_timer_ = 3.0f;
        return;
    }

    // 2. Clone board and clean up visuals
    prep_board_copy_ = clone_board(session_.board_); // save prep state
    combat_board_ = clone_board(session_.board_);

    // Clear dead visual states
    slimes_.clear();
    projectiles_.clear();

    // 3. Compute and apply synergies to the combat board
    auto synergies = unit::compute_synergies(combat_board_);
    unit::apply_combat_synergies(combat_board_, synergies);

    // 4. Begin combat loop
    is_combat_ = true;
    combat_result_announced_ = false;
    combat_tick_timer = 0.0f;
    ticks_elapsed_ = 0;
    status_msg_ = "Combat Phase! Units auto-battling...";
}

void GameApp::ProcessCombatTick() {
    bool player_won = false;
    if (battle_engine_.is_combat_over(combat_board_, player_won)) {
        player_won_combat_ = player_won;
        EndCombatPhase();
        return;
    }

    battle_engine_.tick(combat_board_);
    ticks_elapsed_++;

    if (battle_engine_.is_combat_over(combat_board_, player_won)) {
        player_won_combat_ = player_won;
        EndCombatPhase();
    }
}

void GameApp::EndCombatPhase() {
    combat_result_announced_ = true;

    if (player_won_combat_) {
        int reward_gold = 2 + session_.player_.level * 2;
        session_.player_.gold += reward_gold;
        status_msg_ =
            "VICTORY! Gained " + std::to_string(reward_gold) + " Gold.";

        // 30% drop rate of equipment item
        if ((rand() % 100) < 30) {
            unit::Element random_elem = static_cast<unit::Element>(rand() % 6);
            unit::Equipment item;
            switch (random_elem) {
            case unit::Element::Pyro:
                item = unit::PyroDrop{};
                break;
            case unit::Element::Hydro:
                item = unit::HydroDrop{};
                break;
            case unit::Element::Anemo:
                item = unit::AnemoDrop{};
                break;
            case unit::Element::Geo:
                item = unit::GeoDrop{};
                break;
            case unit::Element::Electro:
                item = unit::ElectroDrop{};
                break;
            case unit::Element::Cryo:
                item = unit::CryoDrop{};
                break;
            }
            session_.equip_pool_.push_back(item);
            status_msg_ +=
                " Dropped a " + GetElementName(random_elem) + " Drop!";
        }
    } else {
        // Count surviving enemy units
        int survivors = 0;
        for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
            for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                if (auto u = get_unit(combat_board_, engine::HexCoord{r, c})) {
                    if (unit::stats(*u).owner == unit::Owner::EnemyCtrl &&
                        unit::stats(*u).hp > 0) {
                        survivors++;
                    }
                }
            }
        }
        int damage = 10 + 2 * survivors;
        session_.player_.hp = std::max(0, session_.player_.hp - damage);
        status_msg_ = "DEFEAT! Took " + std::to_string(damage) + " damage.";
        if (session_.player_.hp <= 0) {
            status_msg_ += " GAME OVER!";
        }
    }
}

void GameApp::Draw() {
    BeginDrawing();
    ClearBackground(Color{24, 24, 28, 255}); // modern dark theme background

    // 1. Draw 3D elements
    BeginMode3D(camera_);
    DrawGame3D();
    EndMode3D();

    // 2. Draw 2D UI overlay
    DrawGame2D();

    EndDrawing();
}

void GameApp::DrawGame3D() {
    engine::Board &active_board = is_combat_ ? combat_board_ : session_.board_;

    // 1. Draw Board hexagonal cells
    int r_start = is_combat_ ? 0 : 4;
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
    if (!is_combat_) {
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
    if (!is_combat_) {
        for (int i = 0; i < 8; ++i) {
            Vector3 pos = GetEquipWorldPos(i);
            bool eq_highlight = (hovered_equip_index_ == i);
            DrawHexCell(pos, Color{45, 45, 55, 255}, eq_highlight);

            // Draw equipment sphere floating in slot if present
            if (i < (int)session_.equip_pool_.size()) {
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

                unit::Equipment eq = session_.equip_pool_[i];
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
        if (!is_combat_) {
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
    // A cylinder with 6 slices is a hexagonal prism. We rotate by 30 degrees
    // (pi/6) around the Y axis so flat edges are parallel to horizontal
    // neighbor lines.
    Vector3 rotationAxis = {0.0f, 1.0f, 0.0f};
    float rotationAngle = 30.0f; // degrees

    DrawModelEx(hex_model_, pos, rotationAxis, rotationAngle,
                {1.0f, 1.0f, 1.0f}, color);

    // Draw wireframe outline to make cells distinct
    Color wire_color = highlight ? YELLOW : Color{35, 35, 45, 255};
    DrawModelWiresEx(hex_model_, pos, rotationAxis, rotationAngle,
                     {1.01f, 1.01f, 1.01f}, wire_color);
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

void GameApp::DrawGame2D() {
    // 1. Draw HUD top stats bar
    DrawRectangle(0, 0, 1280, 60, Color{16, 16, 20, 240});
    DrawRectangleLines(0, 0, 1280, 60, Color{40, 40, 50, 255});

    // Player HP
    DrawGameText("PLAYER HP:", 30, 20, 22, LIGHTGRAY);
    DrawRectangle(150, 20, 200, 20, DARKGRAY);
    float hp_pct = session_.player_.hp / 100.0f;
    DrawRectangle(150, 20, (int)(200 * hp_pct), 20,
                  hp_pct > 0.4f ? GREEN : RED);
    std::string hp_text = std::to_string(session_.player_.hp) + "/100";
    DrawGameText(hp_text.c_str(), 220, 22, 18, WHITE);

    // Player Gold
    DrawGameText("GOLD:", 400, 20, 22, LIGHTGRAY);
    std::string gold_text = std::to_string(session_.player_.gold) + " G";
    DrawGameText(gold_text.c_str(), 470, 20, 22, GOLD);

    // Player Level
    DrawGameText("LEVEL:", 580, 20, 22, LIGHTGRAY);
    std::string level_text = std::to_string(session_.player_.level);
    DrawGameText(level_text.c_str(), 660, 20, 22, SKYBLUE);

    // Board population count (only count PlayerCtrl units)
    int board_units = 0;
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            if (auto u = get_unit(is_combat_ ? combat_board_ : session_.board_,
                                  engine::HexCoord{r, c})) {
                if (unit::stats(*u).owner == unit::Owner::PlayerCtrl) {
                    board_units++;
                }
            }
        }
    }
    std::string pop_text = "(" + std::to_string(board_units) + "/" +
                           std::to_string(session_.player_.level) + ")";
    DrawGameText(pop_text.c_str(), 690, 22, 18, LIGHTGRAY);

    // Round number
    DrawGameText("ROUND:", 800, 20, 22, LIGHTGRAY);
    std::string round_text = std::to_string(session_.round_);
    DrawGameText(round_text.c_str(), 890, 20, 22, WHITE);

    // Phase Indicator
    if (is_combat_) {
        DrawRectangle(980, 15, 120, 30, RED);
        DrawGameText("COMBAT", 1005, 20, 20, WHITE);
    } else {
        DrawRectangle(980, 15, 120, 30, GREEN);
        DrawGameText("PREPARE", 1000, 20, 20, WHITE);
    }

    // 2. Draw Status Message banner
    if (status_msg_timer_ > 0.0f) {
        int width = MeasureGameText(status_msg_.c_str(), 22);
        DrawRectangle(640 - width / 2 - 20, 80, width + 40, 40,
                      Color{20, 20, 25, 220});
        DrawRectangleLines(640 - width / 2 - 20, 80, width + 40, 40, GOLD);
        DrawGameText(status_msg_.c_str(), 640 - width / 2, 90, 20, GOLD);
    }

    // 3. Draw Active Synergies list on the left side
    DrawRectangle(20, 120, 220, 360, Color{16, 16, 20, 200});
    DrawRectangleLines(20, 120, 220, 360, Color{40, 40, 50, 255});
    DrawGameText("ACTIVE RESONANCES", 30, 130, 18, GOLD);

    auto active_synergies =
        unit::compute_synergies(is_combat_ ? combat_board_ : session_.board_);

    int sy_y = 170;
    auto draw_synergy_item = [&](const char *name, int count, bool active,
                                 Color col) {
        std::string text = std::string(name) + ": " + std::to_string(count);
        DrawGameText(text.c_str(), 35, sy_y, 16, active ? col : DARKGRAY);
        if (active)
            DrawRectangle(180, sy_y + 2, 40, 12, col);
        sy_y += 24;
    };

    draw_synergy_item("Pyro (ATK +25%)", active_synergies.pyro_count,
                      active_synergies.fervent_flames, RED);
    draw_synergy_item("Hydro (HP +25%)", active_synergies.hydro_count,
                      active_synergies.soothing_water, BLUE);
    draw_synergy_item("Anemo (Mana -15)", active_synergies.anemo_count,
                      active_synergies.impetuous_winds, SKYBLUE);
    draw_synergy_item("Geo (Shield +15%)", active_synergies.geo_count,
                      active_synergies.enduring_rock, GOLD);
    draw_synergy_item("Electro (Mana+5)", active_synergies.electro_count,
                      active_synergies.high_voltage, PURPLE);
    draw_synergy_item("Cryo (Freeze 20%)", active_synergies.cryo_count,
                      active_synergies.shattering_ice,
                      Color{180, 220, 255, 255});
    draw_synergy_item(
        "Canopy (-15% DMG)",
        active_synergies.pyro_count + active_synergies.hydro_count +
                    active_synergies.anemo_count + active_synergies.geo_count >
                3
            ? 4
            : 0,
        active_synergies.protective_canopy, GREEN);

    // 5. Draw Shop panel at the bottom
    DrawRectangle(0, 500, 1280, 220, Color{20, 20, 24, 255});
    DrawRectangleLines(0, 500, 1280, 220, Color{40, 40, 50, 255});
    DrawGameText("RECRUITMENT SHOP", 50, 512, 18, GOLD);

    // Shop slots
    for (int i = 0; i < 5; ++i) {
        int x = 50 + i * 160;
        int y = 536;
        int w = 145;
        int h = 160;

        DrawRectangle(x, y, w, h, Color{28, 28, 34, 255});
        DrawRectangleLines(x, y, w, h, Color{55, 55, 65, 255});

        if (session_.shop_[i].has_value()) {
            auto &[unit, cost] = *session_.shop_[i];
            Color col = GetElementColor(unit::element(unit));
            auto stats = unit::stats(unit);

            // Draw element color stripe
            DrawRectangle(x, y, w, 8, col);

            // Star levels
            std::string stars = "";
            for (int s = 0; s < stats.level; ++s)
                stars += "*";
            DrawGameText(stars.c_str(), x + 10, y + 18, 20, YELLOW);

            // Element Name
            std::string name = GetElementName(unit::element(unit)) + " Slime";
            DrawGameText(name.c_str(), x + 10, y + 42, 16, WHITE);

            // Stats summary
            std::string stats_lbl = "HP: " + std::to_string(stats.max_hp) +
                                    " ATK: " + std::to_string(stats.atk);
            DrawGameText(stats_lbl.c_str(), x + 10, y + 68, 12, LIGHTGRAY);

            // Cost tag
            std::string cost_str = "COST: " + std::to_string(cost) + " G";
            DrawGameText(cost_str.c_str(), x + 10, y + 95, 16, GOLD);

            // BUY Button
            int btn_x = x + 10;
            int btn_y = y + 120;
            int btn_w = w - 20;
            int btn_h = 30;

            bool hover = CheckCollisionPointRec(
                GetMousePosition(),
                {(float)btn_x, (float)btn_y, (float)btn_w, (float)btn_h});
            DrawRectangle(btn_x, btn_y, btn_w, btn_h,
                          hover ? Color{40, 110, 60, 255}
                                : Color{30, 80, 45, 255});
            DrawGameText("BUY", btn_x + btn_w / 2 - 14, btn_y + 8, 14, WHITE);

            // Click check
            if (!is_combat_ && hover &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                bool ok = session_.buy_unit(i);
                if (!ok) {
                    status_msg_ = "Insufficient gold or bench slots!";
                    status_msg_timer_ = 2.0f;
                } else {
                    status_msg_ = "Purchased Slime successfully!";
                    status_msg_timer_ = 1.5f;
                }
            }
        } else {
            DrawGameText("EMPTY", x + w / 2 - 25, y + h / 2 - 10, 16, DARKGRAY);
        }
    }

    // Shop actions
    int action_x = 875;
    auto draw_shop_action = [&](const char *text, int y, Color col,
                                Color hover_col, auto action_func) {
        int w = 150;
        int h = 45;
        bool hover = CheckCollisionPointRec(
            GetMousePosition(),
            {(float)action_x, (float)y, (float)w, (float)h});
        DrawRectangle(action_x, y, w, h, hover ? hover_col : col);
        DrawRectangleLines(action_x, y, w, h, Color{60, 60, 70, 255});

        int text_w = MeasureGameText(text, 16);
        DrawGameText(text, action_x + w / 2 - text_w / 2, y + 16, 16, WHITE);

        if (!is_combat_ && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            action_func();
        }
    };

    // Refresh Shop
    draw_shop_action("REFRESH (1G)", 536, Color{140, 60, 30, 255},
                     Color{180, 80, 40, 255}, [&]() {
                         bool ok = session_.refresh_shop(false);
                         if (!ok) {
                             status_msg_ = "Insufficient gold to refresh shop!";
                             status_msg_timer_ = 2.0f;
                         }
                     });

    // Buy XP / Level
    int lvl_cost = session_.player_.level * 5 + 5;
    std::string level_btn_lbl = "LEVEL UP (" + std::to_string(lvl_cost) + "G)";
    draw_shop_action(level_btn_lbl.c_str(), 600, Color{30, 60, 140, 255},
                     Color{40, 80, 180, 255}, [&]() {
                         bool ok = session_.buy_level();
                         if (!ok) {
                             status_msg_ = "Insufficient gold to buy level!";
                             status_msg_timer_ = 2.0f;
                         } else {
                             status_msg_ =
                                 "Upgraded player level successfully!";
                             status_msg_timer_ = 1.5f;
                         }
                     });

    // Combat controller buttons on the right side
    auto draw_combat_btn = [&](const char *text, int x, int y, int w, int h,
                               Color col, Color hover_col, auto action_func,
                               bool active) {
        bool hover = active && CheckCollisionPointRec(
                                   GetMousePosition(),
                                   {(float)x, (float)y, (float)w, (float)h});
        DrawRectangle(x, y, w, h,
                      active ? (hover ? hover_col : col) : DARKGRAY);
        DrawRectangleLines(x, y, w, h, Color{60, 60, 70, 255});
        int text_w = MeasureGameText(text, 16);
        DrawGameText(text, x + w / 2 - text_w / 2, y + h / 2 - 7, 16,
                     active ? WHITE : GRAY);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            action_func();
        }
    };

    // Start Combat
    std::string combat_lbl = is_combat_ ? "BATTLE IN PROGRESS" : "START COMBAT";
    draw_combat_btn(
        combat_lbl.c_str(), 1055, 536, 175, 45, Color{30, 110, 45, 255},
        Color{40, 140, 55, 255}, [&]() { StartCombatPhase(); }, !is_combat_);

    // Save Game
    draw_combat_btn(
        "SAVE", 1055, 600, 82, 45, Color{70, 70, 75, 255},
        Color{90, 90, 95, 255},
        [&]() {
            std::string file = "save_session.txt";
            bool ok = save(session_, file);
            if (ok) {
                status_msg_ = "Game saved successfully to save_session.txt!";
                status_msg_timer_ = 2.5f;
            } else {
                status_msg_ = "Failed to save game!";
                status_msg_timer_ = 2.5f;
            }
        },
        !is_combat_);

    // Load Game
    draw_combat_btn(
        "LOAD", 1148, 600, 82, 45, Color{70, 70, 75, 255},
        Color{90, 90, 95, 255},
        [&]() {
            std::string file = "save_session.txt";
            bool ok = load(session_, file);
            if (ok) {
                // Re-sync visual slimes
                slimes_.clear();
                projectiles_.clear();
                status_msg_ = "Game loaded successfully from save_session.txt!";
                status_msg_timer_ = 2.5f;
            } else {
                status_msg_ = "Failed to load game!";
                status_msg_timer_ = 2.5f;
            }
        },
        !is_combat_);

    // Drag-to-Sell Zone
    if (!is_combat_) {
        bool hover_sell =
            is_dragging_ &&
            CheckCollisionPointRec(GetMousePosition(), {1055, 660, 175, 45});
        DrawRectangle(1055, 660, 175, 45,
                      hover_sell ? Color{180, 40, 40, 255}
                                 : Color{120, 30, 30, 255});
        DrawRectangleLines(1055, 660, 175, 45, RED);
        DrawGameText("DRAG HERE TO SELL",
                     1055 + 175 / 2 -
                         MeasureGameText("DRAG HERE TO SELL", 14) / 2,
                     660 + 16, 14, WHITE);
    }

    // 5.5 Draw 3D health/mana/shield/star bars on top of 3D slimes in 2D space
    for (auto &pair : slimes_) {
        auto u_ptr = pair.first;
        VisualSlime &v = pair.second;
        if (v.is_dead)
            continue;

        // Skip enemy units in Prep phase
        if (!is_combat_) {
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
            std::string level_str = "";
            for (int s = 0; s < stats.level; ++s)
                level_str += "*";
            Color star_col = (stats.level == 4) ? ORANGE : YELLOW;
            DrawGameText(level_str.c_str(),
                         x + bar_w / 2 -
                             MeasureGameText(level_str.c_str(), 12) / 2,
                         y - 11, 12, star_col);
        }
    }

    // 6. Draw Combat Result popup overlay
    if (combat_result_announced_) {
        DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.7f));

        int box_w = 400;
        int box_h = 220;
        int box_x = 640 - box_w / 2;
        int box_y = 360 - box_h / 2;

        DrawRectangle(box_x, box_y, box_w, box_h, Color{25, 25, 30, 255});
        DrawRectangleLines(box_x, box_y, box_w, box_h,
                           player_won_combat_ ? GREEN : RED);

        std::string title = player_won_combat_ ? "VICTORY!" : "DEFEAT!";
        Color title_color = player_won_combat_ ? GREEN : RED;
        DrawGameText(title.c_str(),
                     box_x + box_w / 2 - MeasureGameText(title.c_str(), 28) / 2,
                     box_y + 30, 28, title_color);

        std::string desc = player_won_combat_
                               ? "You cleared the stage. Obtained Gold!"
                               : "Your forces fell. Lost player health!";
        if (session_.player_.hp <= 0) {
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
            combat_result_announced_ = false;
            // Check if player died, restart session
            if (session_.player_.hp <= 0) {
                session_ = engine::GameSession();
                slimes_.clear();
                projectiles_.clear();
                status_msg_ =
                    "New Game Started - Drag and drop units to position them.";
                status_msg_timer_ = 3.0f;
            } else {
                // Restore board to prep state
                session_.board_ = prep_board_copy_;

                // Reset stats for all units on board/bench (heal to full)
                for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
                    for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                        HexCoord cell{r, c};
                        if (auto u_ptr = get_unit(session_.board_, cell)) {
                            auto &s = unit::stats(*u_ptr);
                            s.hp = s.max_hp;
                            s.mana = 0;
                            s.state = unit::State::Idle;
                            s.stun_ticks = 0;
                            s.attack_cooldown = 0;
                            s.move_cooldown = 0;
                        }
                    }
                }
                for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
                    LinearCoord cell{i};
                    if (auto u_ptr = get_unit(session_.board_, cell)) {
                        auto &s = unit::stats(*u_ptr);
                        s.hp = s.max_hp;
                        s.mana = 0;
                        s.state = unit::State::Idle;
                        s.stun_ticks = 0;
                        s.attack_cooldown = 0;
                        s.move_cooldown = 0;
                    }
                }

                // Increment round, spawn new enemies, and reset phase
                session_.round_++;
                session_.spawn_enemies();
                session_.refresh_shop(true);

                // Gained Gold: base gold + interest (max 5G interest)
                int base_gold = 2 + session_.player_.level;
                int interest = std::min(5, session_.player_.gold / 10);
                session_.player_.gold += base_gold + interest;
                status_msg_ =
                    "Round " + std::to_string(session_.round_) +
                    " Started! Gained " + std::to_string(base_gold + interest) +
                    " G (Interest: " + std::to_string(interest) + " G).";
                status_msg_timer_ = 4.0f;

                is_combat_ = false;
                slimes_.clear();
                projectiles_.clear();
                status_msg_ =
                    "Preparation Phase - Drag and drop units to position them.";
                status_msg_timer_ = 3.0f;
            }
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

} // namespace Synera::gui
