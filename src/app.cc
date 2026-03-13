#include "app.h"
#include <cstdio>
#include <algorithm>

// ---------------------------------------------------------------------------
// GraphView
// ---------------------------------------------------------------------------

VisualNode* GraphView::find_node(pho::NodeId id) {
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

const VisualNode* GraphView::find_node(pho::NodeId id) const {
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

void GraphView::deselect_all() {
    for (auto& n : nodes) n.selected = false;
    for (auto& w : wires) w.selected = false;
}

void GraphView::select_node(pho::NodeId id) {
    deselect_all();
    if (auto* n = find_node(id)) n->selected = true;
}

void GraphView::select_wire(pho::WireId id) {
    deselect_all();
    for (auto& w : wires) if (w.id == id) { w.selected = true; break; }
}

// ---------------------------------------------------------------------------
// PhographApp
// ---------------------------------------------------------------------------

PhographApp::PhographApp() {
    if (!prims_initialized_) {
        pho_engine_init_prims();
        prims_initialized_ = true;
    }
    engine_ = pho_engine_create();
}

PhographApp::~PhographApp() {
    if (engine_) {
        pho_engine_destroy(engine_);
        engine_ = nullptr;
    }
}

bool PhographApp::new_project() {
    std::string json = R"({
  "name": "Untitled",
  "sections": [
    {
      "name": "Main",
      "methods": []
    }
  ]
})";
    return load_project(json);
}

bool PhographApp::load_project(const std::string& json) {
    project_json_ = json;

    // Load into engine
    int res = pho_engine_load_json(engine_, json.c_str(), json.size());
    if (res != 0) {
        const char* err = pho_engine_last_error(engine_);
        console_ += "Error loading project: ";
        console_ += (err ? err : "unknown error");
        console_ += "\n";
        status_ = "Load failed";
        return false;
    }

    // Parse into our project model
    std::string err;
    if (!pho::load_project_from_json(json, project_, err)) {
        console_ += "Error parsing project: " + err + "\n";
        status_ = "Parse failed";
        return false;
    }

    project_name_ = project_.name;
    rebuild_sections();

    // Auto-select first method
    selected_method_.clear();
    selected_case_ = 0;
    current_graph_.reset();

    for (auto& sec : sections_) {
        if (!sec.methods.empty()) {
            select_method(sec.methods[0].name, 0);
            break;
        }
        for (auto& cls : sec.classes) {
            if (!cls.methods.empty()) {
                select_method(cls.methods[0].name, 0);
                goto found;
            }
        }
    }
found:
    status_ = "Project loaded";
    return true;
}

bool PhographApp::load_project_file(const std::wstring& path) {
    // Read file
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) {
        console_ += "Cannot open file\n";
        status_ = "Open failed";
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string json(sz, '\0');
    fread(&json[0], 1, sz, f);
    fclose(f);

    project_path_ = path;
    if (load_project(json)) {
        // Extract name from filename
        size_t slash = path.find_last_of(L"\\/");
        size_t dot = path.find_last_of(L'.');
        if (slash != std::wstring::npos && dot != std::wstring::npos && dot > slash) {
            std::wstring wname = path.substr(slash + 1, dot - slash - 1);
            int needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), nullptr, 0, nullptr, nullptr);
            project_name_.resize(needed);
            WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), &project_name_[0], needed, nullptr, nullptr);
        }
        status_ = "Opened " + project_name_;
        return true;
    }
    return false;
}

bool PhographApp::save_project_file(const std::wstring& path) {
    if (project_json_.empty()) {
        console_ += "Nothing to save\n";
        status_ = "Save failed";
        return false;
    }
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) {
        console_ += "Cannot write file\n";
        status_ = "Save failed";
        return false;
    }
    fwrite(project_json_.data(), 1, project_json_.size(), f);
    fclose(f);
    project_path_ = path;
    status_ = "Saved";
    return true;
}

void PhographApp::rebuild_sections() {
    sections_.clear();
    for (auto& sec : project_.sections) {
        SectionInfo si;
        si.name = sec.name;
        for (auto& m : sec.methods) {
            MethodInfo mi;
            mi.name = m.name;
            mi.num_inputs = m.num_inputs;
            mi.num_outputs = m.num_outputs;
            mi.num_cases = (int)m.cases.size();
            si.methods.push_back(mi);
        }
        for (auto& cls : sec.classes) {
            ClassInfo ci;
            ci.name = cls.name;
            ci.parent = cls.parent_name;
            for (auto& a : cls.attributes)
                ci.attributes.push_back(a.name);
            for (auto& m : cls.methods) {
                MethodInfo mi;
                mi.name = m.name;
                mi.num_inputs = m.num_inputs;
                mi.num_outputs = m.num_outputs;
                mi.num_cases = (int)m.cases.size();
                mi.owner_class = cls.name;
                ci.methods.push_back(mi);
            }
            si.classes.push_back(ci);
        }
        sections_.push_back(si);
    }
}

static const char* node_type_str(pho::NodeType t) {
    switch (t) {
        case pho::NodeType::Primitive:         return "primitive";
        case pho::NodeType::MethodCall:        return "method_call";
        case pho::NodeType::Constant:          return "constant";
        case pho::NodeType::InputBar:          return "input_bar";
        case pho::NodeType::OutputBar:         return "output_bar";
        case pho::NodeType::Get:               return "get";
        case pho::NodeType::Set:               return "set";
        case pho::NodeType::InstanceGenerator: return "instance_generator";
        case pho::NodeType::Persistent:        return "persistent";
        case pho::NodeType::LocalMethod:       return "local_method";
        case pho::NodeType::Evaluation:        return "evaluation";
        case pho::NodeType::Inject:            return "inject";
        default:                               return "unknown";
    }
}

void PhographApp::build_graph_view(const pho::Method& method, int case_index) {
    current_graph_ = std::make_unique<GraphView>();
    current_graph_->method_name = method.name;
    current_graph_->case_index = case_index;

    if (case_index < 0 || case_index >= (int)method.cases.size())
        return;

    const pho::Case& c = method.cases[case_index];

    // Layout nodes in a grid-like arrangement
    float start_x = 100, start_y = 80;
    float col_spacing = 200, row_spacing = 100;
    int col = 0;

    for (auto& node : c.nodes) {
        VisualNode vn;
        vn.id = node.id;
        vn.label = node.name;
        vn.node_type = node_type_str(node.type);
        vn.num_inputs = node.num_inputs;
        vn.num_outputs = node.num_outputs;

        if (node.type == pho::NodeType::Constant) {
            if (node.constant_value.is_integer())
                vn.constant_value = std::to_string(node.constant_value.as_integer());
            else if (node.constant_value.is_real())
                vn.constant_value = std::to_string(node.constant_value.as_real());
            else if (node.constant_value.is_string())
                vn.constant_value = node.constant_value.as_string()->str();
            else if (node.constant_value.is_boolean())
                vn.constant_value = node.constant_value.as_boolean() ? "true" : "false";
            if (vn.label.empty()) vn.label = vn.constant_value;
        }

        // Simple auto-layout: input bar at top, output bar at bottom, rest in middle
        if (node.type == pho::NodeType::InputBar) {
            vn.x = start_x + 200;
            vn.y = 20;
        } else if (node.type == pho::NodeType::OutputBar) {
            vn.x = start_x + 200;
            vn.y = start_y + row_spacing * (int)c.nodes.size();
        } else {
            vn.x = start_x + (col % 3) * col_spacing;
            vn.y = start_y + (col / 3) * row_spacing;
            col++;
        }

        // Compute size
        float label_w = (float)vn.label.size() * 9.5f + 40.0f;
        vn.width = (std::max)(160.0f, label_w);
        int pin_count = (std::max)(vn.num_inputs, vn.num_outputs);
        float pin_h = 12.0f; // pin radius area at top/bottom
        float header_h = 32.0f;
        vn.height = (std::max)(50.0f, header_h + pin_h * 2);

        current_graph_->nodes.push_back(vn);
    }

    for (auto& wire : c.wires) {
        VisualWire vw;
        vw.id = wire.id;
        vw.source_node = wire.source.node_id;
        vw.source_pin = wire.source.index;
        vw.target_node = wire.target.node_id;
        vw.target_pin = wire.target.index;
        vw.is_execution = wire.is_execution;
        current_graph_->wires.push_back(vw);
    }
}

void PhographApp::select_method(const std::string& name, int case_index) {
    selected_method_ = name;
    selected_case_ = case_index;

    // Find method in project
    const pho::Method* method = project_.find_method(name);
    if (!method) {
        // Check class methods
        for (auto& sec : project_.sections) {
            for (auto& cls : sec.classes) {
                if (auto* m = cls.find_method(name)) {
                    method = m;
                    break;
                }
            }
            if (method) break;
        }
    }

    if (method) {
        build_graph_view(*method, case_index);
    } else {
        current_graph_ = std::make_unique<GraphView>();
        current_graph_->method_name = name;
    }
}

int PhographApp::case_count() const {
    const pho::Method* method = project_.find_method(selected_method_);
    if (!method) {
        for (auto& sec : project_.sections)
            for (auto& cls : sec.classes)
                if (auto* m = cls.find_method(selected_method_))
                    return (int)m->cases.size();
        return 1;
    }
    return (int)method->cases.size();
}

void PhographApp::next_case() {
    if (selected_case_ + 1 < case_count())
        select_method(selected_method_, selected_case_ + 1);
}

void PhographApp::prev_case() {
    if (selected_case_ > 0)
        select_method(selected_method_, selected_case_ - 1);
}

std::string PhographApp::run_method(const std::string& name) {
    is_running_ = true;
    status_ = "Running " + name + "...";

    const char* result = pho_engine_call_method(engine_, name.c_str(), nullptr, 0);
    std::string result_str;
    if (result) {
        result_str = result;
        pho_engine_free_string(result);
    }

    // Get console output from engine
    const char* con = pho_engine_get_console(engine_);
    if (con && con[0]) {
        console_ += con;
        pho_engine_clear_console(engine_);
    }

    if (!result_str.empty()) {
        console_ += "> " + name + ": " + result_str + "\n";
    }

    is_running_ = false;
    status_ = "Done";
    return result_str;
}

void PhographApp::stop_execution() {
    if (is_debugging_) debug_stop();
    is_running_ = false;
    status_ = "Stopped";
}

void PhographApp::debug_run(const std::string& name) {
    is_debugging_ = true;
    is_running_ = true;
    status_ = "Debugging " + name + "...";
    console_ += "> Debug: " + name + "\n";
    pho_engine_debug_run(engine_, name.c_str());
}

void PhographApp::debug_continue() {
    pho_engine_debug_continue(engine_);
}

void PhographApp::debug_step_over() {
    pho_engine_debug_step_over(engine_);
}

void PhographApp::debug_step_into() {
    pho_engine_debug_step_into(engine_);
}

void PhographApp::debug_stop() {
    pho_engine_debug_stop(engine_);
    is_debugging_ = false;
    is_running_ = false;
    status_ = "Stopped";
}

void PhographApp::toggle_breakpoint(pho::NodeId node_id) {
    // Simplified: always add breakpoint (toggle would need tracking)
    pho_engine_debug_add_breakpoint(engine_, node_id,
                                     selected_method_.c_str(), selected_case_);
}

std::vector<std::string> PhographApp::all_primitive_names() const {
    std::vector<std::string> names = {
        "+", "-", "*", "/", "=", "<", ">", "<=", ">=", "!=",
        "and", "or", "not", "if",
        "concat", "length", "to-string", "split", "trim", "replace",
        "get-nth", "append", "sort", "empty?", "reverse",
        "dict-create", "dict-get", "dict-set",
        "log", "inspect",
        "mod", "abs", "round", "floor", "ceil",
        "min", "max", "sqrt", "sin", "cos", "tan",
        "list", "range", "map", "filter", "reduce",
        "shape-rect", "shape-oval", "shape-group",
        "shape-set-fill", "shape-add-child",
        "create-canvas", "canvas-render",
        "print", "to-integer", "to-real",
        "error", "try", "typeof",
    };

    // Add class-derived names
    for (auto& sec : sections_) {
        for (auto& cls : sec.classes) {
            names.push_back("new " + cls.name);
            for (auto& attr : cls.attributes) {
                names.push_back("get " + attr);
                names.push_back("set " + attr);
            }
            for (auto& m : cls.methods) {
                names.push_back(cls.name + "/" + m.name);
            }
        }
        for (auto& m : sec.methods) {
            if (std::find(names.begin(), names.end(), m.name) == names.end())
                names.push_back(m.name);
        }
    }
    return names;
}

const MethodInfo* PhographApp::find_method_info(const std::string& name) const {
    for (auto& sec : sections_) {
        for (auto& m : sec.methods)
            if (m.name == name) return &m;
        for (auto& cls : sec.classes)
            for (auto& m : cls.methods)
                if (m.name == name) return &m;
    }
    return nullptr;
}

const uint8_t* PhographApp::pixel_buffer(int32_t* w, int32_t* h) const {
    return pho_engine_pixel_buffer(engine_, w, h);
}

void PhographApp::tick(double dt) {
    pho_engine_tick(engine_, dt);
}
