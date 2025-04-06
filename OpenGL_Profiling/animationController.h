#pragma once

#include "model.h"

struct AnimationBlend
{
	std::string A, B;

	float blendFactor = 0.0f;
	
	float duration = 0.0;
	float elapsed = 0.0f;

	bool loop = true;
	bool fit = true; // will stretch all animations to the longest one, otherwise they will wait till it's finished
};

// Class for controlling and managing animation blending
class AnimationController
{
public:
	AnimationController(std::shared_ptr<Model> model);

	void advance(float dt);

	void selectAnimation(const std::string& name, float transition = 0.2f, float lockstep = false);

	const std::string& getCurrentAnimation() const { return currentAnimation; }
	const std::string& getNextAnimation() const { return nextAnimation; }

	const float getTrasitionElapsed() const { return transitionElapsed; }
	const float getTrasitionDuration() const { return transitionDuration; }

	const glm::vec3 getVelocity() const;

public:
	void addBlend(const std::string& name, const std::string& A, const std::string& B, 
		bool loop = true, bool fit = true);
	AnimationBlend& getBlend(const std::string& name);

private:
	std::shared_ptr<Model> model;

	std::string currentAnimation;
	std::string nextAnimation;

	float transitionDuration;
	float transitionElapsed;
	float transitionRatio;
	bool inTransition;

private:
	std::unordered_map<std::string, AnimationBlend> blends;
	std::unordered_map<std::string, Animation> animations;

private:
	std::unordered_map<int, TransformOffset> blendOffsets(
		const std::unordered_map<int, TransformOffset>& a,
		const std::unordered_map<int, TransformOffset>& b, float factor);

private:
	void advanceBlend(AnimationBlend& blend, float dt);
	void advanceAnimation(Animation& anim, float dt);

	std::unordered_map<int, TransformOffset> getBlendFrame(const AnimationBlend& blend, float t);
	std::unordered_map<int, TransformOffset> getAnimationFrame(const Animation& anim, float t);

	const glm::vec3 getBlendVelocity(const AnimationBlend& blend) const;
};