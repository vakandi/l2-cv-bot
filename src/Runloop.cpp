#include "Runloop.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include "Utils.h"
#include "Window.h"

void Runloop::Run()
{
    ConfigureEyes();
    ConfigureHands();
    ConfigureBrain();

    const auto title = m_options.String("--window", "Lineage II");
    const auto debug = m_options.Bool("--debug", true);
    auto first = true;

    bool overlayInitialized = false;
    bool overlayVisible = true;
    bool overlayClickThrough = true;

    while (true) {
        m_capture.Clear();
        const auto window = ::Window::Find(title);

        if (!window.has_value()) {
            std::cout << "Can't find window \"" << title << "\"" << std::endl;
            break;
        }

        if (first) {
            window.value().BringToForeground();
        }

        const auto rect = window.value().Rect();
        const auto bitmap = m_capture.Grab({rect.x, rect.y, rect.width, rect.height});

        if (!bitmap.has_value()) {
            std::cout << "Failed to grab window" << std::endl;
            break;
        }

        auto image = BitmapToImage(bitmap.value()); // TODO: make const (OpenCV 4+)

        if (!image.has_value()) {
            std::cout << "Failed to convert bitmap to image" << std::endl;
            break;
        }

        if (m_hands.KeyboardKeyPressed(::Input::KeyboardKey::PrintScreen)) {
            cv::imwrite("shot.png", image.value());
            std::cout << "Screenshot saved to shot.png" << std::endl;
        }

        m_hands.SetWindowRect({rect.x, rect.y, rect.width, rect.height});
        m_eyes.Open(image.value());

        if (first) {
            m_brain.Init();
        }

        m_brain.Process();
        m_eyes.Close();

        if (debug) {
            DrawWorldInfo(image.value());

            if (!overlayInitialized) {
                m_overlay.initialize(window.value().Handle());
                overlayInitialized = true;
                m_overlay.setClickThrough(overlayClickThrough);
                m_overlay.setVisible(overlayVisible);
            }

            // follow window rect
            m_overlay.setRect(rect.x, rect.y, rect.width, rect.height);

            // toggles
            if (m_hands.KeyboardKeyPressed(::Input::KeyboardKey::F9)) {
                overlayVisible = !overlayVisible;
                m_overlay.setVisible(overlayVisible);
            }
            if (m_hands.KeyboardKeyPressed(::Input::KeyboardKey::F10)) {
                overlayClickThrough = !overlayClickThrough;
                m_overlay.setClickThrough(overlayClickThrough);
            }

            m_overlay.begin();
            DrawOverlay();
            m_overlay.end();
        }

        if (!debug && m_hands.MouseMoved(100) || m_hands.KeyboardKeyPressed(::Input::KeyboardKey::Escape)) {
            std::cout << "Bye!" << std::endl;
            break;
        } else if (m_hands.KeyboardKeyPressed(::Input::KeyboardKey::Space)) {
            m_eyes.Reset();
        }

        first = false;
    }

    cv::destroyAllWindows();
}

void Runloop::DrawWorldInfo(cv::Mat &image) const
{
    const auto npcs = m_brain.NPCs();
    const auto far_npcs = m_brain.FarNPCs();
    const auto me = m_brain.Me().value_or(::Eyes::Me());
    const auto target = m_brain.Target().value_or(::Eyes::Target());

    // help
    cv::putText(
        image,
        "Press PrtScn to take screenshot of the Lineage II window",
        {5, image.rows - 235},
        cv::FONT_HERSHEY_COMPLEX,
        0.4,
        {255, 255, 255},
        1,
        cv::LINE_AA
    );

    cv::putText(
        image,
        "Press Space to reset HP/MP/CP bars position",
        {5, image.rows - 215},
        cv::FONT_HERSHEY_COMPLEX,
        0.4,
        {255, 255, 255},
        1,
        cv::LINE_AA
    );

    cv::putText(
        image,
        "Press ESC to exit",
        {5, image.rows - 195},
        cv::FONT_HERSHEY_COMPLEX,
        0.4,
        {255, 255, 255},
        1,
        cv::LINE_AA
    );

    // my HP/MP/CP
    cv::putText(
        image,
        "My HP " + std::to_string(me.hp) + "% " +
        "MP " + std::to_string(me.mp) + "% " +
        "CP: " + std::to_string(me.cp) + "% ",
        {5, 100},
        cv::FONT_HERSHEY_COMPLEX,
        0.5,
        {0, 255, 255},
        1,
        cv::LINE_AA
    );

    // target HP
    cv::putText(
        image,
        "Target HP " + std::to_string(target.hp) + "%",
        {5, 125},
        cv::FONT_HERSHEY_COMPLEX,
        0.5,
        {0, 255, 255},
        1,
        cv::LINE_AA
    );

    // NPCs
    for (const auto &npc : npcs) {
        cv::rectangle(image, npc.rect, {255, 255, 0});
        cv::circle(image, npc.center, 10, {0, 255, 255});

        // name id
        cv::putText(
            image,
            "name id: " + std::to_string(npc.name_id),
            {npc.rect.x, npc.rect.y - 5},
            cv::FONT_HERSHEY_PLAIN,
            0.8,
            {255, 255, 255},
            1,
            cv::LINE_AA
        );

        // tracking id
        cv::putText(
            image,
            "tracking id: " + std::to_string(npc.tracking_id),
            {npc.rect.x, npc.rect.y - 20},
            cv::FONT_HERSHEY_PLAIN,
            0.8,
            {255, 255, 255},
            1,
            cv::LINE_AA
        );

        // selected
        cv::putText(
            image,
            "selected: " + std::to_string(npc.Selected()),
            {npc.rect.x, npc.rect.y - 35},
            cv::FONT_HERSHEY_PLAIN,
            0.8,
            {255, 255, 255},
            1,
            cv::LINE_AA
        );

        // hovered
        cv::putText(
            image,
            "hovered: " + std::to_string(npc.Hovered()),
            {npc.rect.x, npc.rect.y - 50},
            cv::FONT_HERSHEY_PLAIN,
            0.8,
            {255, 255, 255},
            1,
            cv::LINE_AA
        );
    }

    // far NPCs
    for (const auto &npc : far_npcs) {
        cv::rectangle(image, npc.rect, {0, 255, 255});

        cv::putText(
            image,
            std::to_string(npc.tracking_id),
            {npc.rect.x, npc.rect.y + 10},
            cv::FONT_HERSHEY_PLAIN,
            0.8,
            {0, 255, 255},
            1,
            cv::LINE_AA
        );
    }
}

void Runloop::ShowDebugInfo(cv::Mat &image)
{
    // Create a small debug info window (not the main game window)
    static bool debug_window_created = false;
    
    if (!debug_window_created) {
        cv::namedWindow("L2 Bot Debug", cv::WINDOW_AUTOSIZE);
        cv::resizeWindow("L2 Bot Debug", 400, 300);
        cv::moveWindow("L2 Bot Debug", 10, 10); // Position it in top-left corner
        debug_window_created = true;
        std::cout << "Debug info window created" << std::endl;
    }
    
    // Create a debug info image
    cv::Mat debug_image = cv::Mat::zeros(300, 400, CV_8UC3);
    
    // Get current detection info
    const auto npcs = m_brain.NPCs();
    const auto far_npcs = m_brain.FarNPCs();
    const auto me = m_brain.Me();
    const auto target = m_brain.Target();
    
    // Draw debug information
    int y = 30;
    cv::putText(debug_image, "L2 CV Bot Debug Info", cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
    y += 30;
    
    cv::putText(debug_image, "Near NPCs: " + std::to_string(npcs.size()), cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);
    y += 25;
    
    cv::putText(debug_image, "Far NPCs: " + std::to_string(far_npcs.size()), cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);
    y += 25;
    
    if (me.has_value()) {
        cv::putText(debug_image, "HP: " + std::to_string(me.value().hp) + "%", cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 1);
        y += 25;
        cv::putText(debug_image, "MP: " + std::to_string(me.value().mp) + "%", cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 1);
        y += 25;
        cv::putText(debug_image, "CP: " + std::to_string(me.value().cp) + "%", cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 0, 255), 1);
        y += 25;
    }
    
    if (target.has_value()) {
        cv::putText(debug_image, "Target HP: " + std::to_string(target.value().hp) + "%", cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 1);
        y += 25;
    }
    
    cv::putText(debug_image, "FPS: " + std::to_string(static_cast<int>(m_fps.Get())), cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);
    y += 30;
    
    cv::putText(debug_image, "Controls:", cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 0), 1);
    y += 20;
    cv::putText(debug_image, "ESC - Exit", cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
    y += 15;
    cv::putText(debug_image, "F12 - Screenshot", cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
    
    // Show the debug window
    cv::imshow("L2 Bot Debug", debug_image);
    
    // Save manual screenshot on F12
    if (m_hands.KeyboardKeyPressed(::Input::KeyboardKey::F12)) {
        cv::imwrite("game_screenshot.bmp", image);
        std::cout << "Game screenshot saved: game_screenshot.bmp" << std::endl;
    }
}

void Runloop::DrawOverlay()
{
    // Convert detections to overlay primitives
    const auto npcs = m_brain.NPCs();
    const auto far_npcs = m_brain.FarNPCs();
    const auto meOpt = m_brain.Me();
    const auto targetOpt = m_brain.Target();

    int textY = 10;
    m_overlay.drawText(10, textY, L"L2 CV Bot", {0,255,0,255}, 16.0f); textY += 20;
    m_overlay.drawText(10, textY, L"F9: overlay  F10: click-through", {255,255,0,255}, 12.0f); textY += 18;

    if (meOpt.has_value()) {
        auto s = L"HP " + std::to_wstring(meOpt->hp) + L"%  MP " + std::to_wstring(meOpt->mp) + L"%  CP " + std::to_wstring(meOpt->cp) + L"%";
        m_overlay.drawText(10, textY, s, {0,255,255,255}, 13.0f); textY += 18;
    }
    if (targetOpt.has_value()) {
        auto s = L"Target HP " + std::to_wstring(targetOpt->hp) + L"%";
        m_overlay.drawText(10, textY, s, {0,255,255,255}, 13.0f); textY += 18;
    }

    // NPCs (near)
    for (const auto &npc : npcs) {
        RECT r{npc.rect.x, npc.rect.y, npc.rect.x + npc.rect.width, npc.rect.y + npc.rect.height};
        m_overlay.drawRect(r, {255,255,0,200}, 2);
        std::wstring t = L"id:" + std::to_wstring(npc.tracking_id);
        m_overlay.drawText(npc.rect.x, npc.rect.y - 14, t, {255,255,255,255}, 11.0f);
    }

    // Far NPCs
    for (const auto &npc : far_npcs) {
        RECT r{npc.rect.x, npc.rect.y, npc.rect.x + npc.rect.width, npc.rect.y + npc.rect.height};
        m_overlay.drawRect(r, {0,255,255,200}, 1);
    }

    // Target HP bar rectangle (if available)
    const auto target_hp_bar = m_eyes.TargetHPBar();
    if (target_hp_bar.has_value()) {
        RECT r{target_hp_bar->x, target_hp_bar->y, target_hp_bar->x + target_hp_bar->width, target_hp_bar->y + target_hp_bar->height};
        m_overlay.drawRect(r, {255,0,255,220}, 2);
    }

    // My bars
    const auto my_bars = m_eyes.MyBars();
    if (my_bars.has_value()) {
        RECT r1{my_bars->hp_bar.x, my_bars->hp_bar.y, my_bars->hp_bar.x + my_bars->hp_bar.width, my_bars->hp_bar.y + my_bars->hp_bar.height};
        RECT r2{my_bars->mp_bar.x, my_bars->mp_bar.y, my_bars->mp_bar.x + my_bars->mp_bar.width, my_bars->mp_bar.y + my_bars->mp_bar.height};
        RECT r3{my_bars->cp_bar.x, my_bars->cp_bar.y, my_bars->cp_bar.x + my_bars->cp_bar.width, my_bars->cp_bar.y + my_bars->cp_bar.height};
        m_overlay.drawRect(r1, {255,0,0,220}, 2);
        m_overlay.drawRect(r2, {255,255,0,220}, 2);
        m_overlay.drawRect(r3, {0,255,255,220}, 2);
    }
}

void Runloop::ConfigureEyes()
{
    m_eyes.m_blind_spot_radius      = m_options.Int("--blind_spot_radius", m_eyes.m_blind_spot_radius);
    m_eyes.m_npc_tracking_distance  = m_options.Int("--npc_tracking_distance", m_eyes.m_npc_tracking_distance);

    // NPC detection
    m_eyes.m_npc_name_min_height        = m_options.Int("--npc_name_min_height", m_eyes.m_npc_name_min_height);
    m_eyes.m_npc_name_max_height        = m_options.Int("--npc_name_max_height", m_eyes.m_npc_name_max_height);
    m_eyes.m_npc_name_min_width         = m_options.Int("--npc_name_min_width", m_eyes.m_npc_name_min_width);
    m_eyes.m_npc_name_max_width         = m_options.Int("--npc_name_max_width", m_eyes.m_npc_name_max_width);
    m_eyes.m_npc_name_color_from_hsv    = ::VectorToScalar(m_options.IntVector("--npc_name_color_from_hsv"), m_eyes.m_npc_name_color_from_hsv);
    m_eyes.m_npc_name_color_to_hsv      = ::VectorToScalar(m_options.IntVector("--npc_name_color_to_hsv"), m_eyes.m_npc_name_color_to_hsv);
    m_eyes.m_npc_name_color_threshold   = m_options.Double("--npc_name_color_threshold", m_eyes.m_npc_name_color_threshold);
    m_eyes.m_npc_name_center_offset     = m_options.Int("--npc_name_center_offset", m_eyes.m_npc_name_center_offset);

    // selected target detection
    m_eyes.m_target_circle_area_height      = m_options.Int("--target_circle_area_height", m_eyes.m_target_circle_area_height);
    m_eyes.m_target_circle_area_width       = m_options.Int("--target_circle_area_width", m_eyes.m_target_circle_area_width);
    m_eyes.m_target_gray_circle_color_bgr   = ::VectorToScalar(m_options.IntVector("--target_gray_circle_color_bgr"), m_eyes.m_target_gray_circle_color_bgr);
    m_eyes.m_target_blue_circle_color_bgr   = ::VectorToScalar(m_options.IntVector("--target_blue_circle_color_bgr"), m_eyes.m_target_blue_circle_color_bgr);
    m_eyes.m_target_red_circle_color_bgr    = ::VectorToScalar(m_options.IntVector("--target_red_circle_color_bgr"), m_eyes.m_target_red_circle_color_bgr);

    // my HP/MP/CP bars detection
    m_eyes.m_my_bar_min_height      = m_options.Int("--my_bar_min_height", m_eyes.m_my_bar_min_height);
    m_eyes.m_my_bar_max_height      = m_options.Int("--my_bar_max_height", m_eyes.m_my_bar_max_height);
    m_eyes.m_my_bar_min_width       = m_options.Int("--my_bar_min_width", m_eyes.m_my_bar_min_width);
    m_eyes.m_my_bar_max_width       = m_options.Int("--my_bar_max_width", m_eyes.m_my_bar_max_width);
    m_eyes.m_my_hp_color_from_hsv   = ::VectorToScalar(m_options.IntVector("--my_hp_color_from_hsv"), m_eyes.m_my_hp_color_from_hsv);
    m_eyes.m_my_hp_color_to_hsv     = ::VectorToScalar(m_options.IntVector("--my_hp_color_to_hsv"), m_eyes.m_my_hp_color_to_hsv);
    m_eyes.m_my_mp_color_from_hsv   = ::VectorToScalar(m_options.IntVector("--my_mp_color_from_hsv"), m_eyes.m_my_mp_color_from_hsv);
    m_eyes.m_my_mp_color_to_hsv     = ::VectorToScalar(m_options.IntVector("--my_mp_color_to_hsv"), m_eyes.m_my_mp_color_to_hsv);
    m_eyes.m_my_cp_color_from_hsv   = ::VectorToScalar(m_options.IntVector("--my_cp_color_from_hsv"), m_eyes.m_my_cp_color_from_hsv);
    m_eyes.m_my_cp_color_to_hsv     = ::VectorToScalar(m_options.IntVector("--my_cp_color_to_hsv"), m_eyes.m_my_cp_color_to_hsv);

    // target HP bar detection
    m_eyes.m_target_hp_min_height       = m_options.Int("--target_hp_min_height", m_eyes.m_target_hp_min_height);
    m_eyes.m_target_hp_max_height       = m_options.Int("--target_hp_max_height", m_eyes.m_target_hp_max_height);
    m_eyes.m_target_hp_min_width        = m_options.Int("--target_hp_min_width", m_eyes.m_target_hp_min_width);
    m_eyes.m_target_hp_max_width        = m_options.Int("--target_hp_max_width", m_eyes.m_target_hp_max_width);
    m_eyes.m_target_hp_color_from_hsv   = ::VectorToScalar(m_options.IntVector("--target_hp_color_from_hsv"), m_eyes.m_target_hp_color_from_hsv);
    m_eyes.m_target_hp_color_to_hsv     = ::VectorToScalar(m_options.IntVector("--target_hp_color_to_hsv"), m_eyes.m_target_hp_color_to_hsv);
}

void Runloop::ConfigureHands()
{
    m_hands.m_attack_key        = ::StringToKeyboardKey(m_options.String("--attack_key"), m_hands.m_attack_key);
    m_hands.m_next_target_key   = ::StringToKeyboardKey(m_options.String("--next_target_key"), m_hands.m_next_target_key);
    m_hands.m_spoil_key         = ::StringToKeyboardKey(m_options.String("--spoil_key"), m_hands.m_spoil_key);
    m_hands.m_sweep_key         = ::StringToKeyboardKey(m_options.String("--sweep_key"), m_hands.m_sweep_key);
    m_hands.m_pick_up_key       = ::StringToKeyboardKey(m_options.String("--pick_up_key"), m_hands.m_pick_up_key);
    m_hands.m_restore_hp_key    = ::StringToKeyboardKey(m_options.String("--restore_hp_key"), m_hands.m_restore_hp_key);
    m_hands.m_restore_mp_key    = ::StringToKeyboardKey(m_options.String("--restore_mp_key"), m_hands.m_restore_mp_key);
    m_hands.m_restore_cp_key    = ::StringToKeyboardKey(m_options.String("--restore_cp_key"), m_hands.m_restore_cp_key);
}

void Runloop::ConfigureBrain()
{
    m_brain.m_search_attempts = m_options.Int("--search_attempts", m_brain.m_search_attempts);
}
