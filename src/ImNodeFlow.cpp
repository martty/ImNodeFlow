#include "ImNodeFlow.h"

namespace ImFlow
{
    // -----------------------------------------------------------------------------------------------------------------
    // LINK

    void Link::draw()
    {
        auto* leftPin = reinterpret_cast<Pin*>(m_left);
        auto* rightPin = reinterpret_cast<Pin*>(m_right);
        ImVec2 start = leftPin->pos() + ImVec2(leftPin->size().x, leftPin->size().y / 2);
        ImVec2 end = rightPin->pos() + ImVec2(0, leftPin->size().y / 2);
        float thickness = 2.8f;
        if (ImProjectOnCubicBezier(ImGui::GetMousePos(), start, start + ImVec2(50, 0), end - ImVec2(50, 0), end).Distance < 2.5)
        {
            thickness = 3.5f;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                m_selected = true;
        }
        if (m_selected)
            smart_bezier(start, end, IM_COL32(80, 20, 255, 255), 4.0f);
        smart_bezier(start, end, IM_COL32(200, 200, 100, 255), thickness);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // BASE NODE

    void BaseNode::update(ImVec2& offset)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImGui::PushID(this);

        draw_list->ChannelsSetCurrent(1); // Foreground
        ImGui::SetCursorScreenPos(offset + m_pos);

        ImGui::BeginGroup();

        ImGui::BeginGroup();
        ImGui::Text(m_name.c_str());
        ImGui::Spacing();
        ImGui::EndGroup();
        float headerH = ImGui::GetItemRectSize().y;

        ImGui::BeginGroup();
        for(auto& p : m_ins)
        {
            p->pos(ImGui::GetCursorPos() + ImGui::GetWindowPos());
            p->update();
        }
        ImGui::EndGroup();
        ImGui::SameLine();

        ImGui::BeginGroup();
        draw();
        ImGui::EndGroup();
        ImGui::SameLine();

        ImGui::BeginGroup();
        for (auto& p : m_outs)
        {
            p->pos(ImGui::GetCursorPos() + ImGui::GetWindowPos());
            p->update();
        }
        ImGui::EndGroup();
        ImGui::SameLine();

        ImGui::EndGroup();

        m_size = ImGui::GetItemRectSize();
        ImVec2 headerSize = ImVec2(m_size.x + m_padding.x, headerH);

        if (ImGui::IsMouseHoveringRect(offset + m_pos - m_padding, offset + m_pos + headerSize) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_dragged = true;
            m_inf->draggingNode(true);
        }
        if(m_dragged)
        {
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                m_dragged = false;
                m_inf->draggingNode(false);
            }
            m_pos += ImGui::GetIO().MouseDelta;
        }

        draw_list->ChannelsSetCurrent(0); // Background
        draw_list->AddRectFilled(offset + m_pos - m_padding, offset + m_pos + m_size + m_padding, IM_COL32(60, 60, 60, 255), 4.0f);
        draw_list->AddRectFilled(offset + m_pos - m_padding, offset + m_pos + headerSize, IM_COL32(40, 40, 40, 255), 4.0f);
        draw_list->AddRect(offset + m_pos - m_padding, offset + m_pos + m_size + m_padding, IM_COL32(100, 100, 100, 255), 4.0f);

        ImGui::PopID();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // HANDLER

    void ImNodeFlow::createLink(uintptr_t left, uintptr_t right)
    {
        reinterpret_cast<Pin*>(right)->setLink(m_links.emplace_back(std::make_shared<Link>(left, right)));
    }

    void ImNodeFlow::update()
    {
        // Clearing looping stuff
        m_hovering = nullptr;

        // Create child canvas
        ImGui::Text("Hold middle mouse button to scroll (%.2f,%.2f)", m_scroll.x, m_scroll.y);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(60, 60, 70, 200));
        ImGui::BeginChild(m_name.c_str(), ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar(2); // WindowPadding
        ImGui::PopStyleColor();
        ImGui::PushItemWidth(120.0f);

        ImVec2 offset = ImGui::GetCursorScreenPos() + m_scroll;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Display grid
        ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
        float GRID_SZ = 64.0f;
        ImVec2 win_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_sz = ImGui::GetWindowSize();
        for (float x = fmodf(m_scroll.x, GRID_SZ); x < canvas_sz.x; x += 64)
            draw_list->AddLine(ImVec2(x, 0.0f) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, GRID_COLOR);
        for (float y = fmodf(m_scroll.y, GRID_SZ); y < canvas_sz.y; y += 64)
            draw_list->AddLine(ImVec2(0.0f, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, GRID_COLOR);

        // Update and draw nodes
        draw_list->ChannelsSplit(2);
        for (auto& node : m_nodes) { node->update(offset); }
        draw_list->ChannelsMerge();

        // Draw links
        for (auto& l : m_links) { l->draw(); }

        // Links drop-off
        if(m_dragOut && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if(!m_hovering)
            {
                m_droppedLinkCallback();
                goto drop_off_end;
            }
            if (m_dragOut->kind() == PinKind_Output && m_hovering->kind() == PinKind_Input) // OUT to IN
            {
                if ((void *)m_dragOut->parent() == (void *)m_hovering->parent())
                    goto drop_off_end;
                if (m_hovering->getLink().expired())
                {
                    createLink(m_dragOut->me(), m_hovering->me());
                }
                else
                {
                    int i = 0;
                    for (auto& l : m_links)
                    {
                        if(l->right() == m_hovering->me() && l->left() == m_dragOut->me()) // Same link --> Deletion
                        {
                            m_links.erase(m_links.begin() + i);
                            break;
                        }
                        if(l->right() == m_hovering->me()) // New link for same IN --> Swap
                        {
                            m_links.erase(m_links.begin() + i);
                            createLink(m_dragOut->me(), m_hovering->me());
                            break;
                        }
                        i++;
                    }
                }
            }
            if (m_dragOut->kind() == PinKind_Input && m_hovering->kind() == PinKind_Output) // IN to OUT
            {
                if ((void *)m_dragOut->parent() == (void *)m_hovering->parent())
                    goto drop_off_end;
                if (m_dragOut->getLink().expired())
                {
                    createLink(m_hovering->me(), m_dragOut->me());
                }
                else
                {
                    int i = 0;
                    for (auto& l : m_links)
                    {
                        if(l->right() == m_dragOut->me() && l->left() == m_hovering->me()) // Same link --> Deletion
                        {
                            m_links.erase(m_links.begin() + i);
                            break;
                        }
                        if(l->right() == m_dragOut->me()) // New link for same IN --> Swap
                        {
                            m_links.erase(m_links.begin() + i);
                            createLink(m_hovering->me(), m_dragOut->me());
                            break;
                        }
                        i++;
                    }
                }
            }
        }
        drop_off_end:

        // Links drag-out
        if (!m_draggingNode && m_hovering && !m_dragOut && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            m_dragOut = m_hovering;
        if (m_dragOut)
        {
            ImVec2 pinDot;
            if (m_dragOut->kind() == PinKind_Output)
                pinDot = m_dragOut->pos() + ImVec2(m_dragOut->size().x, m_dragOut->size().y / 2);
            else
                pinDot = m_dragOut->pos() + ImVec2(0, m_dragOut->size().y / 2);
            smart_bezier(pinDot, ImGui::GetMousePos(), IM_COL32(200, 200, 100, 255), 3.0f);

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                m_dragOut = nullptr;
        }

        //  Deselection
        if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            for (auto& l : m_links)
            {
                l->selected(false);
            }
        }

        // Deletion of selected stuff
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            std::vector<int> deletions;

            for (int i = 0; i < m_links.size(); i++)
                if (m_links[i]->selected())
                    deletions.emplace_back(i);
            for (int& i : deletions)
                m_links.erase(m_links.begin() + i);
            deletions.clear();

            // TODO: Do the same for Nodes
        }

        // Scrolling
        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
            m_scroll = m_scroll + ImGui::GetIO().MouseDelta;

        ImGui::EndChild();
    }
}
