#pragma once

#include "Controller/CustomizeTypes.h"
#include "Data/CharacterData.h"

#include <array>
#include <string>
#include <vector>

/// <summary>
/// カスタマイズ画面の CharacterData 下書き、技スロット割り当て、保存/読込を管理する。
/// </summary>
class CustomizeCharacterEditorController
{
public:
	static constexpr int CharacterSlotCount = 10;
	static constexpr int AttackButtonSlotCount = 4;
	static constexpr int CharacterNameBufferSize = 128;

	/// <summary>
	/// キャラクタースロット選択画面を描画する。
	/// </summary>
	/// <param name="statusMessage">画面に表示する処理結果メッセージ。</param>
	/// <returns>次に表示する CustomizeMode。</returns>
	CustomizeMode DrawSlotSelect(std::string& statusMessage);

	/// <summary>
	/// キャラクター名と各攻撃ボタンの技割り当て編集画面を描画する。
	/// </summary>
	/// <param name="statusMessage">画面に表示する処理結果メッセージ。</param>
	/// <returns>次に表示する CustomizeMode。</returns>
	CustomizeMode DrawEditor(std::string& statusMessage);

	/// <summary>
	/// 現在選択中のキャラクター側スロットへ割り当てる AttackData 候補を描画する。
	/// </summary>
	/// <param name="statusMessage">画面に表示する処理結果メッセージ。</param>
	/// <returns>次に表示する CustomizeMode。</returns>
	CustomizeMode DrawAttackPicker(std::string& statusMessage);

	/// <summary>
	/// キャラクタースロット番号から CharacterData フォルダ名に使う ID を作る。
	/// </summary>
	/// <param name="slotIndex">キャラクタースロット番号。</param>
	/// <returns>CharacterSlot00 のような固定桁の ID。</returns>
	std::string BuildCharacterId(int slotIndex) const;

	/// <summary>
	/// キャラクタースロット番号から CharacterData 保存フォルダを作る。
	/// </summary>
	/// <param name="slotIndex">キャラクタースロット番号。</param>
	/// <returns>assets/CharacterData 配下の保存フォルダパス。</returns>
	std::string BuildCharacterFolderPath(int slotIndex) const;

	/// <summary>
	/// キャラクタースロット一覧に表示するボタンラベルを作る。
	/// </summary>
	/// <param name="slotIndex">キャラクタースロット番号。</param>
	/// <returns>スロット番号、保存済み名、ImGui ID を含むラベル。</returns>
	std::string BuildSlotButtonLabel(int slotIndex) const;

	/// <summary>
	/// 選択したキャラクタースロットを編集用 draft に読み込む。
	/// </summary>
	/// <param name="slotIndex">編集するキャラクタースロット番号。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string SelectSlot(int slotIndex);

	/// <summary>
	/// キャラクター編集バッファの内容を CharacterData JSON として保存する。
	/// </summary>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string SaveDraft();

	/// <summary>
	/// CharacterData フォルダを確認し、キャラクタースロット一覧表示用の名前を更新する。
	/// </summary>
	void RefreshSlotSummaries();

	/// <summary>
	/// AttackData が選択中のキャラクター側スロットへ割り当て可能か確認する。
	/// </summary>
	/// <param name="group">割り当て先のキャラクター側スロットグループ。</param>
	/// <param name="attackData">確認する AttackData。</param>
	/// <returns>通常/必殺、地上/空中の条件を満たす場合は true。</returns>
	bool IsAttackCompatibleWithSlotGroup(
		CustomizeCharacterAttackSlotGroup group,
		const AttackData& attackData) const;

	/// <summary>
	/// 指定グループのキャラクター側スロット draft 配列を取得する。
	/// </summary>
	/// <param name="group">取得するスロットグループ。</param>
	/// <returns>変更可能な draft 配列。</returns>
	std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount>& GetAttackSlotDrafts(
		CustomizeCharacterAttackSlotGroup group);

	/// <summary>
	/// 指定グループのキャラクター側スロット draft 配列を読み取り専用で取得する。
	/// </summary>
	/// <param name="group">取得するスロットグループ。</param>
	/// <returns>読み取り専用の draft 配列。</returns>
	const std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount>& GetAttackSlotDrafts(
		CustomizeCharacterAttackSlotGroup group) const;

	/// <summary>
	/// 編集中のキャラクターパラメータを取得する。
	/// </summary>
	/// <returns>読み取り専用の CharacterParameterData。</returns>
	const CharacterParameterData& GetDraftParameter() const;

	/// <summary>
	/// キャラクター名入力用バッファを取得する。
	/// </summary>
	/// <returns>ImGui InputText に渡す固定長バッファ。</returns>
	std::array<char, CharacterNameBufferSize>& GetCharacterNameBuffer();

	/// <summary>
	/// AttackPicker の割り当て先スロットグループを設定する。
	/// </summary>
	/// <param name="group">割り当て先グループ。</param>
	void SetPickingSlotGroup(CustomizeCharacterAttackSlotGroup group);

	/// <summary>
	/// AttackPicker の割り当て先スロット番号を設定する。
	/// </summary>
	/// <param name="slotIndex">割り当て先スロット番号。</param>
	void SetPickingSlotIndex(int slotIndex);

	/// <summary>
	/// AttackPicker の割り当て先スロットグループを取得する。
	/// </summary>
	/// <returns>割り当て先グループ。</returns>
	CustomizeCharacterAttackSlotGroup GetPickingSlotGroup() const;

	/// <summary>
	/// AttackPicker の割り当て先スロット番号を取得する。
	/// </summary>
	/// <returns>割り当て先スロット番号。</returns>
	int GetPickingSlotIndex() const;

private:
	/// <summary>
	/// キャラクター側の固定ボタンスロットを初期化する。
	/// </summary>
	/// <param name="group">地上通常技、空中通常技、必殺技のどれか。</param>
	/// <param name="slotIndex">ABXY の何番目か。</param>
	/// <returns>未割り当て状態のスロット draft。</returns>
	CustomizeCharacterAttackSlotDraft CreateAttackSlotDraft(
		CustomizeCharacterAttackSlotGroup group,
		int slotIndex) const;

	/// <summary>
	/// キャラクター draft から保存用 CharacterAttackSlotData 配列を作る。
	/// </summary>
	/// <returns>AttackList.json に保存する技スロット情報。</returns>
	std::vector<CharacterAttackSlotData> BuildAttackSlotsForSave() const;

	/// <summary>
	/// 地上通常技と空中通常技の必須スロットが埋まっているか確認する。
	/// </summary>
	/// <param name="outMissingSlotName">未設定スロットがある場合の表示名。</param>
	/// <returns>必須スロットが全て埋まっている場合は true。</returns>
	bool AreRequiredAttackSlotsFilled(std::string& outMissingSlotName) const;

	/// <summary>
	/// draftParameter のキャラクター名を ImGui 入力用固定バッファへコピーする。
	/// </summary>
	void CopyCharacterNameToBuffer();

	/// <summary>
	/// キャラクター draft 内の attackDataId から表示名を読み直す。
	/// </summary>
	void RefreshAttackSlotNames();

	/// <summary>
	/// 指定したキャラクター側スロットグループの割り当て一覧を描画する。
	/// </summary>
	/// <param name="group">描画するスロットグループ。</param>
	/// <param name="label">ImGui に表示するグループ名。</param>
	bool DrawAttackSlotGroup(CustomizeCharacterAttackSlotGroup group, const char* label);

	/// <summary>
	/// AttackPicker で選んだ技を、現在選択中のキャラクター側スロットへ割り当てる。
	/// </summary>
	/// <param name="attackDataId">割り当てる AttackData ID。</param>
	/// <param name="attackData">表示名確認用に読み込み済みの AttackData。</param>
	void AssignAttackToCurrentSlot(const std::string& attackDataId, const AttackData& attackData);

	CharacterParameterData draftParameter;
	std::array<char, CharacterNameBufferSize> characterNameBuffer = {};
	std::array<CustomizeCharacterSlotSummary, CharacterSlotCount> slotSummaries = {};
	std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount> groundAttackSlotDrafts = {};
	std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount> airAttackSlotDrafts = {};
	std::array<CustomizeCharacterAttackSlotDraft, AttackButtonSlotCount> specialAttackSlotDrafts = {};
	CustomizeCharacterAttackSlotGroup pickingSlotGroup = CustomizeCharacterAttackSlotGroup::Ground;
	int selectedSlotIndex = 0;
	int pickingSlotIndex = 0;
	std::string editingFolderPath;
};
