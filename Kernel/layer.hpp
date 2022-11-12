#pragma once

#include <memory>
#include <map>
#include <vector>

#include "graphics.hpp"
#include "window.hpp"
#include "message.hpp"

class Layer {
  public:
    Layer(unsigned int id = 0);
    unsigned int ID() const;

    Layer& SetWindow(const std::shared_ptr<Window>& window);
    std::shared_ptr<Window> GetWindow() const;

    Layer& Move(Vector2D<int> pos);
    Layer& MoveRelative(Vector2D<int> pos_diff);    
    Layer& SetDraggable(bool draggable);

    void DrawTo(FrameBuffer& screen, const Rectangle<int>& area);    
    Vector2D<int> GetPosition() const;
    
    bool IsDraggable() const;

  private:
    unsigned int id_;
    Vector2D<int> pos_;
    std::shared_ptr<Window> window_;
    bool draggable_{false};
};

class LayerManager {
  public:
    void SetWriter(FrameBuffer* screen);
    Layer& NewLayer();
    void Draw(const Rectangle<int>& area) const;
    void Draw(unsigned int id) const;
    void Draw(unsigned int id, Rectangle<int> area) const;
    void Move(unsigned int id, Vector2D<int> new_position);
    void MoveRelative(unsigned int id, Vector2D<int> pos_diff);
    void UpDown(unsigned int id, int new_height);
    void Hide(unsigned int id);
    const Layer& GetLayer(unsigned int id);
    Layer* FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const;    
    Layer* FindLayer(unsigned int id);    
    int GetHeight(unsigned int id);
    
  private:
    FrameBuffer* screen_{nullptr};    
    mutable FrameBuffer back_buffer_{};
    std::vector<std::unique_ptr<Layer>> layers_{};
    std::vector<Layer*> layer_stack_{};
    unsigned int latest_id_{0};    
};

class ActiveLayer {
  public:
    ActiveLayer(LayerManager& manager);
    void SetMouseLayer(unsigned int mouse_layer);
    void Activate(unsigned int layer_id);
    unsigned int GetActive() const { return active_layer_; }

  private:
    LayerManager& manager_;
    unsigned int active_layer_{0};
    unsigned int mouse_layer_{0};
};

inline LayerManager* layer_manager;
inline ActiveLayer* active_layer;

void InitializeLayer();
void ProcessLayerMessage(const Message& message);