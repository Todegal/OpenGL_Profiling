#include "animationController.h"

#include <spdlog/spdlog.h>

AnimationController::AnimationController(std::shared_ptr<Model> model)
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
	// If a transition is finished then update
	if (transitionElapsed >= transitionDuration && inTransition)
	{
		currentAnimation = nextAnimation;
		transitionElapsed = 0.0f;
		transitionDuration = 0.0f;
		transitionRatio = 0.0f;
		inTransition = false;

		currentAnimation = nextAnimation;
		nextAnimation = "";
	}
	else if (inTransition)
	{
		transitionRatio = transitionElapsed / transitionDuration;
		transitionElapsed += dt;
	}

	if (currentAnimation == "")
		return;

	// Find offsets for current animation
	std::unordered_map<int, TransformOffset> offsets;

	if (blends.find(currentAnimation) != blends.end())
	{
		auto& blend = blends[currentAnimation];

		advanceBlend(blend, dt);

		offsets = getBlendFrame(blend, blend.elapsed);
	}
	else if (animations.find(currentAnimation) != animations.end())
	{
		auto& anim = animations[currentAnimation];

		advanceAnimation(anim, dt);

		offsets = getAnimationFrame(anim, anim.elapsed);
	}
	else
	{
		spdlog::error("Invalid animation or blend: {}", currentAnimation);
	}

	// Find offsets for the next animation or blend if it exists
	if (inTransition && nextAnimation != "")
	{
		std::unordered_map<int, TransformOffset> nextOffsets;

		if (blends.find(nextAnimation) != blends.end())
		{
			auto& blend = blends[nextAnimation];

			advanceBlend(blend, dt);

			nextOffsets = getBlendFrame(blend, blend.elapsed);

			offsets = blendOffsets(offsets, nextOffsets, transitionRatio);
		}
		else if (animations.find(nextAnimation) != animations.end())
		{
			auto& anim = animations[nextAnimation];

			advanceAnimation(anim, dt);

			nextOffsets = getAnimationFrame(anim, anim.elapsed);

			offsets = blendOffsets(offsets, nextOffsets, transitionRatio);
		}
		else
		{
			spdlog::error("Invalid animation or blend: {}", nextAnimation);
		}
	}

	model->setJoints(offsets);
}

void AnimationController::selectAnimation(const std::string& name, float transition, float lockstep)
{
	if (blends.find(name) != blends.end())
	{
		nextAnimation = name;
	}
	else if (animations.find(name) != animations.end())
	{
		nextAnimation = name;
	}
	else
	{
		spdlog::error("Invalid animation or blend: {}", currentAnimation);
	}

	if (currentAnimation != name && !inTransition)
	{
		transitionDuration = transition;
		transitionElapsed = 0.0f;
		transitionRatio = 0.0f;
		inTransition = true;
	}
}

const glm::vec3 AnimationController::getVelocity() const
{
	glm::vec3 v = glm::vec3(0.0f);

	if (blends.find(currentAnimation) != blends.end())
	{
		v = getBlendVelocity(blends.at(currentAnimation));
	}
	else if (animations.find(currentAnimation) != animations.end())
	{
		v = animations.at(currentAnimation).velocity;
	}
	else
	{
		spdlog::error("Invalid animation or blend: {}", currentAnimation);
	}

	glm::vec3 u = v;

	if (inTransition)
	{
		if (blends.find(nextAnimation) != blends.end())
		{
			u = getBlendVelocity(blends.at(nextAnimation));
		}
		else if (animations.find(nextAnimation) != animations.end())
		{
			u = animations.at(nextAnimation).velocity;
		}
		else
		{
			spdlog::error("Invalid animation or blend: {}", currentAnimation);
		}

		v = glm::mix(v, u, transitionRatio);
	}

	return v;
}

void AnimationController::addBlend(const std::string& name, const std::string& A, const std::string& B, bool loop, bool fit)
{
	if (animations.find(A) == animations.end() ||
		animations.find(B) == animations.end())
	{
		spdlog::warn("Invalid blend: {}, {}", A, B);
	}

	const Animation& animA = animations[A];
	const Animation& animB = animations[B];

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
	return blends[name];
}

std::unordered_map<int, TransformOffset> AnimationController::blendOffsets(
	const std::unordered_map<int, TransformOffset>& a, const std::unordered_map<int, TransformOffset>& b, float factor)
{
	std::unordered_map<int, TransformOffset> result;

	for (const auto& [i, offset] : a)
	{
		result[i].rotation = glm::slerp(offset.rotation, b.at(i).rotation, factor);
		
		result[i].translation = glm::mix(offset.translation, b.at(i).translation, factor);
		
		result[i].scale = glm::mix(offset.scale, b.at(i).scale, factor);
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
	if (animations.find(blend.A) == animations.end() ||
		animations.find(blend.B) == animations.end())
	{
		spdlog::warn("Invalid blend: {}, {}", blend.A, blend.B);
	}

	const Animation& animA = animations[blend.A];
	const Animation& animB = animations[blend.B];

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
			difference = std::min(t - *previousFrame, 0.0f) / frameDifference;
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

	if (a != animations.end() && b != animations.end()) return glm::mix(a->second.velocity, b->second.velocity, blend.blendFactor);
	
	return glm::vec3(0.0f);
}

