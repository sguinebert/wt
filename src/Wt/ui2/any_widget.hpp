#pragma once

#include <Wt/ui2/widget_like.hpp>

#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace Wt::ui2 {

class any_widget {
public:
  static constexpr std::size_t inline_capacity = 64;

  any_widget() = default;
  any_widget(std::nullptr_t) { }

  ~any_widget()
  {
    reset();
  }

  any_widget(const any_widget&) = delete;
  any_widget& operator=(const any_widget&) = delete;

  any_widget(any_widget&& other)
  {
    move_from(std::move(other));
  }

  any_widget& operator=(any_widget&& other)
  {
    if (this != &other) {
      reset();
      move_from(std::move(other));
    }

    return *this;
  }

  template<WidgetLike W>
  explicit any_widget(W widget)
  {
    emplace<std::decay_t<W>>(std::move(widget));
  }

  template<WidgetLike W, class... Args>
  W& emplace(Args&&... args)
  {
    static_assert(std::is_move_constructible_v<W>, "ui2::any_widget requires move-constructible widget types");

    reset();

    if constexpr (fits_inline<W>) {
      object_ = storage_;
      heap_ = false;
      ::new (object_) W(std::forward<Args>(args)...);
    } else {
      object_ = new W(std::forward<Args>(args)...);
      heap_ = true;
    }

    vtable_ = &table_for<W>();
    return *static_cast<W*>(object_);
  }

  [[nodiscard]] bool has_value() const noexcept
  {
    return object_ != nullptr;
  }

  explicit operator bool() const noexcept
  {
    return has_value();
  }

  void reset() noexcept
  {
    if (vtable_ && object_) {
      vtable_->destroy(object_, heap_);
    }

    object_ = nullptr;
    vtable_ = nullptr;
    heap_ = false;
  }

  NodeRef mount(MountContext& ctx)
  {
    assert(vtable_ && object_);
    return vtable_->mount(object_, ctx);
  }

  void patch(PatchContext& ctx)
  {
    if (!vtable_ || !object_) {
      return;
    }

    vtable_->patch(object_, ctx);
  }

  void unmount(MountContext& ctx)
  {
    if (!vtable_ || !object_) {
      return;
    }

    vtable_->unmount(object_, ctx);
  }

  void on_event(EventContext& ctx)
  {
    if (!vtable_ || !object_) {
      return;
    }

    vtable_->on_event(object_, ctx);
  }

private:
  struct VTable {
    NodeRef (*mount)(void* object, MountContext& ctx);
    void (*patch)(void* object, PatchContext& ctx);
    void (*unmount)(void* object, MountContext& ctx);
    void (*on_event)(void* object, EventContext& ctx);
    void (*destroy)(void* object, bool heap) noexcept;
    void (*move)(void*& srcObject, bool& srcHeap, void* dstStorage, void*& dstObject, bool& dstHeap);
  };

  template<class W>
  static constexpr bool fits_inline =
    sizeof(W) <= inline_capacity && alignof(W) <= alignof(std::max_align_t);

  template<class W>
  static const VTable& table_for()
  {
    static const VTable table{
      .mount = [](void* object, MountContext& ctx) -> NodeRef {
        return static_cast<W*>(object)->mount(ctx);
      },
      .patch = [](void* object, PatchContext& ctx) {
        static_cast<W*>(object)->patch(ctx);
      },
      .unmount = [](void* object, MountContext& ctx) {
        static_cast<W*>(object)->unmount(ctx);
      },
      .on_event = [](void* object, EventContext& ctx) {
        if constexpr (EventWidgetLike<W>) {
          static_cast<W*>(object)->on_event(ctx);
        } else {
          (void)object;
          (void)ctx;
        }
      },
      .destroy = [](void* object, bool heap) noexcept {
        if (heap) {
          delete static_cast<W*>(object);
        } else {
          std::destroy_at(static_cast<W*>(object));
        }
      },
      .move = [](void*& srcObject, bool& srcHeap, void* dstStorage, void*& dstObject, bool& dstHeap) {
        auto* src = static_cast<W*>(srcObject);

        if (srcHeap) {
          dstObject = srcObject;
          dstHeap = true;
          srcObject = nullptr;
          srcHeap = false;
          return;
        }

        if constexpr (fits_inline<W>) {
          ::new (dstStorage) W(std::move(*src));
          dstObject = dstStorage;
          dstHeap = false;
        } else {
          dstObject = new W(std::move(*src));
          dstHeap = true;
        }
      },
    };

    return table;
  }

  void move_from(any_widget&& other)
  {
    if (!other.vtable_ || !other.object_) {
      return;
    }

    other.vtable_->move(other.object_, other.heap_, storage_, object_, heap_);
    vtable_ = other.vtable_;

    other.reset();
  }

private:
  alignas(std::max_align_t) std::byte storage_[inline_capacity]{};
  void* object_{nullptr};
  const VTable* vtable_{nullptr};
  bool heap_{false};
};

} // namespace Wt::ui2
