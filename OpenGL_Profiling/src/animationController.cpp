#include "animationController.h"

#include <spdlog/spdlog.h>

AnimationController::AnimationController()
	: model(nullptr)
{
}

AnimationController::AnimationController(std::shared_ptr<RenderableModel> model)
	: model(model)
{
	animations = model->getAnimations();

	currentAnimation = "";
	nextAnimation = "";

	transitionDuration = 0.0f;
	transitionElapsed = 0.0f;
	transitionRatio = 0.0f;
	inTransition = false;
}

void AnimationController::advance(float dt)
{
	if (!inTransition && currentAnimation == "") return;

	if (inTransition)
	{
		transitionElapsed += dt;
		transitionRatio = transitionDuration > 0.0f
			? std::clamp(transitionElapsed / transitionDuration, 0.0f, 1.0f)
			: 0.0f;

		if (transitionElapsed >= transitionDuration)
		{
			// Finish transition
			currentAnimation = nextAnimation;
			nextAnimation = "";

			inTransition = false;
			transitionElapsed = 0.0f;
			transitionDuration = 0.0f;
			transitionRatio = 0.0f;
		}
	}

	// Find offsets for current animation
	std::unordered_map<int, TransformOffset> offsets;

	if (blends.contains(currentAnimation))
	{
		auto& blend = blends.at(currentAnimation);

		advanceBlend(blend, dt);

		offsets = getBlendFrame(blend, blend.elapsed);
	}
	else
	{
		auto& anim = animations.at(currentAnimation);

		advanceAnimation(anim, dt);

		offsets = getAnimationFrame(anim, anim.elapsed);
	}

	// Find offsets for the next animation or blend if it exists
	if (inTransition && nextAnimation != "")
	{
		std::unordered_map<int, TransformOffset> nextOffsets;

		if (blends.contains(nextAnimation))
		{
			auto& blend = blends.at(nextAnimation);

			advanceBlend(blend, dt);

			nextOffsets = getBlendFrame(blend, blend.elapsed);

			offsets = blendOffsets(offsets, nextOffsets, transitionRatio);
		}
		else
		{
			auto& anim = animations.at(nextAnimation);

			advanceAnimation(anim, dt);

			nextOffsets = getAnimationFrame(anim, anim.elapsed);

			offsets = blendOffsets(offsets, nextOffsets, transitionRatio);
		}
	}

	model->setJoints(offsets);
}

void AnimationController::selectAnimation(const std::string& name, float transition, bool lockstep)
{
	if (nextAnimation == name || currentAnimation == name) return;
	
	float elapsedRatio = 0.0f;

	if (lockstep && currentAnimation != "")
	{
		if (blends.contains(currentAnimation))
		{
			const auto& b = blends.at(currentAnimation);
			elapsedRatio = b.elapsed / b.duration;
		}
		else
		{
			const auto& curAnim = animations.at(currentAnimation);
			elapsedRatio = curAnim.elapsed / curAnim.duration;
		}
	}

	if (blends.contains(name))
	{
		auto& b = blends.at(name);
		b.elapsed = b.duration * elapsedRatio;
	}
	else if (animations.contains(name))
	{
		auto& a = animations.at(name);
		a.elapsed = a.duration * elapsedRatio;
	}
	else
	{
		spdlog::error("Invalid animation or blend: {}", currentAnimation);
	}

	nextAnimation = name;

	if (currentAnimation != name)
	{
		transitionDuration = transition;
		transitionElapsed = 0.0f;
		transitionRatio = 0.0f;
		inTransition = true;
	}
}

const glm::vec3 AnimationController::getVelocity() const
{
	if (currentAnimation == "") return glm::vec3(0.0f);

	glm::vec3 v = glm::vec3(0.0f);

	if (blends.contains(currentAnimation))
	{
		v = getBlendVelocity(blends.at(currentAnimation));
	}
	else
	{
		v = animations.at(currentAnimation).velocity;
	}

	glm::vec3 u = v;

	if (inTransition)
	{
		if (blends.contains(nextAnimation))
		{
			u = getBlendVelocity(blends.at(nextAnimation));
		}
		else
		{
			u = animations.at(nextAnimation).velocity;
		}

		v = glm::mix(v, u, transitionRatio);
	}

	return v;
}

void AnimationController::addBlend(const std::string& name, const std::string& A, const std::string& B, bool loop, bool fit)
{
	auto aIt = animations.find(A);
	auto bIt = animations.find(B);
	
	if (aIt == animations.end())
	{
		spdlog::error("Invalid animation: {}; unable to add blend: {}", A, name);
		return;
	}

	if (bIt == animations.end())
	{
		spdlog::error("Invalid animation: {}; unable to add blend: {}", B, name);
		return;
	}

	const Animation& animA = aIt->second;
	const Animation& animB = bIt->second;

	AnimationBlend blend;
	blend.A = A;
	blend.B = B;

	blend.blendFactor = 0.0f;
	blend.duration = std::fmax(animA.duration, animB.duration);
	blend.elapsed = 0.0f;

	blends[name] = blend;
}

AnimationBlend& AnimationController::getBlend(const std::string& name)
{
	return blends.at(name);
}

Animation& AnimationController::getAnimation(const std::string& name)
{
	return animations.at(name);
}

std::unordered_map<int, TransformOffset> AnimationController::blendOffsets(
	const std::unordered_map<int, TransformOffset>& a, const std::unordered_map<int, TransformOffset>& b, float factor)
{
	std::unordered_map<int, TransformOffset> result;

	for (const auto& [i, offset] : a)
	{
		auto bIt = b.find(i);
		if (bIt == b.end()) continue;

		result[i].rotation = glm::slerp(offset.rotation, bIt->second.rotation, factor);
		
		result[i].translation = glm::mix(offset.translation, bIt->second.translation, factor);
		
		result[i].scale = glm::mix(offset.scale, bIt->second.scale, factor);
	}

	return result;
}

void AnimationController::advanceBlend(AnimationBlend& blend, float dt)
{
	if (blend.loop && blend.elapsed >= (blend.duration - glm::epsilon<float>())) blend.elapsed -= blend.duration;
	blend.elapsed += dt;
}

void AnimationController::advanceAnimation(Animation& anim, float dt)
{
	if (anim.loop && anim.elapsed >= (anim.duration - glm::epsilon<float>())) anim.elapsed -= anim.duration;
	anim.elapsed += dt;
}

std::unordered_map<int, TransformOffset> AnimationController::getBlendFrame(const AnimationBlend& blend, float t)
{
	const Animation& animA = animations.at(blend.A);
	const Animation& animB = animations.at(blend.B);

	float stretchA = 1.0f;
	float stretchB = 1.0f;

	if (blend.fit)
	{
		float tMax = blend.duration;

		stretchA = animA.duration / tMax;
		stretchB = animB.duration / tMax;
	}

	const auto& offsetsA = getAnimationFrame(animA, blend.elapsed * stretchA);
	const auto& offsetsB = getAnimationFrame(animB, blend.elapsed * stretchB);

	auto offsets = blendOffsets(offsetsA, offsetsB, blend.blendFactor);

	return offsets;
}

std::unordered_map<int, TransformOffset> AnimationController::getAnimationFrame(const Animation& anim, float t)
{
	std::unordered_map<int, TransformOffset> nodeOffsets;

	for (const auto& channel : anim.channels)
	{
		auto nextFrame = std::upper_bound(channel.keyframeTimes.begin(), channel.keyframeTimes.end(), t);
		if (nextFrame == channel.keyframeTimes.end()) {
			// If elapsed time is beyond any value, both iterators point to the last frame
			nextFrame = std::prev(channel.keyframeTimes.end());
		}

		auto previousFrame = (nextFrame == channel.keyframeTimes.begin())
			? nextFrame // If elapsed time is before any value, both iterators point to the first frame
			: std::prev(nextFrame);

		float frameDifference = (*nextFrame - *previousFrame);
		float difference = 0.0f;

		if (frameDifference > 0.0f)
		{
			difference = std::max(t - *previousFrame, 0.0f) / frameDifference;
		}

		int component = 3;
		if (channel.path == "rotation") component = 4;

		const auto previousValuePtr = channel.values.begin() + ((previousFrame - channel.keyframeTimes.begin()) * component);
		const auto nextValuePtr = channel.values.begin() + ((nextFrame - channel.keyframeTimes.begin()) * component);

		if (channel.path == "rotation")
		{
			glm::quat previousR = {
				*(previousValuePtr + 3), // W -- I hate this :,(
				*previousValuePtr, // X
				*(previousValuePtr + 1), // Y
				*(previousValuePtr + 2) // Z
			};

			glm::quat nextR = {
				*(nextValuePtr + 3), // W
				*nextValuePtr, // X
				*(nextValuePtr + 1), // Y
				*(nextValuePtr + 2) // Z
			};

			glm::quat newR = glm::slerp(previousR, nextR, difference);

			nodeOffsets[channel.target].rotation = newR;
		}
		else
		{
			glm::vec3 previous = {
				*previousValuePtr,
				*(previousValuePtr + 1),
				*(previousValuePtr + 2),
			};

			glm::vec3 next = {
				*nextValuePtr,
				*(nextValuePtr + 1),
				*(nextValuePtr + 2),
			};

			glm::vec3 newVector = glm::mix(previous, next, difference);

			if (channel.path == "translation")
			{
				nodeOffsets[channel.target].translation = newVector;
			}
			else
			{
				nodeOffsets[channel.target].scale = newVector;
			}
		}
	}

	return nodeOffsets;
}

const glm::vec3 AnimationController::getBlendVelocity(const AnimationBlend& blend) const
{
	auto a = animations.find(blend.A);
	auto b = animations.find(blend.B);

	if (a == animations.end() || b == animations.end()) return glm::vec3(0.0f);

	glm::vec3 c = glm::normalize(glm::mix(a->second.velocity, b->second.velocity, blend.blendFactor));
	c *= glm::mix(glm::length(a->second.velocity), glm::length(b->second.velocity), blend.blendFactor);

	return c;
}

