#include "gview/authoring.hpp"

#include <algorithm>
#include <fstream>
#include <unordered_set>

namespace gview {
namespace {

void collect_ids(const glayout::GraphNode& node, std::vector<std::string>& ids) {
    ids.push_back(node.id);
    for (const glayout::GraphNode& child : node.children)
        collect_ids(child, ids);
}

NodeSpec* find_spec(View& view, std::string_view id) {
    const auto found = std::find_if(view.nodes.begin(), view.nodes.end(),
                                    [&](const NodeSpec& node) { return node.layout_id == id; });
    return found == view.nodes.end() ? nullptr : &*found;
}

} // namespace

// Owns an editable view and authoring-only snapshot history outside the hot
// runtime.
void AuthoringSession::open(View view, std::filesystem::path source) {
    view_ = std::move(view);
    source_ = std::move(source);
    selection_ = view_.layout.root.id;
    undo_.clear();
    redo_.clear();
    dirty_ = false;
}

const View& AuthoringSession::view() const { return view_; }
View& AuthoringSession::view() { return view_; }
const std::filesystem::path& AuthoringSession::source() const { return source_; }
const std::string& AuthoringSession::selection() const { return selection_; }
void AuthoringSession::select(std::string id) { selection_ = std::move(id); }
bool AuthoringSession::dirty() const { return dirty_; }
bool AuthoringSession::can_undo() const { return !undo_.empty(); }
bool AuthoringSession::can_redo() const { return !redo_.empty(); }

void AuthoringSession::commit_snapshot(View before) {
    undo_.push_back(std::move(before));
    redo_.clear();
    dirty_ = true;
}

bool AuthoringSession::edit(const std::function<bool(View&)>& mutation) {
    const View before = view_;
    if (!mutation(view_)) return false;
    commit_snapshot(before);
    return true;
}

// Keeps layout and presentation identities synchronized during structural
// edits.
bool AuthoringSession::add(std::string_view parent, glayout::GraphNode layout,
                           NodeSpec presentation) {
    if (layout.id.empty() || presentation.layout_id != layout.id) return false;
    return edit([&](View& view) {
        if (!glayout::graph_add_child(view.layout, parent, std::move(layout))) return false;
        view.nodes.push_back(std::move(presentation));
        return true;
    });
}

bool AuthoringSession::remove(std::string_view id) {
    const glayout::GraphNode* node = glayout::find_graph_node(view_.layout, id);
    if (!node || node == &view_.layout.root) return false;
    std::vector<std::string> ids;
    collect_ids(*node, ids);
    const std::unordered_set<std::string> removed(ids.begin(), ids.end());
    const bool changed = edit([&](View& view) {
        if (!glayout::graph_remove_node(view.layout, id)) return false;
        std::erase_if(view.nodes,
                      [&](const NodeSpec& item) { return removed.contains(item.layout_id); });
        std::erase_if(view.focus_edges, [&](const FocusEdge& edge) {
            return removed.contains(edge.from) || removed.contains(edge.to);
        });
        return true;
    });
    if (changed) selection_ = view_.layout.root.id;
    return changed;
}

bool AuthoringSession::duplicate(std::string_view id, std::string new_id) {
    const glayout::GraphNode* source = glayout::find_graph_node(view_.layout, id);
    if (!source || new_id.empty()) return false;
    std::vector<std::string> originals;
    collect_ids(*source, originals);
    const std::string root_id(id);
    const bool changed = edit([&](View& view) {
        if (!glayout::graph_duplicate_node(view.layout, id, new_id)) return false;
        std::vector<NodeSpec> copies;
        for (const std::string& original : originals) {
            if (const NodeSpec* spec = find_spec(view, original)) {
                NodeSpec copy = *spec;
                copy.layout_id = original == root_id ? new_id : new_id + "/" + original;
                copies.push_back(std::move(copy));
            }
        }
        view.nodes.insert(view.nodes.end(), copies.begin(), copies.end());
        return true;
    });
    if (changed) selection_ = std::move(new_id);
    return changed;
}

bool AuthoringSession::reparent(std::string_view id, std::string_view parent) {
    return edit([&](View& view) { return glayout::graph_reparent_node(view.layout, id, parent); });
}

// Persists explicit directional intent independently from geometric fallback
// navigation.
bool AuthoringSession::connect(std::string from, NavAction action, std::string to) {
    return edit([&](View& view) {
        if (!find_spec(view, from) || !find_spec(view, to)) return false;
        const auto existing = std::find_if(
            view.focus_edges.begin(), view.focus_edges.end(),
            [&](const FocusEdge& edge) { return edge.from == from && edge.action == action; });
        if (existing != view.focus_edges.end()) {
            if (existing->to == to) return false;
            existing->to = std::move(to);
            return true;
        }
        view.focus_edges.push_back({std::move(from), action, std::move(to)});
        return true;
    });
}

bool AuthoringSession::disconnect(std::string_view from, NavAction action, std::string_view to) {
    return edit([&](View& view) {
        const std::size_t before = view.focus_edges.size();
        std::erase_if(view.focus_edges, [&](const FocusEdge& edge) {
            return edge.from == from && edge.action == action && edge.to == to;
        });
        return before != view.focus_edges.size();
    });
}

bool AuthoringSession::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(view_);
    view_ = std::move(undo_.back());
    undo_.pop_back();
    dirty_ = true;
    return true;
}

bool AuthoringSession::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(view_);
    view_ = std::move(redo_.back());
    redo_.pop_back();
    dirty_ = true;
    return true;
}

// Round-trips the same S-expression representation consumed by normal runtime
// loading.
bool AuthoringSession::save() { return !source_.empty() && save_as(source_); }

bool AuthoringSession::save_as(const std::filesystem::path& path) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const std::filesystem::path temporary = path.string() + ".gview-tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;
    output << write_views({view_});
    output.close();
    if (!output) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temporary, path, error);
    }
    if (error) return false;
    source_ = path;
    dirty_ = false;
    return true;
}

bool AuthoringSession::reload() {
    if (source_.empty()) return false;
    std::ifstream input(source_);
    if (!input) return false;
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    ParseResult parsed = parse_views(text);
    if (!parsed.ok || parsed.views.empty()) return false;
    open(std::move(parsed.views.front()), source_);
    return true;
}

std::string AuthoringSession::make_id(std::string_view prefix) {
    std::string candidate;
    do
        candidate = std::string(prefix) + "-" + std::to_string(next_id_++);
    while (glayout::find_graph_node(view_.layout, candidate));
    return candidate;
}

} // namespace gview
