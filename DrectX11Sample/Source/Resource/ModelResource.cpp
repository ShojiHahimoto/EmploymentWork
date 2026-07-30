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
	/// <summary>
	/// DirectX11 Buffer を解放し、ポインタを nullptr に戻す。
	/// </summary>
	/// <param name="buffer">解放する Buffer ポインタ。</param>
	void ReleaseBuffer(ID3D11Buffer*& buffer)
	{
		if (buffer)
		{
			buffer->Release();
			buffer = nullptr;
		}
	}

	/// <summary>
	/// ShaderResourceView を解放し、ポインタを nullptr に戻す。
	/// </summary>
	/// <param name="textureView">解放する ShaderResourceView ポインタ。</param>
	void ReleaseTexture(ID3D11ShaderResourceView*& textureView)
	{
		if (textureView)
		{
			textureView->Release();
			textureView = nullptr;
		}
	}

	/// <summary>
	/// std::string のパス文字列を WIC 読み込み用の std::wstring に変換する。
	/// </summary>
	/// <param name="text">変換する文字列。</param>
	/// <returns>wstring へ変換した文字列。</returns>
	std::wstring ToWideString(const std::string& text)
	{
		return std::wstring(text.begin(), text.end());
	}

	/// <summary>
	/// Assimp の行列を SimpleMath の Matrix に変換する。
	/// </summary>
	/// <param name="source">変換元の Assimp 行列。</param>
	/// <returns>SimpleMath の Matrix。</returns>
	Matrix ConvertAssimpMatrix(const aiMatrix4x4& source)
	{
		return Matrix(
			source.a1, source.b1, source.c1, source.d1,
			source.a2, source.b2, source.c2, source.d2,
			source.a3, source.b3, source.c3, source.d3,
			source.a4, source.b4, source.c4, source.d4);
	}

	/// <summary>
	/// Assimp の 3D ベクトルを SimpleMath の Vector3 に変換する。
	/// </summary>
	/// <param name="source">変換元の Assimp ベクトル。</param>
	/// <returns>SimpleMath の Vector3。</returns>
	Vector3 ConvertAssimpVector(const aiVector3D& source)
	{
		return Vector3(source.x, source.y, source.z);
	}

	/// <summary>
	/// Assimp の Quaternion を SimpleMath の Quaternion に変換する。
	/// </summary>
	/// <param name="source">変換元の Assimp Quaternion。</param>
	/// <returns>SimpleMath の Quaternion。</returns>
	Quaternion ConvertAssimpQuaternion(const aiQuaternion& source)
	{
		return Quaternion(source.x, source.y, source.z, source.w);
	}

	/// <summary>
	/// 頂点に最大 4 本までのボーンウェイトを追加する。
	/// </summary>
	/// <param name="vertex">ボーン情報を書き込む頂点。</param>
	/// <param name="boneIndex">追加するボーンのインデックス。</param>
	/// <param name="weight">追加するウェイト値。</param>
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

	/// <summary>
	/// 頂点のボーンウェイト合計が 1 になるように正規化する。
	/// </summary>
	/// <param name="vertex">正規化する頂点。</param>
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

	/// <summary>
	/// Assimp ノード階層を辿り、将来のスケルタルアニメーション用ボーン配列を作る。
	/// </summary>
	/// <param name="node">現在処理する Assimp ノード。</param>
	/// <param name="parentIndex">親ボーンのインデックス。root は -1。</param>
	/// <param name="bones">生成したボーン情報を蓄積する配列。</param>
	/// <param name="boneIndexByName">ボーン名からインデックスを引くための表。</param>
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

	/// <summary>
	/// 頂点やインデックス用の DirectX11 Buffer を作成する。
	/// </summary>
	/// <param name="device">Buffer を作成する DirectX11 Device。</param>
	/// <param name="sourceData">Buffer に書き込む初期データ。</param>
	/// <param name="byteWidth">Buffer のバイトサイズ。</param>
	/// <param name="bindFlags">D3D11_BIND_VERTEX_BUFFER などの用途フラグ。</param>
	/// <param name="buffer">作成した Buffer の受け取り先。</param>
	/// <returns>DirectX API の HRESULT。</returns>
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

	/// <summary>
	/// モデルファイル基準でテクスチャパスを解決する。
	/// </summary>
	/// <param name="modelPath">読み込んでいるモデルファイルのパス。</param>
	/// <param name="texturePath">Assimp Material が持つテクスチャパス。</param>
	/// <returns>実際に読み込むテクスチャパス。</returns>
	std::filesystem::path ResolveTexturePath(const std::filesystem::path& modelPath, const aiString& texturePath)
	{
		std::filesystem::path sourcePath(texturePath.C_Str());
		if (sourcePath.is_absolute())
		{
			return sourcePath;
		}

		return modelPath.parent_path() / sourcePath.filename();
	}

	/// <summary>
	/// モデルと同じディレクトリから diffuse PNG テクスチャ候補を探す。
	/// </summary>
	/// <param name="modelDirectory">探索するモデルディレクトリ。</param>
	/// <returns>見つかった diffuse テクスチャ候補。</returns>
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

	/// <summary>
	/// Material 名に近い diffuse テクスチャを候補一覧から選ぶ。
	/// </summary>
	/// <param name="diffuseTextures">同階層から見つけた diffuse テクスチャ候補。</param>
	/// <param name="materialName">Material 名。</param>
	/// <returns>選択したテクスチャパス。候補がなければ空パス。</returns>
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

	/// <summary>
	/// diffuse テクスチャを読み込み、Material に ShaderResourceView を設定する。
	/// </summary>
	/// <param name="device">テクスチャ作成に使う DirectX11 Device。</param>
	/// <param name="material">テクスチャ情報を書き込む Material。</param>
	/// <param name="texturePath">読み込むテクスチャパス。</param>
	/// <param name="textureCache">同一テクスチャを共有するためのキャッシュ。</param>
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

	/// <summary>
	/// Assimp Scene の Material 情報から、描画用 ModelMaterial 配列を作る。
	/// </summary>
	/// <param name="device">テクスチャ作成に使う DirectX11 Device。</param>
	/// <param name="scene">Material を含む Assimp Scene。</param>
	/// <param name="modelPath">読み込んでいるモデルファイルのパス。</param>
	/// <param name="materials">作成した Material を書き込む配列。</param>
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

	/// <summary>
	/// ボーン名に対応するインデックスを取得し、未登録なら新規ボーンとして追加する。
	/// </summary>
	/// <param name="boneName">検索または追加するボーン名。</param>
	/// <param name="bones">ボーン情報を保持する配列。</param>
	/// <param name="boneIndexByName">ボーン名からインデックスを引くための表。</param>
	/// <returns>ボーン名に対応するインデックス。</returns>
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

	/// <summary>
	/// Assimp の Animation を、エンジン側の ModelAnimationClip に変換する。
	/// </summary>
	/// <param name="sourceAnimation">変換元の Assimp Animation。</param>
	/// <param name="bones">アニメーション対象ボーンを保持する配列。</param>
	/// <param name="boneIndexByName">ボーン名からインデックスを引くための表。</param>
	/// <returns>変換した ModelAnimationClip。</returns>
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

/// <summary>
/// ModelMesh が保持する DirectX11 Buffer を解放する。
/// </summary>
ModelMesh::~ModelMesh()
{
	ReleaseBuffer(indexBuffer);
	ReleaseBuffer(vertexBuffer);
}

/// <summary>
/// ModelMesh の所有する Buffer と配列をムーブする。
/// </summary>
/// <param name="other">ムーブ元の ModelMesh。</param>
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

/// <summary>
/// 既存 Buffer を解放し、別の ModelMesh から描画リソースをムーブ代入する。
/// </summary>
/// <param name="other">ムーブ元の ModelMesh。</param>
/// <returns>ムーブ後の自分自身。</returns>
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

/// <summary>
/// ModelMaterial が保持するテクスチャ View を解放する。
/// </summary>
ModelMaterial::~ModelMaterial()
{
	ReleaseTexture(diffuseTextureView);
}

/// <summary>
/// ModelMaterial のテクスチャ参照をムーブする。
/// </summary>
/// <param name="other">ムーブ元の ModelMaterial。</param>
ModelMaterial::ModelMaterial(ModelMaterial&& other) noexcept
	: diffuseTexturePath(std::move(other.diffuseTexturePath))
	, diffuseTextureView(other.diffuseTextureView)
{
	other.diffuseTextureView = nullptr;
}

/// <summary>
/// 既存テクスチャ参照を解放し、別の ModelMaterial からテクスチャ参照をムーブ代入する。
/// </summary>
/// <param name="other">ムーブ元の ModelMaterial。</param>
/// <returns>ムーブ後の自分自身。</returns>
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

/// <summary>
/// FBX などのモデルファイルを Assimp で読み込み、メッシュ、Material、ボーン、アニメーション情報を作成する。
/// </summary>
/// <param name="device">描画 Buffer とテクスチャ作成に使う DirectX11 Device。</param>
/// <param name="path">読み込むモデルファイルのパス。</param>
/// <returns>描画可能なメッシュを 1 つ以上読み込めた場合は true。</returns>
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

/// <summary>
/// ModelResource が保持する描画用 Mesh 配列を取得する。
/// </summary>
/// <returns>読み取り専用の Mesh 配列。</returns>
const std::vector<ModelMesh>& ModelResource::GetMeshes() const
{
	return meshes;
}

/// <summary>
/// ModelResource が保持する Material 配列を取得する。
/// </summary>
/// <returns>読み取り専用の Material 配列。</returns>
const std::vector<ModelMaterial>& ModelResource::GetMaterials() const
{
	return materials;
}

/// <summary>
/// ModelResource が保持するボーン配列を取得する。
/// </summary>
/// <returns>読み取り専用のボーン配列。</returns>
const std::vector<ModelBone>& ModelResource::GetBones() const
{
	return bones;
}

/// <summary>
/// ModelResource が保持するアニメーションクリップ配列を取得する。
/// </summary>
/// <returns>読み取り専用のアニメーションクリップ配列。</returns>
const std::vector<ModelAnimationClip>& ModelResource::GetAnimationClips() const
{
	return animationClips;
}

/// <summary>
/// 指定キーでモデルリソースを読み込み、既に読み込み済みなら再利用する。
/// </summary>
/// <param name="key">ResourceManager に登録するモデルキー。</param>
/// <param name="path">読み込むモデルファイルのパス。</param>
/// <param name="device">描画 Buffer とテクスチャ作成に使う DirectX11 Device。</param>
/// <returns>読み込みまたは再利用に成功した場合は true。</returns>
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

/// <summary>
/// 登録済みモデルリソースをキーで取得する。
/// </summary>
/// <param name="key">取得するモデルキー。</param>
/// <returns>見つかった ModelResource。存在しない場合は nullptr。</returns>
const ModelResource* ModelResourceManager::GetModel(const std::string& key)
{
	const auto found = resources.find(key);
	if (found == resources.end())
	{
		return nullptr;
	}

	return found->second.get();
}

/// <summary>
/// 読み込み済みのモデルリソースをすべて破棄する。
/// </summary>
void ModelResourceManager::UnloadAll()
{
	resources.clear();
}
