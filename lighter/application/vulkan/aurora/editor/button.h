//
//  button.h
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_H
#define LIGHTER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_H

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lighter/application/vulkan/aurora/editor/button_maker.h"
#include "lighter/application/vulkan/aurora/editor/button_util.h"
#include "lighter/common/file.h"
#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {

namespace draw_button {

/* BEGIN: Consistent with vertex input attributes defined in shaders. */

struct RenderInfo {
  // Returns vertex input attributes.
  static std::vector<common::VertexAttribute> GetVertexAttributes();

  float alpha;
  glm::vec2 pos_center_ndc;
  glm::vec2 tex_coord_center;
};

/* END: Consistent with vertex input attributes defined in shaders. */

} /* namespace draw_button */

// This class is used to render multiple buttons with one render call using
// Vulkan APIs. It assumes that all buttons will have the same size, but
// different transparency and center location.
class ButtonRenderer {
 public:
  ButtonRenderer(
      const renderer::vulkan::SharedBasicContext& context,
      int num_buttons, const button::VerticesInfo& vertices_info,
      std::unique_ptr<renderer::vulkan::OffscreenImage>&& buttons_image);

  // This class is neither copyable nor movable.
  ButtonRenderer(const ButtonRenderer&) = delete;
  ButtonRenderer& operator=(const ButtonRenderer&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(
      VkSampleCountFlagBits sample_count,
      const renderer::vulkan::RenderPass& render_pass, uint32_t subpass_index,
      const renderer::vulkan::GraphicsPipelineBuilder::ViewportInfo& viewport);

  // Renders buttons. The number of buttons rendered depends on the length of
  // 'buttons_to_render'.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer,
            absl::Span<const draw_button::RenderInfo> buttons_to_render);

 private:
  // Creates a descriptor for 'vertices_uniform_' and 'buttons_image_'.
  std::unique_ptr<renderer::vulkan::StaticDescriptor> CreateDescriptor(
      const renderer::vulkan::SharedBasicContext& context) const;

  // Texture that contains all buttons in all states.
  const std::unique_ptr<renderer::vulkan::OffscreenImage> buttons_image_;

  // Objects used for rendering.
  std::unique_ptr<renderer::vulkan::DynamicPerInstanceBuffer>
      per_instance_buffer_;
  std::unique_ptr<renderer::vulkan::UniformBuffer> vertices_uniform_;
  std::unique_ptr<renderer::vulkan::StaticDescriptor> descriptor_;
  renderer::vulkan::GraphicsPipelineBuilder pipeline_builder_;
  std::unique_ptr<renderer::vulkan::Pipeline> pipeline_;
};

// This class is used to render multiple buttons with one render call.
// These buttons will share:
//   - Text font, height, location within each button, and color.
//   - Transparency in each state (i.e. selected and unselected state).
//   - Size of the button.
// They don't share:
//   - Text on the button.
//   - Color of the button (we can have different colors for different buttons
//     in different states).
//   - Center of the button on the frame.
// Update() must have been called before calling Draw() for the first time, and
// whenever the render pass is changed.
class Button {
 public:
  // Contains information for rendering multiple buttons onto a big texture.
  struct ButtonsInfo {
    // Contains information for rendering a single button.
    struct Info {
      std::string text;
      std::array<glm::vec3, button::kNumStates> colors;
      glm::vec2 center;
    };

    // 'base_y' and 'top_y' are in range [0.0, 1.0]. They control where do we
    // render text within each button.
    renderer::vulkan::Text::Font font;
    int font_height;
    float base_y;
    float top_y;
    glm::vec3 text_color;
    std::array<float, button::kNumStates> button_alphas;
    glm::vec2 button_size;
    absl::Span<const Info> button_infos;
  };

  // Possible states of each button.
  enum class State { kHidden, kSelected, kUnselected };

  // When the frame is resized, the aspect ratio of viewport will always be
  // 'viewport_aspect_ratio'.
  Button(const renderer::vulkan::SharedBasicContext& context,
         float viewport_aspect_ratio, const ButtonsInfo& buttons_info);

  // This class is neither copyable nor movable.
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  // For simplicity, the render area will be the same to 'frame_size'.
  void UpdateFramebuffer(const VkExtent2D& frame_size,
                         VkSampleCountFlagBits sample_count,
                         const renderer::vulkan::RenderPass& render_pass,
                         uint32_t subpass_index);

  // Renders all buttons. Buttons in 'State::kHidden' will not be rendered.
  // Others will be rendered with color and alpha selected according to states.
  // The size of 'button_states' must be equal to the size of
  // 'buttons_info.button_infos' passed to the constructor.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer,
            absl::Span<const State> button_states);

  // If any button is clicked, returns 'button_index_offset' plus the index of
  // it. Otherwise, returns std::nullopt. If the current state of a button is
  // 'State::kHidden', it will be ignored in this click detection.
  std::optional<int> GetClickedButtonIndex(
      const glm::vec2& click_ndc, int button_index_offset,
      absl::Span<const State> button_states) const;

 private:
  // The first dimension is different buttons, and the second dimension is
  // different states of one button.
  using DrawButtonRenderInfos =
      std::vector<std::array<draw_button::RenderInfo, button::kNumStates>>;

  // Describes the vertical position of text.
  struct TextPos {
    float base_y;
    float height;
  };

  // Returns a vector of make_button::RenderInfo for all buttons in all states.
  std::vector<make_button::RenderInfo> CreateMakeButtonRenderInfos(
      const ButtonsInfo& buttons_info) const;

  // Returns a button::VerticesInfo that stores the position and texture
  // coordinate of each vertex.
  button::VerticesInfo CreateMakeButtonVerticesInfo(
      int num_buttons, const glm::vec2& button_scale) const;

  // Returns a vector of TextPos to describe where to put each text when
  // generating the buttons image.
  std::vector<TextPos> CreateMakeButtonTextPos(
      const ButtonsInfo& buttons_info) const;

  // Extracts draw_button::RenderInfo from 'buttons_info'.
  DrawButtonRenderInfos ExtractDrawButtonRenderInfos(
      const ButtonsInfo& buttons_info) const;

  // Returns a button::VerticesInfo that stores the position and texture
  // coordinate of each vertex.
  button::VerticesInfo CreateDrawButtonVerticesInfo(
      const ButtonsInfo& buttons_info, const glm::vec2& button_uv_scale) const;

  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // buttons does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  // Size of each button on the frame in the normalized device coordinate.
  const glm::vec2 button_half_size_ndc_;

  // Rendering information for all buttons in all states.
  const DrawButtonRenderInfos all_buttons_;

  // Contains rendering information for buttons that will be rendered.
  std::vector<draw_button::RenderInfo> buttons_to_render_;

  // Renderer for buttons.
  std::unique_ptr<ButtonRenderer> button_renderer_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_H */
