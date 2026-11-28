#include "Resource/ModelResource.h"

#include <assimp/Importer.hpp>
#include "Core/Debugger.h"
#include "Manager/SceneManager.h"
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <chrono>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

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
		bone.bindLocalMatrix = ConvertAssimpMatrix(node->mTransformation);
		bone.bindLocalMatrix.Decompose(bone.bindLocalScale, bone.bindLocalRotation, bone.bindLocalPosition);
		bone.bindLocalRotation.Normalize();
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

	/// <summary>モデルのマテリアルに対応する画像をデコードする。同一画像は結果内で共有する。</summary>
	/// <param name="scene">Assimpが読み込んだモデルデータ。</param>
	/// <param name="path">外部画像を解決する基準となるモデルのパス。</param>
	/// <param name="materials">マテリアルごとの準備結果。</param>
	void PrepareMaterials(const aiScene &scene, const std::filesystem::path &path,
						  std::vector<PreparedModelMaterial> &materials)
	{
		materials.resize(scene.mNumMaterials);
		const auto fallback = FindDiffuseTextures(path.parent_path());
		std::unordered_map<std::string, std::shared_ptr<const TexturePreparation::Image>> decoded;

		for (unsigned int index = 0; index < scene.mNumMaterials; ++index)
		{
			const aiMaterial *source = scene.mMaterials[index];
			if (!source)
			{
				continue;
			}

			auto &material = materials[index];
			aiString materialName;
			if (source->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS)
			{
				material.satinMetal = std::string(materialName.C_Str()).starts_with("SatinMetal_");
			}

			if (material.satinMetal)
			{
				const auto normalPath = path.parent_path() / (std::string(materialName.C_Str()) + ".normal.png");
				auto normalImage = std::make_shared<TexturePreparation::Image>();
				if (std::filesystem::is_regular_file(normalPath) &&
					SUCCEEDED(TexturePreparation::DecodeFile(normalPath.c_str(), *normalImage)))
				{
					normalImage->format = DXGI_FORMAT_R8G8B8A8_UNORM;
					material.normalImage = std::move(normalImage);
				}
			}

			const auto load = [&](const std::string &key, const aiTexture *embedded)
			{
				if (key.empty())
				{
					return false;
				}

				material.path = key;
				if (const auto found = decoded.find(key); found != decoded.end())
				{
					material.image = found->second;

					return true;
				}

				auto image = std::make_shared<TexturePreparation::Image>();
				HRESULT hr = S_OK;
				if (!embedded)
				{
					const auto widePath = ToWideString(key);
					hr = TexturePreparation::DecodeFile(widePath.c_str(), *image);
				}
				else if (embedded->mHeight == 0)
				{
					hr = TexturePreparation::DecodeMemory(reinterpret_cast<const uint8_t *>(embedded->pcData),
														 embedded->mWidth, *image);
				}
				else if (embedded->pcData && embedded->mWidth &&
						 embedded->mWidth <= D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION &&
						 embedded->mHeight <= D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
				{
					image->width = embedded->mWidth;
					image->height = embedded->mHeight;
					const size_t count = static_cast<size_t>(image->width) * image->height;
					image->pixels.reserve(count * 4);

					for (size_t i = 0; i < count; ++i)
					{
						const auto &pixel = embedded->pcData[i];
						image->pixels.insert(image->pixels.end(), {pixel.r, pixel.g, pixel.b, pixel.a});
					}
				}
				else
				{
					hr = E_INVALIDARG;
				}

				if (FAILED(hr))
				{
					DebugLog("[Model] Texture decode failed: ", key, " hr=", static_cast<long>(hr));

					return false;
				}

				decoded.emplace(key, image);
				material.image = std::move(image);

				return true;
			};

			aiString texturePath;
			if (source->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS)
			{
				const auto *embedded = scene.GetEmbeddedTexture(texturePath.C_Str());
				if ((embedded && load(std::string("embedded:") + texturePath.C_Str(), embedded)) ||
					load(ResolveTexturePath(path, texturePath).string(), nullptr))
				{
					continue;
				}
			}

			aiString name;
			source->Get(AI_MATKEY_NAME, name);
			load(FindFallbackDiffuseTexture(fallback, name.C_Str()).string(), nullptr);
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
	: satinMetal(other.satinMetal)
	, diffuseTexturePath(std::move(other.diffuseTexturePath))
	, diffuseTextureView(other.diffuseTextureView)
	, normalTextureView(std::move(other.normalTextureView))
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
	satinMetal = other.satinMetal;
	diffuseTextureView = other.diffuseTextureView;
	normalTextureView = std::move(other.normalTextureView);
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
	if (!device)
	{
		return false;
	}

	PreparedModelData prepared;
	const auto start = std::chrono::steady_clock::now();
	const bool parsed = PrepareFromFile(path, prepared);
	const auto ready = std::chrono::steady_clock::now();
	const bool loaded = parsed && Upload(device, std::move(prepared));
	const auto end = std::chrono::steady_clock::now();

	using Milliseconds = std::chrono::duration<double, std::milli>;
	DebugLog("[Model] ", path, " CPU ms=", Milliseconds(ready - start).count(),
			 " GPU ms=", Milliseconds(end - ready).count(), " success=", loaded);

	return loaded;
}

/// <summary>モデルと画像をGPUに触れずに読み込む。呼び出しスレッドはCOM初期化済みとする。</summary>
/// <param name="path">モデルのファイルパス。</param>
/// <param name="output">読み込み結果の受け取り先。</param>
/// <returns>有効なメッシュを読み込めた場合はtrue。</returns>
bool ModelResource::PrepareFromFile(const std::string &path, PreparedModelData &output)
{
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

	PreparedModelData prepared;
	PrepareMaterials(*scene, std::filesystem::path(path), prepared.materials);
	auto &bones = prepared.bones;

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

		prepared.meshes.push_back(std::move(mesh));
	}

	for (unsigned int animationIndex = 0; animationIndex < scene->mNumAnimations; ++animationIndex)
	{
		const aiAnimation* sourceAnimation = scene->mAnimations[animationIndex];
		if (!sourceAnimation)
		{
			continue;
		}

		prepared.animationClips.push_back(ConvertAnimationClip(*sourceAnimation, bones, boneIndexByName));
	}

	if (prepared.meshes.empty())
	{
		return false;
	}

	output = std::move(prepared);

	return true;
}

/// <summary>CPU準備済みのモデルをGPUに登録する。</summary>
/// <param name="device">登録先のDirect3Dデバイス。</param>
/// <param name="prepared">準備済みデータ。成功時にメッシュと骨格を移譲する。</param>
/// <returns>描画に必要なバッファを作成できた場合はtrue。</returns>
bool ModelResource::Upload(ID3D11Device *device, PreparedModelData &&prepared)
{
	if (!device || prepared.meshes.empty())
	{
		return false;
	}

	for (auto &mesh : prepared.meshes)
	{
		if (FAILED(CreateBuffer(device, mesh.vertices.data(),
							   static_cast<UINT>(mesh.vertices.size() * sizeof(ModelVertex)),
							   D3D11_BIND_VERTEX_BUFFER, &mesh.vertexBuffer)) ||
			FAILED(CreateBuffer(device, mesh.indices.data(),
							   static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t)),
							   D3D11_BIND_INDEX_BUFFER, &mesh.indexBuffer)))
		{
			DebugLog("[Model] GPU buffer creation failed");

			return false;
		}
	}

	std::vector<ModelMaterial> uploadedMaterials;
	std::unordered_map<const TexturePreparation::Image *, ID3D11ShaderResourceView *> uploaded;
	for (const auto &source : prepared.materials)
	{
		auto &material = uploadedMaterials.emplace_back();
		material.diffuseTexturePath = source.path;
		material.satinMetal = source.satinMetal;
		if (source.normalImage)
		{
			TexturePreparation::Upload(device, *source.normalImage, material.normalTextureView.GetAddressOf());
		}

		if (!source.image)
		{
			continue;
		}

		if (const auto found = uploaded.find(source.image.get()); found != uploaded.end())
		{
			material.diffuseTextureView = found->second;
			material.diffuseTextureView->AddRef();
		}
		else if (SUCCEEDED(TexturePreparation::Upload(device, *source.image, &material.diffuseTextureView)))
		{
			uploaded.emplace(source.image.get(), material.diffuseTextureView);
		}
		else
		{
			DebugLog("[Model] GPU texture creation failed: ", source.path);
		}
	}

	meshes = std::move(prepared.meshes);
	materials = std::move(uploadedMaterials);
	bones = std::move(prepared.bones);
	animationClips = std::move(prepared.animationClips);

	return true;
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
/// ボーン名から ModelResource 内のボーン番号を検索する。
/// </summary>
/// <param name="boneName">検索するボーン名。</param>
/// <returns>見つかったボーン番号。存在しない場合は -1。</returns>
int ModelResource::FindBoneIndex(const std::string& boneName) const
{
	for (size_t index = 0; index < bones.size(); ++index)
	{
		if (bones[index].name == boneName)
		{
			return static_cast<int>(index);
		}
	}

	return -1;
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
	const auto *scene = SceneManager::GetInstance().GetCurrentScene();
	if (scene)
	{
		if (const auto *model = scene->FindSceneModel(key))
		{
			return model;
		}
	}

	const auto found = resources.find(key);
	if (found == resources.end())
	{
		return nullptr;
	}

	return found->second.get();
}

/// <summary>
/// 共通プレイヤーモデルをメインスレッドから登録する。
/// </summary>
/// <param name="key">モデルの共有キー。</param>
/// <param name="model">GPU登録済みのモデル。</param>
void ModelResourceManager::RegisterPreparedPlayer(const std::string &key, std::unique_ptr<ModelResource> model)
{
	resources.try_emplace(key, std::move(model));
}

/// <summary>
/// 読み込み済みのモデルリソースをすべて破棄する。
/// </summary>
void ModelResourceManager::UnloadAll()
{
	resources.clear();
}
