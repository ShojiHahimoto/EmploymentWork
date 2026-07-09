#pragma once

class Renderer;
class World;

class Scene
{
public:
	virtual ~Scene() = default;

	virtual void Enter() = 0;
	virtual void Exit() = 0;

	// Scene 自体にゲームロジックを持たせず、System 群を固定順で実行する入口。
	virtual void RunSystems() = 0;
	virtual void Draw(Renderer& renderer) = 0;
	virtual void OnResize(int width, int height) = 0;

	virtual World& GetWorld() = 0;
	virtual const World& GetWorld() const = 0;
};
