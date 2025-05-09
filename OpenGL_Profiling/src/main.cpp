#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS

#include <memory>
#include <algorithm>
#include <execution>

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

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	glViewport(0, 0, WIDTH, HEIGHT);

	InputHandler input(window);

	input.defineAction("rotate", {}, { GLFW_MOUSE_BUTTON_LEFT });
	input.defineAction("pan", { GLFW_KEY_LEFT_SHIFT });

	input.defineAction("recompile", { GLFW_KEY_R }, {});

	OrbitCamera orbitCamera(
		glm::vec3(0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		5.0f, 0.2f, 0.0f, glm::quarter_pi<float>()
	);

	CharacterController character(input);

	//PBRRenderer_old renderer(glm::ivec2(WIDTH, HEIGHT), std::make_shared<OrbitCamera>(std::move(orbitCamera)));
	PBRRenderer renderer(glm::ivec2(WIDTH, HEIGHT), std::make_shared<OrbitCamera>(std::move(orbitCamera)));

	std::vector<std::string> modelPaths = {
		"C:\\Users\\Niall Townley\\Documents\\Source\\Viper\\Models\\Sponza\\glTF\\Sponza.gltf"
		//"C:\\Users\\Niall Townley\\Documents\\Source\\Viper\\Models\\Statue\\greek-slave-plaster-cast-150k-4096-web.gltf",
		//"../Models/Board/Board.glb"
	};
	std::vector<RawModel> loadedModels;

	std::vector<std::shared_ptr<RenderableModel>> models;

	std::for_each(std::execution::par, modelPaths.begin(), modelPaths.end(), 
		[&loadedModels](const std::string& path)
		{
			loadedModels.push_back(RawModel(path));
		}
	);

	std::for_each(loadedModels.begin(), loadedModels.end(),
		[&models] (RawModel& m) {
			models.push_back(std::make_shared<RenderableModel>(m.extract()));
		}
	);

	models.push_back(character.getModel());

	std::vector<Light> lights;
	lights.push_back(
		{
			{ 1, 1, 0 },
			{ 1, 0, 0 },
			10.0f, Light::POINT
		}
	);

	lights.push_back(
		{
			{ 0, 2, 0 },
			{ 0, 1, 0 },
			10.0f, Light::POINT
		}
	);

	lights.push_back(
		{
			{ 0, 1, 1 },
			{ 0, 0, 1 },
			10.0f, Light::POINT
		}
	);

	int nLights = 0;

	for (size_t i = 0; i < nLights; i++)
	{
		glm::vec3 position = { glm::linearRand(-2.0f, 2.0f), glm::linearRand(0.2f, 5.0f), glm::linearRand(-4.0f, 4.0f) };
		glm::vec3 colour = { glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f) };

		lights.push_back(
			{
				glm::vec4(position, 1.0f),
				colour,
				20.0f
			}
		);
	}

	std::shared_ptr<Scene> scene = std::make_shared<Scene>();

	scene->sceneLights = lights;
	scene->sceneModels = models;
	scene->environmentMap = "C://Users/Niall Townley/Documents/Source/Viper/Environments/818-hdri-skies-com.hdr";

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
		if (imguiData.showMetrics) { metrics(t, imguiData); }
		if (imguiData.showCharacterInfo) { character.showInfo(imguiData); }
		if (imguiData.showRenderDialog) { renderer.imguiFrame(imguiData); }

		if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse)
			input.pollInputs();

		if (imguiData.orbitCameraEnabled)
		{
			const auto& offset = input.getMouseOffset();

			if (input.getAction("rotate") && input.getAction("pan"))
			{
				orbitCamera.moveHorizontal(-offset.x * 5.0f);
				orbitCamera.moveVertical(offset.y * 5.0f);
			}
			else if (input.getAction("rotate"))
			{
				orbitCamera.rotateAzimuth(offset.x * 10.0f);
				orbitCamera.rotatePolar(offset.y * 10.0f);
			}

			orbitCamera.zoom(userPtr.scroll * 5.0f);

			renderer.setCamera(std::make_shared<OrbitCamera>(std::move(orbitCamera)));

		}
		else
		{
			character.update(t.getDeltaTime<Timer::f_seconds>());

			renderer.setCamera(character.getCameraPtr());
		}

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
