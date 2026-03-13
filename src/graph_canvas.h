#pragma once
// Graph canvas: Direct2D rendering of the dataflow graph.
// Handles node/wire drawing, selection, pan/zoom, and mouse interaction.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <cstdint>
#include <string>

#include "app.h"

class GraphCanvas {
public:
    GraphCanvas();
    ~GraphCanvas();

    // Window management
    bool create(HWND parent, int x, int y, int w, int h, HINSTANCE hInst);
    void resize(int w, int h);
    HWND hwnd() const { return hwnd_; }

    // Set the app to read graph state from
    void set_app(PhographApp* app) { app_ = app; }

    // Trigger repaint
    void invalidate();

    // Pan/zoom
    void zoom_in();
    void zoom_out();
    void fit_to_window();

    // Static window procedure
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

private:
    // Direct2D resources
    bool create_device_resources();
    void discard_device_resources();

    // Rendering
    void on_paint();
    void draw_grid();
    void draw_node(const VisualNode& node);
    void draw_wire(const VisualWire& wire);
    void draw_method_title();

    // Pin positions (in graph coordinates)
    D2D1_POINT_2F input_pin_pos(const VisualNode& node, int pin_index) const;
    D2D1_POINT_2F output_pin_pos(const VisualNode& node, int pin_index) const;

    // Coordinate conversion
    D2D1_POINT_2F screen_to_graph(POINT screen) const;
    D2D1_POINT_2F graph_to_screen(float gx, float gy) const;

    // Mouse handling
    void on_lbutton_down(int x, int y, WPARAM flags);
    void on_lbutton_up(int x, int y);
    void on_mouse_move(int x, int y, WPARAM flags);
    void on_mouse_wheel(int x, int y, int delta);

    // Hit testing
    VisualNode* hit_test_node(float gx, float gy);
    int hit_test_output_pin(const VisualNode& node, float gx, float gy);
    int hit_test_input_pin(const VisualNode& node, float gx, float gy);

    // Colors for node types
    struct NodeColors {
        D2D1_COLOR_F header, header_text, body, border;
    };
    NodeColors node_colors(const std::string& type) const;

    HWND hwnd_ = nullptr;
    PhographApp* app_ = nullptr;

    // Direct2D
    ID2D1Factory* d2d_factory_ = nullptr;
    ID2D1HwndRenderTarget* render_target_ = nullptr;
    IDWriteFactory* dwrite_factory_ = nullptr;
    IDWriteTextFormat* text_format_ = nullptr;
    IDWriteTextFormat* small_text_format_ = nullptr;
    IDWriteTextFormat* title_text_format_ = nullptr;

    // Pan/zoom state
    float pan_x_ = 0, pan_y_ = 0;
    float zoom_ = 1.0f;

    // Mouse interaction state
    bool is_panning_ = false;
    bool is_dragging_node_ = false;
    bool is_dragging_wire_ = false;
    POINT drag_start_ = {};
    float pan_start_x_ = 0, pan_start_y_ = 0;
    pho::NodeId dragged_node_id_ = 0;
    float drag_node_offset_x_ = 0, drag_node_offset_y_ = 0;

    // Wire drag state
    pho::NodeId wire_source_node_ = 0;
    uint32_t wire_source_pin_ = 0;
    float wire_end_x_ = 0, wire_end_y_ = 0;

    int width_ = 800, height_ = 600;
};
