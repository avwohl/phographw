#include "graph_canvas.h"
#include <windowsx.h>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

static const wchar_t* CANVAS_CLASS = L"PhographCanvas";
static const float PIN_RADIUS = 6.0f;
static const float PIN_HIT_RADIUS = 14.0f;
static const float GRID_SIZE = 20.0f;
static const float NODE_CORNER_RADIUS = 4.0f;
static const float HEADER_HEIGHT = 32.0f;

// Helper: create D2D1 color
static D2D1_COLOR_F color(float r, float g, float b, float a = 1.0f) {
    return D2D1::ColorF(r, g, b, a);
}

GraphCanvas::GraphCanvas() {}

GraphCanvas::~GraphCanvas() {
    discard_device_resources();
    if (d2d_factory_) { d2d_factory_->Release(); d2d_factory_ = nullptr; }
    if (dwrite_factory_) { dwrite_factory_->Release(); dwrite_factory_ = nullptr; }
    if (text_format_) { text_format_->Release(); text_format_ = nullptr; }
    if (small_text_format_) { small_text_format_->Release(); small_text_format_ = nullptr; }
    if (title_text_format_) { title_text_format_->Release(); title_text_format_ = nullptr; }
}

bool GraphCanvas::create(HWND parent, int x, int y, int w, int h, HINSTANCE hInst) {
    // Register class (once)
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = GraphCanvas::WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = CANVAS_CLASS;
        RegisterClassExW(&wc);
        registered = true;
    }

    hwnd_ = CreateWindowExW(0, CANVAS_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                            x, y, w, h, parent, nullptr, hInst, this);
    if (!hwnd_) return false;
    width_ = w;
    height_ = h;

    // Create D2D factory
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory_);

    // Create DirectWrite factory
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                        __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(&dwrite_factory_));

    // Create text formats
    if (dwrite_factory_) {
        dwrite_factory_->CreateTextFormat(L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-US", &text_format_);
        dwrite_factory_->CreateTextFormat(L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-US", &small_text_format_);
        dwrite_factory_->CreateTextFormat(L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-US", &title_text_format_);

        if (text_format_) {
            text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (small_text_format_) {
            small_text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            small_text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    return true;
}

void GraphCanvas::resize(int w, int h) {
    width_ = w;
    height_ = h;
    if (render_target_) {
        render_target_->Resize(D2D1::SizeU(w, h));
    }
    invalidate();
}

void GraphCanvas::invalidate() {
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

bool GraphCanvas::create_device_resources() {
    if (render_target_) return true;
    if (!d2d_factory_ || !hwnd_) return false;

    RECT rc;
    GetClientRect(hwnd_, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = d2d_factory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, size),
        &render_target_);

    return SUCCEEDED(hr);
}

void GraphCanvas::discard_device_resources() {
    if (render_target_) { render_target_->Release(); render_target_ = nullptr; }
}

// -----------------------------------------------------------------------
// Coordinate conversion
// -----------------------------------------------------------------------

D2D1_POINT_2F GraphCanvas::screen_to_graph(POINT screen) const {
    return D2D1::Point2F(
        (screen.x - pan_x_) / zoom_,
        (screen.y - pan_y_) / zoom_
    );
}

D2D1_POINT_2F GraphCanvas::graph_to_screen(float gx, float gy) const {
    return D2D1::Point2F(gx * zoom_ + pan_x_, gy * zoom_ + pan_y_);
}

// -----------------------------------------------------------------------
// Pin positions
// -----------------------------------------------------------------------

D2D1_POINT_2F GraphCanvas::input_pin_pos(const VisualNode& node, int pin_index) const {
    int count = node.num_inputs;
    float px = node.x + (float)(pin_index + 1) * node.width / (float)(count + 1);
    float py = node.y;
    return D2D1::Point2F(px, py);
}

D2D1_POINT_2F GraphCanvas::output_pin_pos(const VisualNode& node, int pin_index) const {
    int count = node.num_outputs;
    float px = node.x + (float)(pin_index + 1) * node.width / (float)(count + 1);
    float py = node.y + node.height;
    return D2D1::Point2F(px, py);
}

// -----------------------------------------------------------------------
// Node colors
// -----------------------------------------------------------------------

GraphCanvas::NodeColors GraphCanvas::node_colors(const std::string& type) const {
    if (type == "input_bar")
        return { color(0.2f,0.55f,0.3f), color(1,1,1), color(0.85f,0.95f,0.87f), color(0.2f,0.55f,0.3f,0.6f) };
    if (type == "output_bar")
        return { color(0.7f,0.2f,0.2f), color(1,1,1), color(0.97f,0.88f,0.88f), color(0.7f,0.2f,0.2f,0.6f) };
    if (type == "constant")
        return { color(0.5f,0.45f,0.2f), color(1,1,1), color(0.98f,0.96f,0.88f), color(0.5f,0.45f,0.2f,0.6f) };
    if (type == "primitive")
        return { color(0.25f,0.35f,0.6f), color(1,1,1), color(0.92f,0.94f,0.98f), color(0.25f,0.35f,0.6f,0.5f) };
    if (type == "method_call")
        return { color(0.4f,0.25f,0.55f), color(1,1,1), color(0.95f,0.92f,0.98f), color(0.4f,0.25f,0.55f,0.5f) };
    if (type == "instance_generator")
        return { color(0.8f,0.5f,0.0f), color(1,1,1), color(0.98f,0.94f,0.85f), color(0.8f,0.5f,0.0f,0.5f) };
    if (type == "get")
        return { color(0.2f,0.5f,0.45f), color(1,1,1), color(0.88f,0.96f,0.94f), color(0.2f,0.5f,0.45f,0.5f) };
    if (type == "set")
        return { color(0.55f,0.3f,0.2f), color(1,1,1), color(0.97f,0.92f,0.88f), color(0.55f,0.3f,0.2f,0.5f) };
    // Default
    return { color(0.4f,0.4f,0.4f), color(1,1,1), color(0.95f,0.95f,0.95f), color(0.4f,0.4f,0.4f,0.5f) };
}

// -----------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------

void GraphCanvas::on_paint() {
    if (!create_device_resources()) return;

    render_target_->BeginDraw();

    // Background
    render_target_->Clear(color(0.98f, 0.98f, 0.98f));

    // Apply transform for pan/zoom
    D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(zoom_, zoom_) *
                                   D2D1::Matrix3x2F::Translation(pan_x_, pan_y_);
    render_target_->SetTransform(transform);

    draw_grid();

    if (app_ && app_->current_graph()) {
        auto* graph = app_->current_graph();

        // Draw wires first (behind nodes)
        for (auto& wire : graph->wires) {
            draw_wire(wire);
        }

        // Draw wire being dragged
        if (is_dragging_wire_) {
            auto* src_node = graph->find_node(wire_source_node_);
            if (src_node) {
                auto start = output_pin_pos(*src_node, wire_source_pin_);
                ID2D1SolidColorBrush* brush = nullptr;
                render_target_->CreateSolidColorBrush(color(0.2f, 0.5f, 0.9f, 0.8f), &brush);
                if (brush) {
                    ID2D1PathGeometry* path = nullptr;
                    d2d_factory_->CreatePathGeometry(&path);
                    if (path) {
                        ID2D1GeometrySink* sink = nullptr;
                        path->Open(&sink);
                        if (sink) {
                            sink->BeginFigure(start, D2D1_FIGURE_BEGIN_HOLLOW);
                            float dy = (std::max)(fabsf(wire_end_y_ - start.y) * 0.4f, 30.0f);
                            sink->AddBezier(D2D1::BezierSegment(
                                D2D1::Point2F(start.x, start.y + dy),
                                D2D1::Point2F(wire_end_x_, wire_end_y_ - dy),
                                D2D1::Point2F(wire_end_x_, wire_end_y_)));
                            sink->EndFigure(D2D1_FIGURE_END_OPEN);
                            sink->Close();
                            sink->Release();

                            ID2D1StrokeStyle* dashStyle = nullptr;
                            float dashes[] = {6.0f, 4.0f};
                            d2d_factory_->CreateStrokeStyle(
                                D2D1::StrokeStyleProperties(
                                    D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                    D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 10.0f,
                                    D2D1_DASH_STYLE_CUSTOM, 0.0f),
                                dashes, 2, &dashStyle);

                            render_target_->DrawGeometry(path, brush, 2.0f, dashStyle);
                            if (dashStyle) dashStyle->Release();
                        }
                        path->Release();
                    }
                    brush->Release();
                }
            }
        }

        // Draw nodes
        for (auto& node : graph->nodes) {
            draw_node(node);
        }
    }

    // Reset transform for overlays
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    draw_method_title();

    HRESULT hr = render_target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        discard_device_resources();
    }
}

void GraphCanvas::draw_grid() {
    float gs = GRID_SIZE;
    if (gs * zoom_ < 4) return;

    ID2D1SolidColorBrush* brush = nullptr;
    render_target_->CreateSolidColorBrush(color(0.85f, 0.85f, 0.88f), &brush);
    if (!brush) return;

    // Visible area in graph coords
    float left = -pan_x_ / zoom_;
    float top = -pan_y_ / zoom_;
    float right = left + width_ / zoom_;
    float bottom = top + height_ / zoom_;

    float start_x = floorf(left / gs) * gs;
    float start_y = floorf(top / gs) * gs;

    for (float x = start_x; x <= right; x += gs) {
        for (float y = start_y; y <= bottom; y += gs) {
            render_target_->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(x, y), 1.0f, 1.0f), brush);
        }
    }
    brush->Release();
}

void GraphCanvas::draw_node(const VisualNode& node) {
    auto colors = node_colors(node.node_type);

    // Node body (rounded rect)
    D2D1_ROUNDED_RECT body_rr = D2D1::RoundedRect(
        D2D1::RectF(node.x, node.y, node.x + node.width, node.y + node.height),
        NODE_CORNER_RADIUS, NODE_CORNER_RADIUS);

    ID2D1SolidColorBrush* body_brush = nullptr;
    render_target_->CreateSolidColorBrush(colors.body, &body_brush);
    if (body_brush) {
        render_target_->FillRoundedRectangle(body_rr, body_brush);
        body_brush->Release();
    }

    // Header background
    D2D1_ROUNDED_RECT header_rr = D2D1::RoundedRect(
        D2D1::RectF(node.x, node.y, node.x + node.width, node.y + HEADER_HEIGHT),
        NODE_CORNER_RADIUS, NODE_CORNER_RADIUS);

    ID2D1SolidColorBrush* header_brush = nullptr;
    render_target_->CreateSolidColorBrush(colors.header, &header_brush);
    if (header_brush) {
        render_target_->FillRoundedRectangle(header_rr, header_brush);
        // Fill bottom corners of header (square)
        D2D1_RECT_F sq = D2D1::RectF(node.x, node.y + HEADER_HEIGHT - NODE_CORNER_RADIUS,
                                       node.x + node.width, node.y + HEADER_HEIGHT);
        render_target_->FillRectangle(sq, header_brush);
        header_brush->Release();
    }

    // Border
    D2D1_COLOR_F border_col = node.selected
        ? color(0.2f, 0.5f, 0.9f) : colors.border;
    float border_w = node.selected ? 2.5f : 1.0f;

    ID2D1SolidColorBrush* border_brush = nullptr;
    render_target_->CreateSolidColorBrush(border_col, &border_brush);
    if (border_brush) {
        render_target_->DrawRoundedRectangle(body_rr, border_brush, border_w);
        border_brush->Release();
    }

    // Shadow (simple bottom line)
    ID2D1SolidColorBrush* shadow_brush = nullptr;
    render_target_->CreateSolidColorBrush(color(0, 0, 0, 0.1f), &shadow_brush);
    if (shadow_brush) {
        render_target_->DrawLine(
            D2D1::Point2F(node.x + 2, node.y + node.height + 1),
            D2D1::Point2F(node.x + node.width - 2, node.y + node.height + 1),
            shadow_brush, 2.0f);
        shadow_brush->Release();
    }

    // Label text
    if (text_format_) {
        ID2D1SolidColorBrush* text_brush = nullptr;
        render_target_->CreateSolidColorBrush(colors.header_text, &text_brush);
        if (text_brush) {
            std::wstring wlabel(node.label.begin(), node.label.end());
            D2D1_RECT_F text_rect = D2D1::RectF(
                node.x + 4, node.y + 2,
                node.x + node.width - 4, node.y + HEADER_HEIGHT);
            render_target_->DrawText(wlabel.c_str(), (UINT32)wlabel.size(),
                                     text_format_, text_rect, text_brush);
            text_brush->Release();
        }
    }

    // Input pins (top edge)
    ID2D1SolidColorBrush* in_pin_brush = nullptr;
    render_target_->CreateSolidColorBrush(color(0.2f, 0.5f, 0.9f), &in_pin_brush);
    if (in_pin_brush) {
        for (int i = 0; i < node.num_inputs; i++) {
            auto p = input_pin_pos(node, i);
            render_target_->FillEllipse(
                D2D1::Ellipse(p, PIN_RADIUS, PIN_RADIUS), in_pin_brush);
        }
        in_pin_brush->Release();
    }

    // Output pins (bottom edge)
    ID2D1SolidColorBrush* out_pin_brush = nullptr;
    render_target_->CreateSolidColorBrush(color(0.9f, 0.3f, 0.2f), &out_pin_brush);
    if (out_pin_brush) {
        for (int i = 0; i < node.num_outputs; i++) {
            auto p = output_pin_pos(node, i);
            render_target_->FillEllipse(
                D2D1::Ellipse(p, PIN_RADIUS, PIN_RADIUS), out_pin_brush);
        }
        out_pin_brush->Release();
    }
}

void GraphCanvas::draw_wire(const VisualWire& wire) {
    if (!app_ || !app_->current_graph()) return;
    auto* graph = app_->current_graph();

    auto* src = graph->find_node(wire.source_node);
    auto* dst = graph->find_node(wire.target_node);
    if (!src || !dst) return;

    auto start = output_pin_pos(*src, wire.source_pin);
    auto end = input_pin_pos(*dst, wire.target_pin);

    // Bezier control points
    float dy = (std::max)(fabsf(end.y - start.y) * 0.4f, 30.0f);

    ID2D1PathGeometry* path = nullptr;
    d2d_factory_->CreatePathGeometry(&path);
    if (!path) return;

    ID2D1GeometrySink* sink = nullptr;
    path->Open(&sink);
    if (sink) {
        sink->BeginFigure(start, D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(start.x, start.y + dy),
            D2D1::Point2F(end.x, end.y - dy),
            end));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        sink->Release();
    }

    D2D1_COLOR_F wire_color;
    float wire_width;
    ID2D1StrokeStyle* style = nullptr;

    if (wire.selected) {
        wire_color = color(0.2f, 0.5f, 0.9f);
        wire_width = 2.5f;
    } else if (wire.is_execution) {
        wire_color = color(0.5f, 0.5f, 0.5f, 0.7f);
        wire_width = 2.0f;
        float dashes[] = {8.0f, 4.0f};
        d2d_factory_->CreateStrokeStyle(
            D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 10.0f,
                D2D1_DASH_STYLE_CUSTOM, 0.0f),
            dashes, 2, &style);
    } else {
        wire_color = color(0, 0, 0, 0.55f);
        wire_width = 1.5f;
    }

    ID2D1SolidColorBrush* brush = nullptr;
    render_target_->CreateSolidColorBrush(wire_color, &brush);
    if (brush) {
        render_target_->DrawGeometry(path, brush, wire_width, style);
        brush->Release();
    }

    if (style) style->Release();
    path->Release();
}

void GraphCanvas::draw_method_title() {
    if (!app_ || app_->selected_method().empty()) return;

    // Draw method title overlay at top-left
    ID2D1SolidColorBrush* bg_brush = nullptr;
    render_target_->CreateSolidColorBrush(color(1, 1, 1, 0.9f), &bg_brush);
    ID2D1SolidColorBrush* text_brush = nullptr;
    render_target_->CreateSolidColorBrush(color(0.2f, 0.2f, 0.2f), &text_brush);

    if (bg_brush && text_brush && title_text_format_) {
        std::string title = app_->selected_method();
        int ncases = app_->case_count();
        if (ncases > 1) {
            title += "  [Case " + std::to_string(app_->selected_case_index() + 1)
                   + " of " + std::to_string(ncases) + "]";
        }
        std::wstring wtitle(title.begin(), title.end());

        D2D1_RECT_F bg_rect = D2D1::RectF(8, 8, 400, 32);
        D2D1_ROUNDED_RECT bg_rr = D2D1::RoundedRect(bg_rect, 4, 4);
        render_target_->FillRoundedRectangle(bg_rr, bg_brush);

        D2D1_RECT_F text_rect = D2D1::RectF(16, 8, 400, 32);
        title_text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        title_text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        render_target_->DrawText(wtitle.c_str(), (UINT32)wtitle.size(),
                                 title_text_format_, text_rect, text_brush);
    }

    if (bg_brush) bg_brush->Release();
    if (text_brush) text_brush->Release();
}

// -----------------------------------------------------------------------
// Hit testing
// -----------------------------------------------------------------------

VisualNode* GraphCanvas::hit_test_node(float gx, float gy) {
    if (!app_ || !app_->current_graph()) return nullptr;
    auto* graph = app_->current_graph();
    // Iterate in reverse (top-most nodes first)
    for (int i = (int)graph->nodes.size() - 1; i >= 0; i--) {
        auto& n = graph->nodes[i];
        if (gx >= n.x && gx <= n.x + n.width &&
            gy >= n.y && gy <= n.y + n.height) {
            return &graph->nodes[i];
        }
    }
    return nullptr;
}

int GraphCanvas::hit_test_output_pin(const VisualNode& node, float gx, float gy) {
    for (int i = 0; i < node.num_outputs; i++) {
        auto p = output_pin_pos(node, i);
        float dx = gx - p.x, dy = gy - p.y;
        if (dx * dx + dy * dy <= PIN_HIT_RADIUS * PIN_HIT_RADIUS) return i;
    }
    return -1;
}

int GraphCanvas::hit_test_input_pin(const VisualNode& node, float gx, float gy) {
    for (int i = 0; i < node.num_inputs; i++) {
        auto p = input_pin_pos(node, i);
        float dx = gx - p.x, dy = gy - p.y;
        if (dx * dx + dy * dy <= PIN_HIT_RADIUS * PIN_HIT_RADIUS) return i;
    }
    return -1;
}

// -----------------------------------------------------------------------
// Mouse handling
// -----------------------------------------------------------------------

void GraphCanvas::on_lbutton_down(int x, int y, WPARAM flags) {
    SetCapture(hwnd_);
    drag_start_ = {x, y};

    auto gp = screen_to_graph({x, y});

    if (app_ && app_->current_graph()) {
        auto* graph = app_->current_graph();

        // Check for pin hit first
        auto* node = hit_test_node(gp.x, gp.y);
        if (node) {
            int out_pin = hit_test_output_pin(*node, gp.x, gp.y);
            if (out_pin >= 0) {
                // Start wire drag
                is_dragging_wire_ = true;
                wire_source_node_ = node->id;
                wire_source_pin_ = out_pin;
                wire_end_x_ = gp.x;
                wire_end_y_ = gp.y;
                return;
            }

            // Node drag
            is_dragging_node_ = true;
            dragged_node_id_ = node->id;
            drag_node_offset_x_ = gp.x - node->x;
            drag_node_offset_y_ = gp.y - node->y;
            graph->select_node(node->id);
            invalidate();
            return;
        }

        // Click on empty space: deselect all
        graph->deselect_all();
        invalidate();
    }

    // Start panning
    is_panning_ = true;
    pan_start_x_ = pan_x_;
    pan_start_y_ = pan_y_;
}

void GraphCanvas::on_lbutton_up(int x, int y) {
    ReleaseCapture();

    if (is_dragging_wire_ && app_ && app_->current_graph()) {
        auto gp = screen_to_graph({x, y});
        auto* graph = app_->current_graph();

        // Try to find target input pin
        auto* target_node = hit_test_node(gp.x, gp.y);
        if (target_node && target_node->id != wire_source_node_) {
            int in_pin = hit_test_input_pin(*target_node, gp.x, gp.y);
            if (in_pin >= 0) {
                // Create wire
                VisualWire vw;
                vw.id = (uint32_t)(graph->wires.size() + 1);
                vw.source_node = wire_source_node_;
                vw.source_pin = wire_source_pin_;
                vw.target_node = target_node->id;
                vw.target_pin = in_pin;
                graph->wires.push_back(vw);
            }
        }
    }

    is_panning_ = false;
    is_dragging_node_ = false;
    is_dragging_wire_ = false;
    invalidate();
}

void GraphCanvas::on_mouse_move(int x, int y, WPARAM flags) {
    if (is_panning_) {
        pan_x_ = pan_start_x_ + (x - drag_start_.x);
        pan_y_ = pan_start_y_ + (y - drag_start_.y);
        invalidate();
    } else if (is_dragging_node_ && app_ && app_->current_graph()) {
        auto gp = screen_to_graph({x, y});
        auto* node = app_->current_graph()->find_node(dragged_node_id_);
        if (node) {
            node->x = gp.x - drag_node_offset_x_;
            node->y = gp.y - drag_node_offset_y_;
            invalidate();
        }
    } else if (is_dragging_wire_) {
        auto gp = screen_to_graph({x, y});
        wire_end_x_ = gp.x;
        wire_end_y_ = gp.y;
        invalidate();
    }
}

void GraphCanvas::on_mouse_wheel(int x, int y, int delta) {
    POINT cursor = {x, y};
    ScreenToClient(hwnd_, &cursor);

    float factor = delta > 0 ? 1.1f : 0.9f;
    float new_zoom = zoom_ * factor;
    new_zoom = (std::max)(0.1f, (std::min)(5.0f, new_zoom));

    // Zoom towards cursor
    float cx = (float)cursor.x;
    float cy = (float)cursor.y;
    pan_x_ = cx - (cx - pan_x_) * (new_zoom / zoom_);
    pan_y_ = cy - (cy - pan_y_) * (new_zoom / zoom_);
    zoom_ = new_zoom;

    invalidate();
}

void GraphCanvas::zoom_in() {
    float cx = width_ / 2.0f, cy = height_ / 2.0f;
    float new_zoom = zoom_ * 1.25f;
    new_zoom = (std::min)(5.0f, new_zoom);
    pan_x_ = cx - (cx - pan_x_) * (new_zoom / zoom_);
    pan_y_ = cy - (cy - pan_y_) * (new_zoom / zoom_);
    zoom_ = new_zoom;
    invalidate();
}

void GraphCanvas::zoom_out() {
    float cx = width_ / 2.0f, cy = height_ / 2.0f;
    float new_zoom = zoom_ * 0.8f;
    new_zoom = (std::max)(0.1f, new_zoom);
    pan_x_ = cx - (cx - pan_x_) * (new_zoom / zoom_);
    pan_y_ = cy - (cy - pan_y_) * (new_zoom / zoom_);
    zoom_ = new_zoom;
    invalidate();
}

void GraphCanvas::fit_to_window() {
    if (!app_ || !app_->current_graph() || app_->current_graph()->nodes.empty()) return;
    auto* graph = app_->current_graph();

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (auto& n : graph->nodes) {
        minX = (std::min)(minX, n.x);
        minY = (std::min)(minY, n.y);
        maxX = (std::max)(maxX, n.x + n.width);
        maxY = (std::max)(maxY, n.y + n.height);
    }

    float graphW = maxX - minX;
    float graphH = maxY - minY;
    float margin = 40.0f;

    float scaleX = (width_ - margin * 2) / (std::max)(graphW, 1.0f);
    float scaleY = (height_ - margin * 2) / (std::max)(graphH, 1.0f);
    zoom_ = (std::max)(0.1f, (std::min)((std::min)(scaleX, scaleY), 2.0f));

    pan_x_ = (width_ - graphW * zoom_) / 2 - minX * zoom_;
    pan_y_ = (height_ - graphH * zoom_) / 2 - minY * zoom_;

    invalidate();
}

// -----------------------------------------------------------------------
// Window procedure
// -----------------------------------------------------------------------

LRESULT CALLBACK GraphCanvas::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GraphCanvas* self = nullptr;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<GraphCanvas*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<GraphCanvas*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        self->on_paint();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        self->resize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_LBUTTONDOWN:
        self->on_lbutton_down(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;
    case WM_LBUTTONUP:
        self->on_lbutton_up(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        self->on_mouse_move(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;
    case WM_MOUSEWHEEL:
        self->on_mouse_wheel(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
                             GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    case WM_ERASEBKGND:
        return 1; // Prevent flicker
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
