//
//  file.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/common/file.h"

#include <cstdlib>
#include <fstream>

#include "lighter/common/util.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/strings/str_split.h"
#include "third_party/absl/strings/string_view.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

ABSL_FLAG(std::string, resource_folder, "external/resource",
          "Path to the resource folder");
ABSL_FLAG(std::string, shader_folder, "lighter/shader",
          "Path to the shader folder");
#ifdef USE_VULKAN
ABSL_FLAG(std::string, vulkan_folder, "external/lib-vulkan/",
          "Path to the Vulkan SDK folder");
#endif /* USE_VULKAN */

namespace lighter {
namespace common {
namespace {

// Opens the file in the given 'path' and checks whether it is successful.
std::ifstream OpenFile(const std::string& path) {
  std::ifstream file{path};
  ASSERT_FALSE(!file.is_open() || file.bad() || file.fail(),
               absl::StrCat("Failed to open file: ", path));
  return file;
}

// Returns the suffix of the given 'text', starting from index 'start_pos'.
inline absl::string_view GetSuffix(const std::string& text, size_t start_pos) {
  return absl::string_view{text.c_str() + start_pos, text.length() - start_pos};
}

// Splits the given 'text' by 'delimiter', while 'num_segments' is the expected
// length of results. An exception will be thrown if the length does not match.
std::vector<std::string> SplitText(absl::string_view text, char delimiter,
                                   int num_segments) {
  const std::vector<std::string> result =
      absl::StrSplit(text, delimiter, absl::SkipWhitespace{});
  ASSERT_TRUE(
      result.size() == num_segments,
      absl::StrFormat("Invalid number of segments (expected %d, but get %d)",
                      num_segments, result.size()));
  return result;
}

} /* namespace */

RawData::RawData(const std::string& path) {
  std::ifstream file = OpenFile(path);
  file.seekg(0, std::ios::end);
  size = file.tellg();
  auto* content = new char[size];
  file.seekg(0, std::ios::beg);
  file.read(content, size);
  data = content;
}

Image::Image(const std::string& path) {
  const auto raw_data = absl::make_unique<RawData>(path);
  data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(raw_data->data),
                               static_cast<int>(raw_data->size),
                               &width, &height, &channel, STBI_default);
  ASSERT_NON_NULL(data, absl::StrCat("Failed to read image from ", path));

  switch (channel) {
    case common::kBwImageChannel:
    case common::kRgbaImageChannel:
      break;
    case common::kRgbImageChannel: {
      stbi_image_free(const_cast<void*>(data));
      data = stbi_load_from_memory(
          reinterpret_cast<const stbi_uc*>(raw_data->data),
          static_cast<int>(raw_data->size),
          &width, &height, &channel, STBI_rgb_alpha);
      channel = STBI_rgb_alpha;
      break;
    }
    default:
      FATAL(absl::StrFormat("Unsupported number of channels: %d", channel));
  }
}

Image::Image(int width, int height, int channel,
             const void* raw_data, bool flip_y)
    : width{width}, height{height}, channel{channel} {
  ASSERT_TRUE(channel == common::kBwImageChannel ||
              channel == common::kRgbaImageChannel,
              absl::StrFormat("Unsupported number of channels: %d", channel));

  const size_t total_size = width * height * channel;
  data = std::malloc(total_size);
  if (flip_y) {
    const int stride = width * channel;
    for (int row = 0; row < height; ++row) {
      std::memcpy(
          static_cast<char*>(const_cast<void*>(data)) + stride * row,
          static_cast<const char*>(raw_data) + stride * (height - row - 1),
          stride);
    }
  } else {
    std::memcpy(const_cast<void*>(data), raw_data, total_size);
  }
}

Image::~Image() {
  std::free(const_cast<void*>(data));
}

std::array<Vertex2DPosOnly, 6> Vertex2DPosOnly::GetFullScreenSquadVertices() {
  return {
      Vertex2DPosOnly{/*pos=*/{-1.0f, -1.0f}},
      Vertex2DPosOnly{/*pos=*/{ 1.0f, -1.0f}},
      Vertex2DPosOnly{/*pos=*/{ 1.0f,  1.0f}},
      Vertex2DPosOnly{/*pos=*/{-1.0f, -1.0f}},
      Vertex2DPosOnly{/*pos=*/{ 1.0f,  1.0f}},
      Vertex2DPosOnly{/*pos=*/{-1.0f,  1.0f}},
  };
}

std::array<Vertex2D, 6> Vertex2D::GetFullScreenSquadVertices(bool flip_y) {
  if (flip_y) {
    return {
        Vertex2D{/*pos=*/{-1.0f, -1.0f}, /*tex_coord=*/{0.0f, 1.0f}},
        Vertex2D{/*pos=*/{ 1.0f, -1.0f}, /*tex_coord=*/{1.0f, 1.0f}},
        Vertex2D{/*pos=*/{ 1.0f,  1.0f}, /*tex_coord=*/{1.0f, 0.0f}},
        Vertex2D{/*pos=*/{-1.0f, -1.0f}, /*tex_coord=*/{0.0f, 1.0f}},
        Vertex2D{/*pos=*/{ 1.0f,  1.0f}, /*tex_coord=*/{1.0f, 0.0f}},
        Vertex2D{/*pos=*/{-1.0f,  1.0f}, /*tex_coord=*/{0.0f, 0.0f}},
    };
  } else {
    return {
        Vertex2D{/*pos=*/{-1.0f, -1.0f}, /*tex_coord=*/{0.0f, 0.0f}},
        Vertex2D{/*pos=*/{ 1.0f, -1.0f}, /*tex_coord=*/{1.0f, 0.0f}},
        Vertex2D{/*pos=*/{ 1.0f,  1.0f}, /*tex_coord=*/{1.0f, 1.0f}},
        Vertex2D{/*pos=*/{-1.0f, -1.0f}, /*tex_coord=*/{0.0f, 0.0f}},
        Vertex2D{/*pos=*/{ 1.0f,  1.0f}, /*tex_coord=*/{1.0f, 1.0f}},
        Vertex2D{/*pos=*/{-1.0f,  1.0f}, /*tex_coord=*/{0.0f, 1.0f}},
    };
  }
}

ObjFile::ObjFile(const std::string& path, int index_base) {
  std::ifstream file = OpenFile(path);

  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> tex_coords;
  absl::flat_hash_map<std::string, uint32_t> loaded_vertices;

  const auto parse_line = [&](const std::string& line) {
    const size_t non_space = line.find_first_not_of(' ');
    if (non_space == std::string::npos || line[0] == '#') {
      // Skip blank lines and comments.
      return;
    }

    switch (line[non_space]) {
      case 'v': {
        // Either position, normal or texture coordinates.
        switch (line[non_space + 1]) {
          case ' ': {
            // Position.
            const auto nums = SplitText(GetSuffix(line, non_space + 2), ' ',
                                        /*num_segments=*/3);
            positions.push_back(
                glm::vec3{stof(nums[0]), stof(nums[1]), stof(nums[2])});
            break;
          }
          case 'n': {
            // Normal.
            const auto nums = SplitText(GetSuffix(line, non_space + 3), ' ',
                                        /*num_segments=*/3);
            normals.push_back(
                glm::vec3{stof(nums[0]), stof(nums[1]), stof(nums[2])});
            break;
          }
          case 't': {
            // Texture coordinates.
            const auto nums = SplitText(GetSuffix(line, non_space + 3), ' ',
                                        /*num_segments=*/2);
            tex_coords.push_back(glm::vec2{stof(nums[0]), stof(nums[1])});
            break;
          }
          default:
            FATAL(absl::StrFormat("Unexpected symbol '%c'",
                                  line[non_space + 1]));
        }
        break;
      }
      case 'f': {
        // Face.
        for (const auto& seg : SplitText(GetSuffix(line, non_space + 2), ' ',
                                         /*num_segments=*/3)) {
          const auto iter = loaded_vertices.find(seg);
          if (iter != loaded_vertices.end()) {
            indices.push_back(iter->second);
          } else {
            indices.push_back(vertices.size());
            loaded_vertices[seg] = vertices.size();
            const auto idxs = SplitText(seg, '/', /*num_segments=*/3);
            vertices.push_back(Vertex3DWithTex{
                positions.at(stoi(idxs[0]) - index_base),
                normals.at(stoi(idxs[2]) - index_base),
                tex_coords.at(stoi(idxs[1]) - index_base),
            });
          }
        }
        break;
      }
      default:
        FATAL(absl::StrFormat("Unexpected symbol '%c'", line[non_space]));
    }
  };

  int line_num = 1;
  for (std::string line; getline(file, line); ++line_num) {
    try {
      parse_line(line);
    } catch (const std::out_of_range& e) {
      FATAL(absl::StrFormat("Out of range at line %d: %s", line_num, line));
    } catch (const std::invalid_argument& e) {
      FATAL(absl::StrFormat("Invalid argument at line %d: %s", line_num, line));
    } catch (const std::exception& e) {
      FATAL(absl::StrFormat("Failed to parse line %d: %s\n%s",
                            line_num, line, e.what()));
    }
  }
}

ObjFilePosOnly::ObjFilePosOnly(const std::string& path, int index_base) {
  ObjFile file(path, index_base);
  indices = std::move(file.indices);
  vertices.reserve(file.vertices.size());
  for (const auto& vertex : file.vertices) {
    vertices.push_back({vertex.pos});
  }
}

} /* namespace common */
} /* namespace lighter */