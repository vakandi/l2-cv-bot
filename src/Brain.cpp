#include "Brain.h"

#include <iostream>

#include "Utils.h"

void Brain::Init()
{
    m_hands.Delay(500);
    m_hands.ResetUI();
    m_hands.ResetCamera();
    m_hands.Send(500);
}

void Brain::Process()
{
    m_npcs = m_eyes.DetectNPCs();
    m_far_npcs = m_eyes.DetectFarNPCs();
    m_me = m_eyes.DetectMe();
    m_target = m_eyes.DetectTarget();
    
    // Debug: Show detection counts every 10 frames
    static int frame_count = 0;
    if (++frame_count % 10 == 0) {
        std::cout << "Detected: " << m_npcs.size() << " NPCs, " << m_far_npcs.size() << " far NPCs";
        if (m_me.has_value()) {
            std::cout << ", HP: " << m_me.value().hp << "%, MP: " << m_me.value().mp << "%, CP: " << m_me.value().cp << "%";
        }
        if (m_target.has_value()) {
            std::cout << ", Target HP: " << m_target.value().hp << "%";
        }
        std::cout << std::endl;
    }

    if (m_me.has_value()) {
        const auto me = m_me.value();

        // Emergency flee if HP is critically low
        if (me.hp < m_flee_hp_threshold && !LOCKED(2000)) {
            std::cout << "CRITICAL HP (" << me.hp << "%) - FLEEING!" << std::endl;
            m_hands.CancelTarget();  // Cancel current target
            m_hands.Send(500);
            m_state = State::Flee;
            return;  // Skip normal processing to flee immediately
        }

        // Normal HP restoration
        if (me.hp < m_restore_hp_threshold && !LOCKED(1000)) {
            std::cout << "Restore HP (" << me.hp << "%)" << std::endl;
            m_hands.RestoreHP();
            m_hands.Send();
        }

        if (me.mp < 70 && !LOCKED(1000)) {
            std::cout << "Restore MP (" << me.mp << "%)" << std::endl;
            m_hands.RestoreMP();
            m_hands.Send();
        }

        if (me.cp < 90 && !LOCKED(1000)) {
            std::cout << "Restore CP (" << me.cp << "%)" << std::endl;
            m_hands.RestoreCP();
            m_hands.Send();
        }
    }

    const auto target = m_target.value_or(::Eyes::Target{});

    // Only attack if attack mode is enabled and we have a valid target
    if (target.hp > 0 && m_attack_mode_enabled) {
        m_state = State::Attack;
    }

    if (!m_hands.IsReady()) {
        return;
    }

    if (m_state == State::NextTarget) {
        std::cout << "Next target" << std::endl;
        m_hands.NextTarget();
        m_hands.Send(500);
        m_state = State::NearSearch;
    } else if (m_state == State::NearSearch) {
        const auto npc = UnselectedNPC();

        if (npc.has_value()) {
            std::cout << "Near search: Found NPC" << std::endl;
            IgnoreNPC(npc.value().Id());
            m_hands.MoveMouseTo({npc.value().center.x, npc.value().center.y});
            m_hands.Send(500);
            m_previous_state = m_state;
            m_state = State::Check;
        } else if (m_search_attempt < m_search_attempts) {
            ++m_search_attempt;
            std::cout << "Near search: Look around attempt: " << m_search_attempt << std::endl;
            m_hands.LookAround();
            m_hands.NextTarget();
            m_hands.Send(500);
            ClearIgnoredNPCs();
        } else if (!LOCKED(2000)) {
            m_state = State::FarSearch;
            m_search_attempt = 0;
        }
    } else if (m_state == State::FarSearch) {
        const auto npc = FarNPC();

        if (npc.has_value()) {
            std::cout << "Far search: Found NPC" << std::endl;
            IgnoreNPC(npc.value().Id());
            m_hands.MoveMouseTo({npc.value().center.x, npc.value().center.y});
            m_hands.Send(500);
            m_previous_state = m_state;
            m_state = State::Check;
        } else if (m_search_attempt < m_search_attempts) {
            ++m_search_attempt;
            std::cout << "Far search: Look around attempt: " << m_search_attempt << std::endl;
            m_hands.LookAround();
            m_hands.NextTarget();
            m_hands.Send(2000);
            ClearIgnoredNPCs();
        } else {
            m_state = State::NextTarget;
            m_search_attempt = 0;
        }
    } else if (m_state == State::Check) {
        std::cout << "Check NPC" << std::endl;
        const auto npc = HoveredNPC();

        if (npc.has_value()) {
            m_hands.SelectTarget();
            m_hands.Send(500);
        }

        m_state = m_previous_state;
    } else if (m_state == State::Attack) {
        if (target.hp > 0) {
            if (m_first_attack) {
                std::cout << "Attack NPC (HP: " << target.hp << "%)" << std::endl;
                m_first_attack = false;
                m_search_attempt = 0;
                ClearIgnoredNPCs();
                m_hands.Spoil();
                m_hands.Attack();
                m_hands.Delay(500);
                m_hands.ResetCamera();
                m_hands.Send();
            } else {
                m_hands.Attack();
                m_hands.Send(250);
            }
        } else if (!LOCKED(1000)) {
            std::cout << "Target defeated, looking for next target" << std::endl;
            m_first_attack = true;
            const auto npc = SelectedNPC();

            if (npc.has_value()) {
                std::cout << "Go to NPC" << std::endl;
                m_hands.GoTo({npc.value().center.x, npc.value().center.y});
                m_hands.Send(4000); // TODO: moving of the selected target can be detected
            }

            m_state = State::PickUp;
        }
    } else if (m_state == State::PickUp) {
        std::cout << "Pick up loot" << std::endl;
        m_hands.Sweep();
        m_hands.Delay(500);
        m_hands.PickUp();
        m_hands.Send();
        m_state = State::NextTarget;
    } else if (m_state == State::Flee) {
        std::cout << "FLEEING - Moving away from danger!" << std::endl;
        
        // Move character away from current position (simple flee strategy)
        const auto center = m_hands.WindowCenter();
        m_hands.MoveMouseTo({center.x - 100, center.y - 100});  // Move northwest
        m_hands.Send(1000);
        m_hands.LeftMouseButtonClick();  // Click to move
        m_hands.Send(3000);  // Wait while moving
        
        // Try to restore HP while fleeing
        if (m_me.has_value() && m_me.value().hp < m_restore_hp_threshold) {
            m_hands.RestoreHP();
            m_hands.Send(1000);
        }
        
        // Return to normal operation after fleeing
        m_state = State::NextTarget;
        m_first_attack = true;  // Reset attack state
    }
}

std::optional<::Eyes::NPC> Brain::UnselectedNPC() const
{
    const auto npcs = FilteredNPCs();

    for (const auto &npc : npcs) {
        if (!npc.Selected()) {
            return npc;
        }
    }

    return {};
}

std::optional<::Eyes::NPC> Brain::SelectedNPC() const
{
    const auto npcs = FilteredNPCs();

    for (const auto &npc : npcs) {
        if (npc.Selected()) {
            return npc;
        }
    }

    return {};
}

std::optional<::Eyes::NPC> Brain::HoveredNPC() const
{
    for (const auto &npc : m_npcs) {
        if (npc.Hovered()) {
            return npc;
        }
    }

    return {};
}

std::optional<::Eyes::FarNPC> Brain::FarNPC() const
{
    for (const auto &npc : m_far_npcs) {
        if (m_ignored_npc_ids.find(npc.Id()) == m_ignored_npc_ids.end()) {
            return npc;
        }
    }

    return {};
}

std::vector<::Eyes::NPC> Brain::FilteredNPCs() const
{
    std::vector<::Eyes::NPC> npcs;

    for (const auto &npc : m_npcs) {
        if (m_ignored_npc_ids.find(npc.Id()) == m_ignored_npc_ids.end()) {
            npcs.push_back(npc);
        }
    }

    return npcs;
}
