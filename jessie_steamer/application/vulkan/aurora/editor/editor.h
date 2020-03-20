//
//  editor.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H

#include <array>
#include <memory>

#include "jessie_steamer/application/vulkan/aurora/editor/button.h"
#include "jessie_steamer/application/vulkan/aurora/editor/celestial.h"
#include "jessie_steamer/application/vulkan/aurora/editor/path.h"
#include "jessie_steamer/application/vulkan/aurora/scene.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/rotation.h"
#include "jessie_steamer/common/timer.h"
#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/window_context.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

// This class is used for rendering the aurora path editor using Vulkan APIs.
class EditorRenderer {
 public:
  explicit EditorRenderer(const wrapper::vulkan::WindowContext* window_context);

  // This class is neither copyable nor movable.
  EditorRenderer(const EditorRenderer&) = delete;
  EditorRenderer& operator=(const EditorRenderer&) = delete;

  // Recreates the swapchain and associated resources.
  void Recreate();

  // Renders the aurora path editor using 'render_ops'.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer, int framebuffer_index,
            absl::Span<const wrapper::vulkan::RenderPass::RenderOp> render_ops);

  // Accessors.
  const wrapper::vulkan::RenderPass& render_pass() const {
    return *render_pass_;
  }

 private:
  // Objects used for rendering.
  const wrapper::vulkan::WindowContext& window_context_;
  std::unique_ptr<wrapper::vulkan::NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
  std::unique_ptr<wrapper::vulkan::Image> depth_stencil_image_;
};

// This class is used to manage and render the aurora path editor scene.
class Editor : public Scene {
 public:
  Editor(wrapper::vulkan::WindowContext* window_context,
         int num_frames_in_flight);

  // This class is neither copyable nor movable.
  Editor(const Editor&) = delete;
  Editor& operator=(const Editor&) = delete;

  // Overrides.
  void OnEnter() override;
  void OnExit() override;
  void Recreate() override;
  void UpdateData(int frame) override;
  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame) override;
  bool ShouldTransitionScene() const override {
    return state_manager_.ShouldDisplayAurora();
  }

 private:
  enum ButtonIndex {
    kPath1ButtonIndex,
    kPath2ButtonIndex,
    kPath3ButtonIndex,
    kEditingButtonIndex,
    kDaylightButtonIndex,
    kAuroraButtonIndex,
    kNumButtons,
    kNumAuroraPaths = kEditingButtonIndex,
  };

  // Manages button states.
  class StateManager {
   public:
    explicit StateManager();

    // This class is neither copyable nor movable.
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    // Updates button states. 'clicked_button' should be absl::nullopt if no
    // button is clicked. Note that the state of clicked button may not change
    // if it keeps being clicked, until the button bouncing time is reached.
    void Update(absl::optional<ButtonIndex> clicked_button);

    // Returns the index of aurora path that is being edited.
    int GetEditingPathIndex() const;

    // Convenience functions for reading button states.
    bool IsSelected(ButtonIndex index) const {
      return button_states_[index] == Button::State::kSelected;
    }
    bool IsUnselected(ButtonIndex index) const {
      return button_states_[index] == Button::State::kUnselected;
    }
    bool IsEditing() const {
      return IsSelected(ButtonIndex::kEditingButtonIndex);
    }
    bool ShouldDisplayAurora() const {
      return IsSelected(ButtonIndex::kAuroraButtonIndex);
    }

    // Resets the state of display aurora button. This should be called every
    // time we enter this scene.
    void ResetDisplayAuroraButton() {
      button_states_[ButtonIndex::kAuroraButtonIndex] =
          Button::State::kUnselected;
    }

    // Accessors.
    const std::array<Button::State, kNumButtons>& button_states() const {
      return button_states_;
    }

   private:
    // Records a click on the button.
    struct ClickInfo {
      ButtonIndex button_index;
      float start_time;
    };

    // Sets states of all aurora path buttons to the same 'state'.
    void SetPathButtonStates(Button::State state);

    // Flips the state of button at 'index'. This should not be called if this
    // button is hidden currently.
    void FlipButtonState(ButtonIndex index);

    // Timer used for disabling the interaction between user and button for a
    // pre-defined time period if the user keeps clicking on the same button.
    common::BasicTimer timer_;

    // States of all buttons.
    std::array<Button::State, kNumButtons> button_states_;

    // Records the last click on any button.
    absl::optional<ClickInfo> click_info_;

    // Tracks the index of the last edited aurora path.
    ButtonIndex last_edited_path_ = kPath1ButtonIndex;
  };

  using ButtonColors = std::array<glm::vec3, button::kNumStates>;

  // Returns the color/alpha/center of each button.
  static const std::array<ButtonColors, kNumButtons>& GetButtonColors();
  static const std::array<float, button::kNumStates>& GetButtonAlphas();
  static const std::array<glm::vec2, kNumButtons>& GetButtonCenters();

  // Accessors.
  const wrapper::vulkan::RenderPass& render_pass() const {
    return editor_renderer_.render_pass();
  }

  // On-screen rendering context.
  wrapper::vulkan::WindowContext& window_context_;

  // Flags used for mouse button callbacks.
  bool did_press_left_ = false;
  bool did_release_right_ = false;

  // Renderer of the editor scene.
  EditorRenderer editor_renderer_;

  // Sphere models used to handle user interaction with the earth model and
  // virtual aurora layer in the scene.
  common::Sphere earth_;
  common::Sphere aurora_layer_;

  // Manages button states.
  StateManager state_manager_;

  // Renderers for objects in the scene.
  std::unique_ptr<Celestial> celestial_;
  std::unique_ptr<AuroraPath> aurora_path_;
  std::unique_ptr<Button> button_;

  // Camera models. We use a perspective camera for the skybox, and an
  // orthographic camera for the earth model, so that the user need not worry
  // about the distortion of perspective camera when editing aurora paths.
  std::unique_ptr<common::UserControlledCamera> general_camera_;
  std::unique_ptr<common::UserControlledCamera> skybox_camera_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H */
