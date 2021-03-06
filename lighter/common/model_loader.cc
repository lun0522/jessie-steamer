//
//  model_loader.cc
//
//  Created by Pujun Lun on 4/13/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/common/model_loader.h"

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/assimp/Importer.hpp"
#include "third_party/assimp/postprocess.h"

namespace lighter::common {
namespace {

// Translates the resource type we defined to its counterpart in Assimp.
aiTextureType TextureTypeToAssimpType(ModelLoader::TextureType type) {
  using TextureType = ModelLoader::TextureType;
  switch (type) {
    case TextureType::kDiffuse:
      return aiTextureType_DIFFUSE;
    case TextureType::kSpecular:
      return aiTextureType_SPECULAR;
    case TextureType::kReflection:
      return aiTextureType_AMBIENT;
    default:
      FATAL(absl::StrFormat("Unsupported resource type: %d", type));
  }
}

}  // namespace

ModelLoader::ModelLoader(const std::string& model_path,
                         const std::string& texture_dir) {
  constexpr unsigned int flags = aiProcess_Triangulate
                                     | aiProcess_GenNormals
                                     | aiProcess_PreTransformVertices
                                     | aiProcess_FlipUVs;

  Assimp::Importer importer;
  const aiScene* scene = importer.ReadFile(model_path, flags);
  ASSERT_FALSE(scene == nullptr || scene->mRootNode == nullptr ||
               (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE),
               absl::StrFormat("Failed to import scene: %s",
                               importer.GetErrorString()));

  ProcessNode(texture_dir, scene->mRootNode, scene);
}

void ModelLoader::ProcessNode(const std::string& directory,
                              const aiNode* node,
                              const aiScene* scene) {
  mesh_datas_.reserve(mesh_datas_.size() + node->mNumMeshes);
  for (int i = 0; i < node->mNumMeshes; ++i) {
    const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
    mesh_datas_.push_back(LoadMesh(directory, mesh, scene));
  }
  for (int i = 0; i < node->mNumChildren; ++i) {
    ProcessNode(directory, node->mChildren[i], scene);
  }
}

ModelLoader::MeshData ModelLoader::LoadMesh(const std::string& directory,
                                            const aiMesh* mesh,
                                            const aiScene* scene) const {
  // Load vertices. Assimp allows a vertex to have multiple sets of texture
  // coordinates. We will simply use the first set.
  std::vector<Vertex3DWithTex> vertices;
  vertices.reserve(mesh->mNumVertices);
  constexpr int kTexCoordSetIndex = 0;
  const aiVector3D* tex_coord_set = mesh->mTextureCoords[kTexCoordSetIndex];
  for (int i = 0; i < mesh->mNumVertices; ++i) {
    const glm::vec3 position{mesh->mVertices[i].x,
                             mesh->mVertices[i].y,
                             mesh->mVertices[i].z};
    const glm::vec3 normal{mesh->mNormals[i].x,
                           mesh->mNormals[i].y,
                           mesh->mNormals[i].z};
    glm::vec2 tex_coord{0.0f};
    if (tex_coord_set != nullptr) {
      tex_coord = {tex_coord_set[i].x, tex_coord_set[i].y};
    }
    vertices.push_back({position, normal, tex_coord});
  }

  // Load indices.
  std::vector<uint32_t> indices;
  for (int i = 0; i < mesh->mNumFaces; ++i) {
    const aiFace& face = mesh->mFaces[i];
    indices.insert(indices.end(), face.mIndices,
                   face.mIndices + face.mNumIndices);
  }

  // Load textures.
  std::vector<TextureInfo> textures;
  if (scene->HasMaterials()) {
    const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    LoadTextures(directory, material, TextureType::kDiffuse, &textures);
    LoadTextures(directory, material, TextureType::kSpecular, &textures);
    LoadTextures(directory, material, TextureType::kReflection, &textures);
  }

  return MeshData{std::move(vertices), std::move(indices), std::move(textures)};
}

void ModelLoader::LoadTextures(const std::string& directory,
                               const aiMaterial* material,
                               TextureType texture_type,
                               std::vector<TextureInfo>* texture_infos) const {
  const aiTextureType ai_type = TextureTypeToAssimpType(texture_type);
  const int num_textures = material->GetTextureCount(ai_type);
  texture_infos->reserve(texture_infos->size() + num_textures);
  for (unsigned int i = 0; i < num_textures; ++i) {
    aiString path;
    material->GetTexture(ai_type, i, &path);
    texture_infos->push_back(TextureInfo{
        absl::StrFormat("%s/%s", directory, path.C_Str()), texture_type});
  }
}

}  // namespace lighter::common
