#include <memory>
#include <algorithm>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tiny_gltf.h>

#undef TINYGLTF_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION

#include <glm/gtc/random.hpp>
#include <glm/gtx/string_cast.hpp>

#include "characterController.h"
#include "inputHandler.h"
#include "model.h"
#include "orbitCamera.h"
#include "pbrRenderer.h"
#include "shaderProgram.h"
#include "timer.h"

constexpr int WIDTH = 1920 * 0.9;
constexpr int HEIGHT = 1080 * 0.9;

void GLAPIENTRY
gl_error_callback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:
		spdlog::error("OpenGL: {}", message);
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		spdlog::warn("OpenGL: {}", message);
		break;
	default:
		spdlog::trace("OpenGL: {}", message);
	}

	//spdlog::debug("GL CALLBACK: {0} type = {1:#X}, severity = {2:#X}, message = {3}",
	//	(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
	//	type, severity, message);
}

int main()
{
	Timer t;
	t.start();

	spdlog::set_level(spdlog::level::trace);

	if (!glfwInit())
	{
		spdlog::critical("Failed to init GLFW!");

		return EXIT_FAILURE;
	}

	glfwWindowHint(GLFW_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_VERSION_MINOR, 5);

	glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = 
		glfwCreateWindow(WIDTH, HEIGHT, "-- OpenGL_Profiling --", nullptr, nullptr);

	if (!window)
	{
		spdlog::critical("Failed to create Window!");
		return EXIT_FAILURE;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(0);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		spdlog::critical("Failed to initialize OpenGL context");

		return EXIT_FAILURE;
	}

	// Successfully loaded OpenGL
	spdlog::info("Loaded OpenGL {}.{}", GLVersion.major, GLVersion.minor);

	spdlog::info("Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
	spdlog::info("Vendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
	spdlog::info("GLSL Version: {}", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(gl_error_callback, 0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glEnable(GL_CULL_FACE);

	glViewport(0, 0, WIDTH, HEIGHT);

	InputHandler input(window);
	input.defineToggle("toggleOcclusion", { GLFW_KEY_O }, {}, true);
	input.defineToggle("toggleNormals", { GLFW_KEY_N }, {}, true);
	input.defineToggle("toggleShadows", { GLFW_KEY_M }, {}, false);
	input.defineToggle("toggleDPP", { GLFW_KEY_P }, {}, true);
	input.defineToggle("toggleDeferredPass", { GLFW_KEY_D }, {}, true);
	input.defineToggle("toggleHdrPass", { GLFW_KEY_H }, {}, true);


	input.defineAction("rotate", {}, { GLFW_MOUSE_BUTTON_LEFT });
	input.defineAction("pan", { GLFW_KEY_LEFT_SHIFT });
	input.defineAction("zoom", { }, { GLFW_MOUSE_BUTTON_RIGHT });

	OrbitCamera camera(
		glm::vec3(0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		5.0f, 0.2f, 0.0f, glm::quarter_pi<float>()
	);
		

	PBRRenderer renderer(glm::ivec2( WIDTH, HEIGHT ), camera);

	std::vector<std::shared_ptr<Model>> models;

	models.push_back(std::make_shared<Model>("C:/Users/Niall Townley/Documents/Source/Viper/Models/Sponza/glTF/Sponza.gltf"));

	//models.push_back(Model::constructUnitQuad());
	//models[1]->getTransform()->rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1, 0, 0));
	//models[1]->getTransform()->scale = glm::vec3(10.0f);

	std::vector<Light> lights;
	lights.push_back(
		{
			{ 0, 1, 1 },
			{ 1, 1, 1 },
			2.0f
		}
	);

	int nLights = 5;

	for (size_t i = 0; i < nLights; i++)
	{
		glm::vec3 position = { glm::linearRand(-8.0f, 8.0f), glm::linearRand(0.2f, 10.0f), glm::linearRand(-4.0f, 4.0f) };
		glm::vec3 colour = { glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f) };

		lights.push_back(
			{
				glm::vec4(position, 1.0f),
				colour,
				10.0f
			}
		);
	}

	std::shared_ptr<Scene> scene = std::make_shared<Scene>();

	scene->sceneLights = lights;
	scene->sceneModels = models;

	renderer.loadScene(scene);

	struct UserPointer
	{
		PBRRenderer* rendererPtr;
		float scroll = 0.0f;
	} userPtr;

	userPtr.rendererPtr = &renderer;

	glfwSetWindowUserPointer(window, &userPtr);

	glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) 
		{
			UserPointer* data = reinterpret_cast<UserPointer*>(glfwGetWindowUserPointer(window));

			data->rendererPtr->resize({ width < 1 ? 1 : width, height < 1 ? 1 : height });
		});

	glfwSetScrollCallback(window, [](GLFWwindow* window, double _, double y)
		{
			UserPointer* data = reinterpret_cast<UserPointer*>(glfwGetWindowUserPointer(window));

			data->scroll = static_cast<float>(y);
		});


	glm::mat4 projectionMatrix = glm::perspective(90.0f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.00001f, 10000.0f);

	while (!glfwWindowShouldClose(window))
	{
		t.tick();

		userPtr.scroll = 0.0f;

		glfwPollEvents();

		input.pollInputs();

		const auto& offset = input.getMouseOffset();
		
		if (input.getAction("rotate") && input.getAction("pan"))
		{
			camera.moveHorizontal(-offset.x * 5.0f);
			camera.moveVertical(offset.y * 5.0f);
		}
		else if (input.getAction("rotate"))
		{
			camera.rotateAzimuth(offset.x * 10.0f);
			camera.rotatePolar(offset.y * 10.0f);
		}
		
		camera.zoom(userPtr.scroll * 5.0f);

		renderer.setFlag(PBRRenderer::OCCLUSION_ENABLED, input.getToggle("toggleOcclusion"));

		renderer.setFlag(PBRRenderer::NORMALS_ENABLED, input.getToggle("toggleNormals"));

		renderer.setFlag(PBRRenderer::SHADOWS_ENABLED, input.getToggle("toggleShadows"));

		renderer.setFlag(PBRRenderer::DEPTH_PREPASS_ENABLED, input.getToggle("toggleDPP"));

		renderer.setFlag(PBRRenderer::DEFERRED_PASS_ENABLED, input.getToggle("toggleDeferredPass"));

		renderer.setFlag(PBRRenderer::HDR_PASS_ENABLED, input.getToggle("toggleHdrPass"));

		renderer.frame();

		glfwSwapBuffers(window);
	}

	glfwDestroyWindow(window);

	glfwTerminate();

	return EXIT_SUCCESS;
}
