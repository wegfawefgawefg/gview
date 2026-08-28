#pragma once

#include "gview/theme.hpp"
#include "gview/types.hpp"

#include <glayout/graph.hpp>

namespace gview {

struct FocusEdge {
    std::string from;
    NavAction action = NavAction::Down;
    std::string to;
};

struct FocusGroup {
    std::string id;
    std::string entry;
    std::string owner;
    bool contain = false;
    bool remember = true;
};

// Connects scopes without persisting one current collection item's identity.
struct FocusGroupEdge {
    std::string from;
    NavAction action = NavAction::Down;
    std::string to;

    bool operator==(const FocusGroupEdge&) const = default;
};

struct View {
    std::string id;
    std::string label;
    glayout::GraphLayout layout;
    std::vector<NodeSpec> nodes;
    std::vector<FocusGroup> focus_groups;
    std::vector<FocusEdge> focus_edges;
    std::vector<FocusGroupEdge> focus_group_edges;
    std::vector<Theme> themes;
    std::string active_theme;
};

struct ViewStore {
    std::vector<View> views;

    const View* find(std::string_view id) const;
    void add_or_replace(View view);
};

} // namespace gview
