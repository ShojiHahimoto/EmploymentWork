#include "Resource/ModelResource.h"

#include <assimp/Importer.hpp>
#include "System/Debugger.h"
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <WICTextureLoader.h>

#include <algorithm>
#include <cctype>
#include <filesystem>

using namespace DirectX::SimpleMath;

std::unordered_map<std::string, std::unique_ptr<ModelResource>> ModelResourceManager::resources;

namespace
{
	void ReleaseBuffer(ID3D11Buffer*& buffer)
	{
		if (buffer)
		{
			buffer->Release();
			buffer = nullptr;
		}
	}

	void ReleaseTexture(ID3D11ShaderResourceView*& textureView)
	{
		if (textureView)
		{
			textureView->Release();
			textureView = nullptr;
		}
	}

	std::wstring ToWideString(const std::string& text)
	{
		return std::wstring(text.begin(), text.end());
	}

	Matrix ConvertAssimpMatrix(const aiMatrix4x4& source)
	{
		return Matrix(
			source.a1, source.b1, source.c1, source.d1,
			source.a2, source.b2, source.c2, source.d2,
			source.a3, source.b3, source.c3, source.d3,
			source.a4, source.b4, source.c4, source.d4);
	}

	Vector3 ConvertAssimpVector(const aiVector3D& source)
	{
		return Vector3(source.x, source.y, source.z);
	}

	Quaternion ConvertAssimpQuaternion(const aiQuaternion& source)
	{
		return Quaternion(source.x, source.y, source.z, source.w);
	}

	void AddBoneWeight(ModelVertex& vertex, uint32_t boneIndex, float weight)
	{
		for (int i = 0; i < 4; ++i)
		{
			if (vertex.boneWeights[i] <= 0.0f)
			{
				vertex.boneIndices[i] = boneIndex;
				vertex.boneWeights[i] = weight;
				return;
			}
		}
	}

	void NormalizeBoneWeights(ModelVertex& vertex)
	{
		float totalWeight = 0.0f;
		for (float weight : vertex.boneWeights)
		{
			totalWeight += weight;
		}

		if (totalWeight <= 0.0f)
		{
			vertex.boneIndices[0] = 0;
			vertex.boneWeights[0] = 1.0f;
			return;
		}

		for (float& weight : vertex.boneWeights)
		{
			weight /= totalWeight;
		}
	}

	void CollectNodeHierarchy(
		const aiNode* node,
		int parentIndex,
		std::vector<ModelBone>& bones,
		std::unordered_map<std::string, uint32_t>& boneIndexByName)
	{
		if (!node)
		{
			return;
		}

		const int currentIndex = static_cast<int>(bones.size());
		ModelBone bone;
		bone.name = node->mName.C_Str();
		bone.parentIndex = parentIndex;
		bones.push_back(bone);
		boneIndexByName[bone.name] = static_cast<uint32_t>(currentIndex);

		for (unsigned int i = 0; i < node->mNumChildren; ++i)
		{
			CollectNodeHierarchy(node->mChildren[i], currentIndex, bones, boneIndexByName);
		}
	}

	HRESULT CreateBuffer(
		ID3D11Device* device,
		const void* sourceData,
		UINT byteWidth,
		UINT bindFlags,
		ID3D11Buffer** buffer)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = byteWidth;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = bindFlags;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = sourceData;

		return device->CreateBuffer(&desc, &data, buffer);
	}

	std::filesystem::path ResolveTexturePath(const std::filesystem::path& modelPath, const aiString& texturePath)
	{
		std::filesystem::path sourcePath(texturePath.C_Str());
		if (sourcePath.is_absolute())
		{
			return sourcePath;
		}

		return modelPath.parent_path() / sourcePath.filename();
	}

	std::vector<std::filesystem::path> FindDiffuseTextures(const std::filesystem::path& modelDirectory)
	{
		std::vector<std::filesystem::path> diffuseTextures;
		if (!std::filesystem::exists(modelDirectory))
		{
			return diffuseTextures;
		}

		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(modelDirectory))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			std::string filename = entry.path().filename().string();
			std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char c)
				{
					return static_cast<char>(std::tolower(c));
				});

			if (filename.find("diffuse") != std::string::npos
				&& (entry.path().extension() == ".png" || entry.path().extension() == ".PNG"))
			{
				diffuseTextures.push_back(entry.path());
			}
		}

		return diffuseTextures;
	}

	std::filesystem::path FindFallbackDiffuseTexture(
		const std::vector<std::filesystem::path>& diffuseTextures,
		const std::string& materialName)
	{
		if (diffuseTextures.empty())
		{
			return {};
		}

		std::string loweredMaterialName = materialName;
		std::transform(loweredMaterialName.begin(), loweredMaterialName.end(), loweredMaterialName.begin(), [](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});

		for (const std::filesystem::path& texturePath : diffuseTextures)
		{
			std::string loweredTextureName = texturePath.filename().string();
			std::transform(loweredTextureName.begin(), loweredTextureName.end(), loweredTextureName.begin(), [](unsigned char c)
				{
					return static_cast<char>(std::tolower(c));
				});

			if ((loweredMaterialName.find("body") != std::string::npos && loweredTextureName.find("body") != std::string::npos)
				|| (loweredMaterialName.find("cloth") != std::string::npos && loweredTextureName.find("cloth") != std::string::npos)
				|| (loweredMaterialName.find("eyelash") != std::string::npos && loweredTextureName.find("eyelash") != std::string::npos))
			{
				return texturePath;
			}
		}

		return diffuseTextures.front();
	}

	void LoadDiffuseTexture(
		ID3D11Device* device,
		ModelMaterial& material,
		const std::filesystem::path& texturePath,
		std::unordered_map<std::string, ID3D11ShaderResourceView*>& textureCache)
	{
		if (texturePath.empty())
		{
			return;
		}

		material.diffuseTexturePath = texturePath.string();
		const auto cachedTexture = textureCache.find(material.diffuseTexturePath);
		if (cachedTexture != textureCache.end())
		{
			material.diffuseTextureView = cachedTexture->second;
			if (material.diffuseTextureView)
			{
				material.diffuseTextureView->AddRef();
			}
			return;
		}

		const std::wstring widePath = ToWideString(material.diffuseTexturePath);
		const HRESULT hr = DirectX::CreateWICTextureFromFile(
			device,
			widePath.c_str(),
			nullptr,
			&material.diffuseTextureView);

		if (SUCCEEDED(hr) && material.diffuseTextureView)
		{
			textureCache[material.diffuseTexturePath] = material.diffuseTextureView;
			DebugLog("[Model] Texture loaded: ", material.diffuseTexturePath);
		}
		else
		{
			DebugLog("[Model] Texture load failed: ", material.diffuseTexturePath, " hr=", static_cast<long>(hr));
		}
	}

	void LoadMaterials(
		ID3D11Device* device,
		const aiScene& scene,
		const std::filesystem::path& modelPath,
		std::vector<ModelMaterial>& materials)
	{
		materials.clear();
		materials.resize(scene.mNumMaterials);

		const std::vector<std::filesystem::path> fallbackDiffuseTextures = FindDiffuseTextures(modelPath.parent_path());
		std::unordered_map<std::string, ID3D11ShaderResourceView*> textureCache;

		for (unsigned int materialIndex = 0; materialIndex < scene.mNumMaterials; ++materialIndex)
		{
			const aiMaterial* sourceMaterial = scene.mMaterials[materialIndex];
			if (!sourceMaterial)
			{
				continue;
			}

			aiString texturePath;
			if (sourceMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS)
			{
				LoadDiffuseTexture(
					device,
					materials[materialIndex],
					ResolveTexturePath(modelPath, texturePath),
					textureCache);
				continue;
			}

			aiString materialName;
			sourceMaterial->Get(AI_MATKEY_NAME, materialName);
			LoadDiffuseTexture(
				device,
				materials[materialIndex],
				FindFallbackDiffuseTexture(fallbackDiffuseTextures, materialName.C_Str()),
				textureCache);
		}
	}

	uint32_t FindOrCreateBoneIndex(
		const std::string& boneName,
		std::vector<ModelBone>& bones,
		std::unordered_map<std::string, uint32_t>& boneIndexByName)
	{
		const auto foundBone = boneIndexByName.find(boneName);
		if (foundBone != boneIndexByName.end())
		{
			return foundBone->second;
		}

		ModelBone bone;
		bone.name = boneName;
		bones.push_back(bone);

		const uint32_t boneIndex = static_cast<uint32_t>(bones.size() - 1);
		boneIndexByName[boneName] = boneIndex;
		return boneIndex;
	}

	ModelAnimationClip ConvertAnimationClip(
		const aiAnimation& sourceAnimation,
		std::vector<ModelBone>& bones,
		std::unordered_map<std::string, uint32_t>& boneIndexByName)
	{
		ModelAnimationClip clip;
		clip.name = sourceAnimation.mName.C_Str();
		clip.duration = sourceAnimation.mDuration;
		clip.ticksPerSecond = sourceAnimation.mTicksPerSecond;

		for (unsigned int channelIndex = 0; channelIndex < sourceAnimation.mNumChannels; ++channelIndex)
		{
			const aiNodeAnim* sourceChannel = sourceAnimation.mChannels[channelIndex];
			if (!sourceChannel)
			{
				continue;
			}

			ModelAnimationChannel channel;
			channel.boneName = sourceChannel->mNodeName.C_Str();
			channel.boneIndex = FindOrCreateBoneIndex(channel.boneName, bones, boneIndexByName);

			for (unsigned int keyIndex = 0; keyIndex < sourceChannel->mNumPositionKeys; ++keyIndex)
			{
				const aiVectorKey& sourceKey = sourceChannel->mPositionKeys[keyIndex];
				ModelVectorKeyframe key;
				key.time = sourceKey.mTime;
				key.value = ConvertAssimpVector(sourceKey.mValue);
				channel.positionKeys.push_back(key);
			}

			for (unsigned int keyIndex = 0; keyIndex < sourceChannel->mNumRotationKeys; ++keyIndex)
			{
				const aiQuatKey& sourceKey = sourceChannel->mRotationKeys[keyIndex];
				ModelQuaternionKeyframe key;
				key.time = sourceKey.mTime;
				key.value = ConvertAssimpQuaternion(sourceKey.mValue);
				channel.rotationKeys.push_back(key);
			}

			for (unsigned int keyIndex = 0; keyIndex < sourceChannel->mNumScalingKeys; ++keyIndex)
			{
				const aiVectorKey& sourceKey = sourceChannel->mScalingKeys[keyIndex];
				ModelVectorKeyframe key;
				key.time = sourceKey.mTime;
				key.value = ConvertAssimpVector(sourceKey.mValue);
				channel.scaleKeys.push_back(key);
			}

			clip.channels.push_back(std::move(channel));
		}

		return clip;
	}
}

ModelMesh::~ModelMesh()
{
	ReleaseBuffer(indexBuffer);
	ReleaseBuffer(vertexBuffer);
}

ModelMesh::ModelMesh(ModelMesh&& other) noexcept
	: vertices(std::move(other.vertices))
	, indices(std::move(other.indices))
	, materialIndex(other.materialIndex)
	, vertexBuffer(other.vertexBuffer)
	, indexBuffer(other.indexBuffer)
{
	other.vertexBuffer = nullptr;
	other.indexBuffer = nullptr;
}

ModelMesh& ModelMesh::operator=(ModelMesh&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	ReleaseBuffer(indexBuffer);
	ReleaseBuffer(vertexBuffer);

	vertices = std::move(other.vertices);
	indices = std::move(other.indices);
	materialIndex = other.materialIndex;
	vertexBuffer = other.vertexBuffer;
	indexBuffer = other.indexBuffer;

	other.vertexBuffer = nullptr;
	other.indexBuffer = nullptr;

	return *this;
}

ModelMaterial::~ModelMaterial()
{
	ReleaseTexture(diffuseTextureView);
}

ModelMaterial::ModelMaterial(ModelMaterial&& other) noexcept
	: diffuseTexturePath(std::move(other.diffuseTexturePath))
	, diffuseTextureView(other.diffuseTextureView)
{
	other.diffuseTextureView = nullptr;
}

ModelMaterial& ModelMaterial::operator=(ModelMaterial&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	ReleaseTexture(diffuseTextureView);

	diffuseTexturePath = std::move(other.diffuseTexturePath);
	diffuseTextureView = other.diffuseTextureView;
	other.diffuseTextureView = nullptr;

	return *this;
}

bool ModelResource::LoadFromFile(ID3D11Device* device, const std::string& path)
{
	DebugLog("[Model] Load start: ", path);

	if (!device)
	{
		DebugLog("[Model] Load failed: device is null");
		return false;
	}

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(
		path,
		aiProcess_Triangulate
		| aiProcess_JoinIdenticalVertices
		| aiProcess_GenSmoothNormals
		| aiProcess_ConvertToLeftHanded
		| aiProcess_ImproveCacheLocality);

	if (!scene || !scene->HasMeshes())
	{
		DebugLog("[Model] Load failed: ", importer.GetErrorString());
		return false;
	}

	DebugLog(
		"[Model] Scene loaded. Meshes=",
		scene->mNumMeshes,
		" Materials=",
		scene->mNumMaterials,
		" Animations=",
		scene->mNumAnimations);

	meshes.clear();
	materials.clear();
	bones.clear();
	animationClips.clear();

	LoadMaterials(device, *scene, std::filesystem::path(path), materials);

	std::unordered_map<std::string, uint32_t> boneIndexByName;
	CollectNodeHierarchy(scene->mRootNode, -1, bones, boneIndexByName);

	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		const aiMesh* sourceMesh = scene->mMeshes[meshIndex];
		if (!sourceMesh)
		{
			continue;
		}

		ModelMesh mesh;
		mesh.materialIndex = sourceMesh->mMaterialIndex;
		mesh.vertices.resize(sourceMesh->mNumVertices);

		for (unsigned int vertexIndex = 0; vertexIndex < sourceMesh->mNumVertices; ++vertexIndex)
		{
			ModelVertex& vertex = mesh.vertices[vertexIndex];
			vertex.position = Vector3(
				sourceMesh->mVertices[vertexIndex].x,
				sourceMesh->mVertices[vertexIndex].y,
				sourceMesh->mVertices[vertexIndex].z);

			if (sourceMesh->HasNormals())
			{
				vertex.normal = Vector3(
					sourceMesh->mNormals[vertexIndex].x,
					sourceMesh->mNormals[vertexIndex].y,
					sourceMesh->mNormals[vertexIndex].z);
			}

			if (sourceMesh->HasTextureCoords(0))
			{
				vertex.uv = Vector2(
					sourceMesh->mTextureCoords[0][vertexIndex].x,
					sourceMesh->mTextureCoords[0][vertexIndex].y);
			}
		}

		for (unsigned int boneIndex = 0; boneIndex < sourceMesh->mNumBones; ++boneIndex)
		{
			const aiBone* sourceBone = sourceMesh->mBones[boneIndex];
			if (!sourceBone)
			{
				continue;
			}

			const std::string boneName = sourceBone->mName.C_Str();
			const uint32_t storedBoneIndex = FindOrCreateBoneIndex(boneName, bones, boneIndexByName);
			bones[storedBoneIndex].offsetMatrix = ConvertAssimpMatrix(sourceBone->mOffsetMatrix);

			for (unsigned int weightIndex = 0; weightIndex < sourceBone->mNumWeights; ++weightIndex)
			{
				const aiVertexWeight& weight = sourceBone->mWeights[weightIndex];
				if (weight.mVertexId < mesh.vertices.size())
				{
					AddBoneWeight(mesh.vertices[weight.mVertexId], storedBoneIndex, weight.mWeight);
				}
			}
		}

		for (ModelVertex& vertex : mesh.vertices)
		{
			NormalizeBoneWeights(vertex);
		}

		for (unsigned int faceIndex = 0; faceIndex < sourceMesh->mNumFaces; ++faceIndex)
		{
			const aiFace& face = sourceMesh->mFaces[faceIndex];
			if (face.mNumIndices != 3)
			{
				continue;
			}

			mesh.indices.push_back(face.mIndices[0]);
			mesh.indices.push_back(face.mIndices[1]);
			mesh.indices.push_back(face.mIndices[2]);
		}

		if (mesh.vertices.empty() || mesh.indices.empty())
		{
			continue;
		}

		if (FAILED(CreateBuffer(
			device,
			mesh.vertices.data(),
			static_cast<UINT>(mesh.vertices.size() * sizeof(ModelVertex)),
			D3D11_BIND_VERTEX_BUFFER,
			&mesh.vertexBuffer)))
		{
			DebugLog("[Model] Vertex buffer creation failed. Mesh=", meshIndex);
			return false;
		}

		if (FAILED(CreateBuffer(
			device,
			mesh.indices.data(),
			static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t)),
			D3D11_BIND_INDEX_BUFFER,
			&mesh.indexBuffer)))
		{
			DebugLog("[Model] Index buffer creation failed. Mesh=", meshIndex);
			return false;
		}

		meshes.push_back(std::move(mesh));
	}

	for (unsigned int animationIndex = 0; animationIndex < scene->mNumAnimations; ++animationIndex)
	{
		const aiAnimation* sourceAnimation = scene->mAnimations[animationIndex];
		if (!sourceAnimation)
		{
			continue;
		}

		animationClips.push_back(ConvertAnimationClip(*sourceAnimation, bones, boneIndexByName));
	}

	const bool loaded = !meshes.empty();
	DebugLog(
		"[Model] Load result: ",
		loaded ? "success" : "failed",
		" MeshBuffers=",
		meshes.size(),
		" Materials=",
		materials.size(),
		" Bones=",
		bones.size(),
		" Animations=",
		animationClips.size());
	return loaded;
}

const std::vector<ModelMesh>& ModelResource::GetMeshes() const
{
	return meshes;
}

const std::vector<ModelMaterial>& ModelResource::GetMaterials() const
{
	return materials;
}

const std::vector<ModelBone>& ModelResource::GetBones() const
{
	return bones;
}

const std::vector<ModelAnimationClip>& ModelResource::GetAnimationClips() const
{
	return animationClips;
}

bool ModelResourceManager::LoadModel(const std::string& key, const std::string& path, ID3D11Device* device)
{
	if (resources.find(key) != resources.end())
	{
		DebugLog("[Model] Load skipped. Already loaded: ", key);
		return true;
	}

	std::unique_ptr<ModelResource> resource = std::make_unique<ModelResource>();
	if (!resource->LoadFromFile(device, path))
	{
		const std::string solutionRelativePath = "DrectX11Sample/" + path;
		DebugLog("[Model] Retry load: ", solutionRelativePath);
		if (!resource->LoadFromFile(device, solutionRelativePath))
		{
			DebugLog("[Model] LoadModel failed. Key=", key);
			return false;
		}
	}

	resources.emplace(key, std::move(resource));
	DebugLog("[Model] LoadModel success. Key=", key);
	return true;
}

const ModelResource* ModelResourceManager::GetModel(const std::string& key)
{
	const auto found = resources.find(key);
	if (found == resources.end())
	{
		return nullptr;
	}

	return found->second.get();
}

void ModelResourceManager::UnloadAll()
{
	resources.clear();
}
