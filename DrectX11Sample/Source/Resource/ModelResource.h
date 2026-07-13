#pragma once

#include <SimpleMath.h>
#include <d3d11.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct ModelVertex
{
	DirectX::SimpleMath::Vector3 position = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3 normal = DirectX::SimpleMath::Vector3::Up;
	DirectX::SimpleMath::Vector2 uv = DirectX::SimpleMath::Vector2::Zero;

	// Reserved for future skeletal animation. Static rendering does not use them yet.
	uint32_t boneIndices[4] = {};
	float boneWeights[4] = {};
};

struct ModelMesh
{
	std::vector<ModelVertex> vertices;
	std::vector<uint32_t> indices;
	uint32_t materialIndex = 0;
	ID3D11Buffer* vertexBuffer = nullptr;
	ID3D11Buffer* indexBuffer = nullptr;

	~ModelMesh();
	ModelMesh() = default;
	ModelMesh(const ModelMesh&) = delete;
	ModelMesh& operator=(const ModelMesh&) = delete;
	ModelMesh(ModelMesh&& other) noexcept;
	ModelMesh& operator=(ModelMesh&& other) noexcept;
};

struct ModelMaterial
{
	std::string diffuseTexturePath;
	ID3D11ShaderResourceView* diffuseTextureView = nullptr;

	~ModelMaterial();
	ModelMaterial() = default;
	ModelMaterial(const ModelMaterial&) = delete;
	ModelMaterial& operator=(const ModelMaterial&) = delete;
	ModelMaterial(ModelMaterial&& other) noexcept;
	ModelMaterial& operator=(ModelMaterial&& other) noexcept;
};

struct ModelBone
{
	std::string name;
	int parentIndex = -1;
	DirectX::SimpleMath::Matrix offsetMatrix = DirectX::SimpleMath::Matrix::Identity;
};

struct ModelVectorKeyframe
{
	double time = 0.0;
	DirectX::SimpleMath::Vector3 value = DirectX::SimpleMath::Vector3::Zero;
};

struct ModelQuaternionKeyframe
{
	double time = 0.0;
	DirectX::SimpleMath::Quaternion value = DirectX::SimpleMath::Quaternion::Identity;
};

struct ModelAnimationChannel
{
	std::string boneName;
	uint32_t boneIndex = 0;
	std::vector<ModelVectorKeyframe> positionKeys;
	std::vector<ModelQuaternionKeyframe> rotationKeys;
	std::vector<ModelVectorKeyframe> scaleKeys;
};

struct ModelAnimationClip
{
	std::string name;
	double duration = 0.0;
	double ticksPerSecond = 0.0;
	std::vector<ModelAnimationChannel> channels;
};

class ModelResource
{
public:
	~ModelResource() = default;

	bool LoadFromFile(ID3D11Device* device, const std::string& path);
	const std::vector<ModelMesh>& GetMeshes() const;
	const std::vector<ModelMaterial>& GetMaterials() const;
	const std::vector<ModelBone>& GetBones() const;
	const std::vector<ModelAnimationClip>& GetAnimationClips() const;

private:
	std::vector<ModelMesh> meshes;
	std::vector<ModelMaterial> materials;
	std::vector<ModelBone> bones;
	std::vector<ModelAnimationClip> animationClips;
};

class ModelResourceManager
{
public:
	static bool LoadModel(const std::string& key, const std::string& path, ID3D11Device* device);
	static const ModelResource* GetModel(const std::string& key);
	static void UnloadAll();

private:
	static std::unordered_map<std::string, std::unique_ptr<ModelResource>> resources;
};
