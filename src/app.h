#pragma once
// Application state: wraps the C++ engine and manages IDE state.
// On Windows we can use the engine's C++ types directly (no ObjC bridge needed).

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "pho_graph.h"
#include "pho_eval.h"
#include "pho_serial.h"
#include "pho_prim.h"
#include "pho_bridge.h"
#include "pho_debug.h"
#include "pho_scene.h"
#include "pho_draw.h"
#include "pho_thread.h"
#include "pho_ide_render.h"
#include "pho_fuzzy.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>

// Visual layout for a node on the canvas
struct VisualNode {
    pho::NodeId id = 0;
    float x = 0, y = 0;
    float width = 160, height = 50;
    std::string label;
    std::string node_type; // "primitive","constant","input_bar","output_bar","method_call" etc.
    int num_inputs = 0;
    int num_outputs = 0;
    bool selected = false;
    std::string constant_value;
};

// Visual layout for a wire
struct VisualWire {
    pho::WireId id = 0;
    pho::NodeId source_node = 0;
    uint32_t source_pin = 0;
    pho::NodeId target_node = 0;
    uint32_t target_pin = 0;
    bool is_execution = false;
    bool selected = false;
};

// A graph view (nodes + wires for a single case)
struct GraphView {
    std::string method_name;
    int case_index = 0;
    std::vector<VisualNode> nodes;
    std::vector<VisualWire> wires;

    VisualNode* find_node(pho::NodeId id);
    const VisualNode* find_node(pho::NodeId id) const;
    void deselect_all();
    void select_node(pho::NodeId id);
    void select_wire(pho::WireId id);
};

// Method info for the class browser
struct MethodInfo {
    std::string name;
    int num_inputs = 0;
    int num_outputs = 0;
    int num_cases = 1;
    std::string owner_class; // empty for universal methods
};

// Class info for the class browser
struct ClassInfo {
    std::string name;
    std::string parent;
    std::vector<std::string> attributes;
    std::vector<MethodInfo> methods;
};

// Section info
struct SectionInfo {
    std::string name;
    std::vector<MethodInfo> methods;
    std::vector<ClassInfo> classes;
};

class PhographApp {
public:
    PhographApp();
    ~PhographApp();

    // Project management
    bool new_project();
    bool load_project(const std::string& json);
    bool load_project_file(const std::wstring& path);
    bool save_project_file(const std::wstring& path);
    std::string get_project_json() const { return project_json_; }

    // Method selection
    void select_method(const std::string& name, int case_index = 0);
    void next_case();
    void prev_case();

    // Execution
    std::string run_method(const std::string& name);
    void stop_execution();

    // Debug
    void debug_run(const std::string& name);
    void debug_continue();
    void debug_step_over();
    void debug_step_into();
    void debug_stop();
    void toggle_breakpoint(pho::NodeId node_id);

    // State queries
    const std::string& project_name() const { return project_name_; }
    const std::vector<SectionInfo>& sections() const { return sections_; }
    GraphView* current_graph() { return current_graph_.get(); }
    const GraphView* current_graph() const { return current_graph_.get(); }
    const std::string& selected_method() const { return selected_method_; }
    int selected_case_index() const { return selected_case_; }
    int case_count() const;
    bool is_running() const { return is_running_; }
    bool is_debugging() const { return is_debugging_; }

    // Console
    const std::string& console_output() const { return console_; }
    void clear_console() { console_.clear(); }
    void append_console(const std::string& text) { console_ += text; }

    // Status
    const std::string& status_message() const { return status_; }
    void set_status(const std::string& msg) { status_ = msg; }

    // Primitive names for fuzzy finder
    std::vector<std::string> all_primitive_names() const;

    // Find method info
    const MethodInfo* find_method_info(const std::string& name) const;

    // Canvas access (for pixel buffer rendering)
    const uint8_t* pixel_buffer(int32_t* w, int32_t* h) const;
    void tick(double dt);

private:
    void rebuild_sections();
    void build_graph_view(const pho::Method& method, int case_index);

    PhoEngineRef engine_ = nullptr;
    pho::Project project_;
    std::string project_json_;
    std::string project_name_;
    std::wstring project_path_;

    std::vector<SectionInfo> sections_;
    std::unique_ptr<GraphView> current_graph_;
    std::string selected_method_;
    int selected_case_ = 0;

    std::string console_;
    std::string status_ = "Ready";
    bool is_running_ = false;
    bool is_debugging_ = false;
    bool prims_initialized_ = false;
};
