//
//  camera.cc
//
//  Created by Pujun Lun on 3/13/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "camera.h"

#include <stdexcept>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

namespace camera {
namespace {

using glm::radians;
using glm::vec3;
using window::key_map::KeyMap;

} /* namespace */

Camera::Camera(const vec3& position, const vec3& front, const vec3& up,
               float fov, float near, float far,
               float yaw, float pitch, float sensitivity)
    : pos_{position}, front_{front}, up_{up},
      fov_{fov}, near_{near}, far_{far},
      yaw_{yaw}, pitch_{pitch}, sensitivity_{sensitivity} {
  UpdateRightVector();
  UpdateViewMatrix();
}

void Camera::UpdateFrontVector() {
  front_ = vec3(glm::cos(radians(pitch_)) * glm::cos(radians(yaw_)),
                glm::sin(radians(pitch_)),
                glm::cos(radians(pitch_)) * glm::sin(radians(yaw_)));
}

void Camera::UpdateRightVector() {
  right_ = glm::normalize(glm::cross(front_, up_));
}

void Camera::UpdateViewMatrix() {
  view_ = glm::lookAt(pos_, pos_ + front_, up_);
}

void Camera::UpdateProjMatrix() {
  proj_ = glm::perspective(radians(fov_), (float)width_ / height_, near_, far_);
}

void Camera::Init(const glm::ivec2& screen_size, const glm::dvec2& cursor_pos) {
  width_ = screen_size.x;
  height_ = screen_size.y;
  last_x_ = cursor_pos.x;
  last_y_ = cursor_pos.y;
  UpdateProjMatrix();
}

void Camera::ProcessCursorMove(double x, double y) {
  float x_offset = (x - last_x_) * sensitivity_;
  float y_offset = (last_y_ - y) * sensitivity_;
  last_x_ = x;
  last_y_ = y;
  yaw_ = glm::mod(yaw_ + x_offset, 360.0f);
  pitch_ = glm::clamp(pitch_ + y_offset, -89.0f, 89.0f);

  UpdateFrontVector();
  UpdateRightVector();
  UpdateViewMatrix();
}

void Camera::ProcessScroll(double y, double min_val, double max_val) {
  fov_ = glm::clamp(fov_ + y, min_val, max_val);
  UpdateProjMatrix();
}

void Camera::ProcessKey(KeyMap key, float elapsed_time) {
  float distance = elapsed_time * 5.0f;
  switch (key) {
    case KeyMap::kUp:
      pos_ += front_ * distance;
      break;
    case KeyMap::kDown:
      pos_ -= front_ * distance;
      break;
    case KeyMap::kLeft:
      pos_ -= right_ * distance;
      break;
    case KeyMap::kRight:
      pos_ += right_ * distance;
      break;
    default:
      throw std::runtime_error{"Unsupported key"};
  }
  UpdateViewMatrix();
}

} /* namespace camera */