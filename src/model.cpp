#include "model.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/spdlog.h>

#include <iostream>
#include <execution>
#include <span>

RawModel::RawModel(std::filesystem::path gltfPath, std::string rootNode)
	: rootNode(rootNode)
{
	model.emplace();

	tinygltf::TinyGLTF loader;

	std::string error;
	std::string warning;

	bool ret = false;
	if (gltfPath.extension() == ".glb")
		ret = loader.LoadBinaryFromFile(&model.value(), &error, &warning, gltfPath.string());
	else
		ret = loader.LoadASCIIFromFile(&model.value(), &error, &warning, gltfPath.string());

	if (!warning.empty())
	{
		spdlog::warn(warning);
	}

	if (!error.empty())
	{
		spdlog::error(error);
	}

	if (!ret) {
		spdlog::error("Failed to load file: {}\n\n", gltfPath.filename().string());
		throw std::runtime_error(std::format("Failed to load file: {}", gltfPath.string()));
	}

	spdlog::trace("Loaded model from disk: {}", gltfPath.filename().string());
}

RenderableModel::RenderableModel(const LoadedModel& modelData, bool isStatic)
	: rootNode(modelData.rootNode), transformation(std::make_shared<TransformNode>()), isStatic(isStatic)
{
	loadBuffers(modelData.model);

	loadTextures(modelData.model);

	loadNodes(modelData.model);

	loadSkins(modelData.model);

	loadAnimations(modelData.model);
}

void RenderableModel::setJoints(const std::unordered_map<int, TransformOffset>& offsets)
{
	for (const auto& [i, offset] : offsets)
	{
		nodes[i]->setRotation(offset.rotation);

		nodes[i]->setTranslation(offset.translation);

		nodes[i]->setScale(offset.scale);
	}
}

RenderableModel::~RenderableModel()
{
	glDeleteBuffers(static_cast<GLsizei>(buffers.size()), buffers.data());
	glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
}

// todo: rewrite all these functions
std::shared_ptr<RenderableModel> RenderableModel::constructUnitSphere(unsigned int columns, unsigned int rows)
{
	GLuint vertexArray, vertexBuffer, indicesBuffer;
	glGenVertexArrays(1, &vertexArray);

	glGenBuffers(1, &vertexBuffer);
	glGenBuffers(1, &indicesBuffer);

	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> uv;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;
	std::vector<float> data;

	for (unsigned int x = 0; x <= columns; ++x)
	{
		for (unsigned int y = 0; y <= rows; ++y)
		{
			float xSegment = (float)x / (float)columns;
			float ySegment = (float)y / (float)rows;
			float xPos = std::cos(xSegment * 2.0f * glm::pi<float>()) * std::sin(ySegment * glm::pi<float>());
			float yPos = std::cos(ySegment * glm::pi<float>());
			float zPos = std::sin(xSegment * 2.0f * glm::pi<float>()) * std::sin(ySegment * glm::pi<float>());

			positions.push_back(glm::vec3(xPos, yPos, zPos));
			uv.push_back(glm::vec2(xSegment, ySegment));
			normals.push_back(glm::vec3(xPos, yPos, zPos));

			data.push_back(xPos);
			data.push_back(yPos);
			data.push_back(zPos);
			data.push_back(xPos);
			data.push_back(yPos);
			data.push_back(zPos);
			data.push_back(xSegment);
			data.push_back(ySegment);
		}
	}

	bool oddRow = false;
	for (unsigned int y = 0; y < rows; ++y)
	{
		if (!oddRow) // even rows: y == 0, y == 2; and so on
		{
			for (unsigned int x = 0; x <= columns; ++x)
			{
				indices.push_back(y * (columns + 1) + x);
				indices.push_back((y + 1) * (columns + 1) + x);
			}
		}
		else
		{
			for (int x = columns; x >= 0; --x)
			{
				indices.push_back((y + 1) * (columns + 1) + x);
				indices.push_back(y * (columns + 1) + x);
			}
		}
		oddRow = !oddRow;
	}

	glBindVertexArray(vertexArray);

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	constexpr unsigned int stride = (3 + 2 + 3) * sizeof(float);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

	MeshPrimitive sphere;
	sphere.vertexArray = vertexArray;
	sphere.mode = GL_TRIANGLE_STRIP;
	sphere.indicesBuffer = indicesBuffer;
	sphere.byteOffset = 0;
	sphere.count = static_cast<unsigned int>(indices.size());
	sphere.transform = std::make_shared<TransformNode>();
	sphere.materialDesc = tinygltf::Material();
	sphere.componentType = GL_UNSIGNED_INT;

	const auto buffers = { vertexBuffer, indicesBuffer };
	const auto primitives = { std::make_shared<MeshPrimitive>(sphere) };

	return std::make_shared<RenderableModel>(buffers, primitives, std::vector<GLuint>());
}

std::shared_ptr<RenderableModel> RenderableModel::constructUnitCube()
{
	const std::vector<GLfloat> vertices = {
		// back face
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left

		// front face
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left

		// left face
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right

		// right face
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
		 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left 

		 // bottom face
		 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		  1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
		  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		 -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right

		 // top face
		 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		  1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		  1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
		 -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
	};

	const std::vector<unsigned int> indices =
	{
		// back face
		0, 1, 2,
		1, 0, 3,

		// front face
		4, 5, 6,
		6, 7, 4,

		// left face
		8, 9, 10,
		10, 11, 8,

		// right face
		12, 13, 14,
		13, 12, 15,

		// bottom face
		16, 17, 18,
		18, 19, 16,

		// top face
		20, 21, 22,
		21, 20, 23
	};

	GLuint vertexArray, vertexBuffer, indicesBuffer;
	glGenVertexArrays(1, &vertexArray);

	glGenBuffers(1, &vertexBuffer);
	glGenBuffers(1, &indicesBuffer);

	glBindVertexArray(vertexArray);

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	constexpr unsigned int stride = (3 + 2 + 3) * sizeof(float);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

	MeshPrimitive cube;
	cube.vertexArray = vertexArray;
	cube.mode = GL_TRIANGLES;
	cube.indicesBuffer = indicesBuffer;
	cube.byteOffset = 0;
	cube.count = static_cast<unsigned int>(indices.size());
	cube.transform = std::make_shared<TransformNode>();
	cube.materialDesc = tinygltf::Material();
	cube.componentType = GL_UNSIGNED_INT;

	const auto buffers = { vertexBuffer, indicesBuffer };
	const auto primitives = { std::make_shared<MeshPrimitive>(cube) };

	return std::make_shared<RenderableModel>(buffers, primitives, std::vector<GLuint>());
}

std::shared_ptr<RenderableModel> RenderableModel::constructUnitQuad()
{
	std::vector<float> vertices = {
		// positions       // normals         // texture Coords
		-1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
	};

	std::vector<unsigned int> indices = {
		0, 1, 2, 3
	};

	GLuint vertexArray, vertexBuffer, indicesBuffer;
	glGenVertexArrays(1, &vertexArray);

	glGenBuffers(1, &vertexBuffer);
	glGenBuffers(1, &indicesBuffer);

	glBindVertexArray(vertexArray);

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	constexpr unsigned int stride = (3 + 2 + 3) * sizeof(float);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

	MeshPrimitive quad;
	quad.vertexArray = vertexArray;
	quad.mode = GL_TRIANGLE_STRIP;
	quad.indicesBuffer = indicesBuffer;
	quad.byteOffset = 0;
	quad.count = static_cast<unsigned int>(indices.size());
	quad.transform = std::make_shared<TransformNode>();
	quad.materialDesc = tinygltf::Material();
	quad.componentType = GL_UNSIGNED_INT;

	const auto buffers = { vertexBuffer, indicesBuffer };
	const auto primitives = { std::make_shared<MeshPrimitive>(quad) };

	return std::make_shared<RenderableModel>(buffers, primitives, std::vector<GLuint>());
}

void RenderableModel::loadBuffers(const tinygltf::Model& model)
{
	buffers.resize(model.bufferViews.size(), 0); // Resize the buffer container

	glGenBuffers(static_cast<GLsizei>(buffers.size()), buffers.data());

	for (size_t i = 0; i < buffers.size(); i++)
	{
		const tinygltf::BufferView& bufferView = model.bufferViews.at(i);
		if (bufferView.target != GL_ARRAY_BUFFER && bufferView.target != GL_ELEMENT_ARRAY_BUFFER)
			continue;

		const tinygltf::Buffer& buffer = model.buffers.at(bufferView.buffer);

		glBindBuffer(bufferView.target, buffers.at(i));

		const auto& begin = buffer.data.begin() + bufferView.byteOffset;
		const auto& end = begin + bufferView.byteLength;

		glBufferData(bufferView.target, bufferView.byteLength, std::vector<unsigned char>(begin, end).data(), GL_STATIC_DRAW);
	}
}

void RenderableModel::loadAnimations(const tinygltf::Model& model)
{
	std::mutex m;

	std::for_each(std::execution::par, model.animations.begin(), model.animations.end(), 
		[&](const tinygltf::Animation& animation)
		{
			spdlog::trace("Loading animation: {}", animation.name);

			Animation anim;
			float maxDuration = 0.0f;

			std::vector<Joint>::iterator rootJointIdx = joints.end();
			int rootNodeIdx = -1;

			float start = 0.0f;
			glm::vec3 initialPos, finalPos;

			for (const auto& channel : animation.channels)
			{
				AnimationChannel animationChannel;
				animationChannel.target = channel.target_node;
				animationChannel.path = channel.target_path;

				const auto& sampler = animation.samplers[channel.sampler];

				const auto& inputAccessor = model.accessors[sampler.input];

				if (inputAccessor.maxValues[0] > maxDuration) maxDuration = inputAccessor.maxValues[0];
				if (inputAccessor.minValues[0] > start) start = inputAccessor.minValues[0];

				animationChannel.keyframeTimes = accessorToFloats(model, inputAccessor);

				const auto& outputAccessor = model.accessors[sampler.output];

				animationChannel.values = accessorToFloats(model, outputAccessor);

				if (nodes[animationChannel.target]->name == rootNode
					&& animationChannel.path == "translation")
				{
					// simple find the joint needed -- code is getting bad but this is only done once!
					rootJointIdx = std::find_if(joints.begin(), joints.end(), [&](const Joint& j) {
						return j.nodeIdx == animationChannel.target;
						});
					rootNodeIdx = animationChannel.target;

					initialPos = glm::make_vec3(&animationChannel.values[0]);
					finalPos = glm::make_vec3(&animationChannel.values[animationChannel.values.size() - 3]);
				}

				anim.channels.push_back(animationChannel);
			}

			anim.duration = maxDuration;

			float realDuration = maxDuration - start;

			glm::vec3 naiveVelocity = (finalPos - initialPos) / realDuration;

			if (rootNodeIdx != -1)
			{
				// Now convert these back into world space and calculate the bounds properly
				initialPos =
					nodes[rootNodeIdx]->getWorldTransform() *
					glm::inverse(rootJointIdx->inverseBindMatrix) *
					glm::vec4(initialPos, 1.0f);

				finalPos =
					nodes[rootNodeIdx]->getWorldTransform() *
					glm::inverse(rootJointIdx->inverseBindMatrix) *
					glm::vec4(finalPos, 1.0f);

				anim.rootOffset = (finalPos - initialPos);
				anim.velocity = anim.rootOffset / realDuration;
			}

			for (auto& channel : anim.channels)
			{
				if (channel.path == "translation" && channel.target == rootNodeIdx)
				{
					for (size_t i = 0; i < channel.keyframeTimes.size(); i++)
					{
						channel.values[(i * 3)] -= naiveVelocity.x * channel.keyframeTimes[i];
						channel.values[(i * 3) + 1] -= naiveVelocity.y * channel.keyframeTimes[i];
						channel.values[(i * 3) + 2] -= naiveVelocity.z * channel.keyframeTimes[i];
					}

				}
			}

			std::lock_guard<std::mutex> lock(m);
			
			animations[animation.name] = anim;
		}
	);
}

void RenderableModel::loadSkins(const tinygltf::Model& model)
{
	for (const auto& skin : model.skins)
	{
		std::vector<float> inverseMatricesData = accessorToFloats(model, model.accessors[skin.inverseBindMatrices]);

		for (size_t i = 0; i < skin.joints.size(); i++)
		{
			Joint joint;
			joint.inverseBindMatrix = glm::make_mat4(inverseMatricesData.data() + (i * 16));
			joint.nodeIdx = skin.joints[i];

			joints.push_back(joint);
		}
	}
}

void RenderableModel::loadTextures(const tinygltf::Model& model)
{
	std::vector<GLint> internalFormats(model.textures.size(), GL_RGBA);

	for (const auto& material : model.materials)
	{
		const auto& pbr = material.pbrMetallicRoughness;
		if (pbr.baseColorTexture.index >= 0)
		{
			internalFormats[pbr.baseColorTexture.index] = GL_SRGB8_ALPHA8;
		}
	}

	textures.resize(model.textures.size(), 0); // Resize textures container

	tinygltf::Sampler defaultSampler;
	defaultSampler.minFilter = GL_LINEAR;
	defaultSampler.magFilter = GL_LINEAR;
	defaultSampler.wrapS = GL_REPEAT;
	defaultSampler.wrapT = GL_REPEAT;

	glGenTextures(static_cast<GLsizei>(textures.size()), textures.data());

	for (size_t i = 0; i < model.textures.size(); ++i) {

		const tinygltf::Texture& texture = model.textures.at(i);
		if (texture.source < 0) continue;

		const tinygltf::Image& image = model.images.at(texture.source);

		spdlog::trace("Loading texture: {}", image.uri);

		const tinygltf::Sampler& sampler = // Use default sampler?
			texture.sampler >= 0 ? model.samplers[texture.sampler] : defaultSampler;

		glBindTexture(GL_TEXTURE_2D, textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormats[i], image.width, image.height, 0, GL_RGBA, image.pixel_type, image.image.data());

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			sampler.minFilter != -1 ? sampler.minFilter : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
			sampler.magFilter != -1 ? sampler.magFilter : GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);

		if (sampler.minFilter == GL_NEAREST_MIPMAP_NEAREST ||
			sampler.minFilter == GL_NEAREST_MIPMAP_LINEAR ||
			sampler.minFilter == GL_LINEAR_MIPMAP_NEAREST ||
			sampler.minFilter == GL_LINEAR_MIPMAP_LINEAR) {
			glGenerateMipmap(GL_TEXTURE_2D);
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderableModel::loadNodes(const tinygltf::Model& model)
{
	nodes.reserve(model.nodes.size());
	primitives.reserve(model.meshes.size());

	for (size_t i = 0; i < model.nodes.size(); i++)
	{
		const auto& node = model.nodes[i];
		
		std::shared_ptr<TransformNode> transformNode = std::make_shared<TransformNode>();

		if (node.translation.size() == 3)
		{
			transformNode->setTranslation( {
				static_cast<float>(node.translation.at(0)), static_cast<float>(node.translation.at(1)), static_cast<float>(node.translation.at(2))
			} );
		}

		if (node.scale.size() == 3)
		{
			transformNode->setScale({
				static_cast<float>(node.scale.at(0)), static_cast<float>(node.scale.at(1)), static_cast<float>(node.scale.at(2))
				});
		}

		if (node.rotation.size() == 4)
		{
			// GLM quats go W X Y Z; tinyGLTF quats go X Y Z W
			transformNode->setRotation({
				static_cast<float>(node.rotation.at(3)), static_cast<float>(node.rotation.at(0)), static_cast<float>(node.rotation.at(1)), static_cast<float>(node.rotation.at(2))
				});
		}

		transformNode->name = node.name;
		transformation->addChild(transformNode);

		for (size_t j = 0; j < node.children.size(); j++)
		{
			transformNode->addChild(nodes[node.children[j]]);
		}

		nodes.push_back(transformNode);

		if ((node.mesh >= 0) && (node.mesh < model.meshes.size()))
		{
			const tinygltf::Mesh& mesh = model.meshes.at(node.mesh);

			for (size_t i = 0; i < mesh.primitives.size(); i++)
			{
				loadPrimitive(model, mesh.primitives.at(i), transformNode);
			}
		}
	}
}

void RenderableModel::loadPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::shared_ptr<TransformNode> transformNode)
{
	std::shared_ptr<MeshPrimitive> meshPrimitive = std::make_shared<MeshPrimitive>();

	const tinygltf::Accessor& indexAccessor = model.accessors.at(primitive.indices);

	meshPrimitive->mode = primitive.mode;
	meshPrimitive->indicesBuffer = buffers.at(indexAccessor.bufferView);
	meshPrimitive->count = indexAccessor.count;
	meshPrimitive->componentType = indexAccessor.componentType;
	meshPrimitive->byteOffset = indexAccessor.byteOffset;

	if (model.materials.size() > 0)
	{
		meshPrimitive->materialDesc = model.materials.at(primitive.material);
	}
	else
	{
		meshPrimitive->materialDesc = tinygltf::Material();
	}

	glGenVertexArrays(1, &meshPrimitive->vertexArray);
	glBindVertexArray(meshPrimitive->vertexArray);

	for (const auto& attrib : primitive.attributes)
	{
		const tinygltf::Accessor& accessor = model.accessors.at(attrib.second);
		const int byteStride = accessor.ByteStride(model.bufferViews.at(accessor.bufferView));
		glBindBuffer(GL_ARRAY_BUFFER, buffers.at(accessor.bufferView));

		int size = 1;
		if (accessor.type != TINYGLTF_TYPE_SCALAR) {
			size = accessor.type;
		}

		int vaa = -1;
		if (attrib.first.compare("POSITION") == 0) vaa = 0;
		if (attrib.first.compare("NORMAL") == 0) vaa = 1;
		if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
		if (attrib.first.compare("JOINTS_0") == 0) vaa = 3;
		if (attrib.first.compare("WEIGHTS_0") == 0) vaa = 4;

		if (vaa != -1) {
			glEnableVertexAttribArray(vaa);

			glVertexAttribPointer(
				vaa,
				size, 
				accessor.componentType,
				accessor.normalized ? GL_TRUE : GL_FALSE,
				(GLsizei)byteStride, 
				(void*)accessor.byteOffset
			);
		}
	}

	meshPrimitive->transform = transformNode;
	
	primitives.push_back(meshPrimitive);

	if (meshPrimitive->materialDesc.alphaMode == "OPAQUE")
		opaquePrimitives.push_back(meshPrimitive);
	else
		translucentPrimitives.push_back(meshPrimitive);

}

// chatgpt because im lazy :<|
std::vector<float> RenderableModel::accessorToFloats(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
	std::vector<float> result;

	// Ensure accessor's buffer view is valid
	if (accessor.bufferView < 0 || accessor.bufferView >= model.bufferViews.size()) {
		return result;
	}

	const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
	if (bufferView.buffer < 0 || bufferView.buffer >= model.buffers.size()) {
		return result;
	}

	const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

	// Pointer to the start of the accessor's data in the buffer
	const unsigned char* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

	// Get the component size
	size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
	if (componentSize == 0) {
		return result;
	}

	// Number of components per element (e.g., VEC3 has 3 components)
	int numComponents = tinygltf::GetNumComponentsInType(accessor.type);
	if (numComponents == 0) {
		return result;
	}

	// Total number of elements in the accessor
	size_t count = accessor.count;

	// Resize result to accommodate all floats
	result.resize(count * numComponents);

	// Convert raw data to floats based on component type
	for (size_t i = 0; i < count; ++i) {
		for (int j = 0; j < numComponents; ++j) {
			size_t index = i * numComponents + j;
			size_t byteOffset = i * accessor.ByteStride(bufferView) + j * componentSize;

			// Interpret the component based on its type
			switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_FLOAT: {
				const float* value = reinterpret_cast<const float*>(dataPtr + byteOffset);
				result[index] = *value;
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
				const uint8_t* value = reinterpret_cast<const uint8_t*>(dataPtr + byteOffset);
				result[index] = static_cast<float>(*value);
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_BYTE: {
				const int8_t* value = reinterpret_cast<const int8_t*>(dataPtr + byteOffset);
				result[index] = static_cast<float>(*value);
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
				const uint16_t* value = reinterpret_cast<const uint16_t*>(dataPtr + byteOffset);
				result[index] = static_cast<float>(*value);
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_SHORT: {
				const int16_t* value = reinterpret_cast<const int16_t*>(dataPtr + byteOffset);
				result[index] = static_cast<float>(*value);
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
				const uint32_t* value = reinterpret_cast<const uint32_t*>(dataPtr + byteOffset);
				result[index] = static_cast<float>(*value);
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_INT: {
				const int32_t* value = reinterpret_cast<const int32_t*>(dataPtr + byteOffset);
				result[index] = static_cast<float>(*value);
				break;
			}
			default:
				return {};
			}
		}
	}

	return result;
}




