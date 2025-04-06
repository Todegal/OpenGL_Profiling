#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS

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

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "characterController.h"
#include "inputHandler.h"
#include "model.h"
#include "orbitCamera.h"
#include "pbrRenderer.h"
#include "shaderProgram.h"
#include "timer.h"

#include "imguiWindows.h"

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

	spdlog::set_level(spdlog::level::debug);

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

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);          // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
	ImGui_ImplOpenGL3_Init();

	imgui_data imguiData;

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

	input.defineAction("rotate", {}, { GLFW_MOUSE_BUTTON_LEFT });
	input.defineAction("pan", { GLFW_KEY_LEFT_SHIFT });

	input.defineAction("recompile", { GLFW_KEY_R }, {});

	//OrbitCamera camera(
	//	glm::vec3(0.0f),
	//	glm::vec3(0.0f, 1.0f, 0.0f),
	//	5.0f, 0.2f, 0.0f, glm::quarter_pi<float>()
	//);

	CharacterController character(input);

	PBRRenderer renderer(glm::ivec2(WIDTH, HEIGHT), character.getCamera());

	std::vector<std::shared_ptr<Model>> models;

	models.push_back(character.getModel());
	//models.push_back(std::make_shared<Model>("C:/Users/Niall Townley/Documents/Source/Viper/Models/Sponza/glTF/Sponza.gltf"));

	//models.push_back(Model::constructUnitQuad());
	//models.back()->getTransform()->rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1, 0, 0));
	//models.back()->getTransform()->scale = glm::vec3(10.0f);

	models.push_back(std::make_shared<Model>("../Models/Board/Board.glb"));

	std::vector<Light> lights;
	lights.push_back(
		{
			{ 1, 1, 0 },
			{ 1, 0, 0 },
			10.0f
		}
	);

	lights.push_back(
		{
			{ 0, 2, 0 },
			{ 0, 1, 0 },
			10.0f
		}
	);

	lights.push_back(
		{
			{ 0, 1, 1 },
			{ 0, 0, 1 },
			10.0f
		}
	);

	int nLights = 20;

	for (size_t i = 0; i < nLights; i++)
	{
		glm::vec3 position = { glm::linearRand(-2.0f, 2.0f), glm::linearRand(0.2f, 5.0f), glm::linearRand(-2.0f, 2.0f) };
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
	scene->enviromentMap = "C://Users/Niall Townley/Documents/Source/Viper/Environments/lake_pier_4k/lake_pier_4k.hdr";

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

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		drawMenuBar(imguiData);
		if (imguiData.showRenderFlags) { renderer.drawFlagsDialog(imguiData); }
		if (imguiData.showMetrics) { metrics(t, imguiData); }
		if (imguiData.showCharacterInfo) { character.showInfo(imguiData); }

		if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse)
			input.pollInputs();

		/*const auto& offset = input.getMouseOffset();

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

		if (input.getAction("recompile"))
		{
			ShaderProgram::recompileAllPrograms();
		}

		camera.zoom(userPtr.scroll * 5.0f);*/

		character.update(t.getDeltaTime<Timer::f_seconds>());

		renderer.frame();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);

	glfwTerminate();

	return EXIT_SUCCESS;
}
