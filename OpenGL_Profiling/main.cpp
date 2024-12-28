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
		spdlog::info("OpenGL: {}", message);
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

	glfwWindowHint(GLFW_SAMPLES, 4);

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
	//glfwSwapInterval(0);

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

	glEnable(GL_FRAMEBUFFER_SRGB);

	glViewport(0, 0, WIDTH, HEIGHT);

	OrbitCamera camera(
		{ 0, 0, 0 },
		{ 0, 1, 0 },
		2.5f, 1.0f,
		glm::radians(-90.0f), glm::radians(40.0f)
	);

	PBRRenderer renderer({ WIDTH, HEIGHT }, camera);


	std::vector<std::shared_ptr<Model>> models;
	const auto m = std::make_shared<Model>("..//Models//Dummy//Dummy.glb", "mixamorig:Hips");
	models.push_back(m);

	// find head
	auto head = std::find_if(
		std::begin(m->getNodes()), std::end(m->getNodes()),
		[](const std::shared_ptr<TransformNode> n)
		{
			return n->name == "mixamorig:Spine2";
		}
	);

	//camera.setViewPoint(*head->translation);

	models[0]->selectAnimation("idle");

	models.push_back(Model::constructUnitQuad());
	models[1]->getTransform()->rotation = glm::quat(glm::radians(glm::vec3(-90, 0, 0)));
	models[1]->getTransform()->scale *= 100;

	std::vector<Light> lights;
	lights.push_back(
		{
			{ 0, 1, 1 },
			{ 1, 1, 1 },
			2.0f
		}
	);

	int nLights = 10;

	for (size_t i = 0; i < nLights; i++)
	{
		glm::vec3 position = { glm::linearRand(-3.0f, 3.0f), glm::linearRand(0.2f, 3.0f), glm::linearRand(-3.0f, 3.0f) };
		glm::vec3 colour = { glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f) };

		lights.push_back(
			{
				glm::vec4(position, 1.0f),
				colour,
				2.0f
			}
		);
	}


	std::shared_ptr<Scene> scene = std::make_shared<Scene>();

	scene->sceneLights = lights;
	scene->sceneModels = models;

	renderer.loadScene(scene);

	struct UserPointer
	{
		double x, y, lastX, lastY;
		bool rotate = false;
		OrbitCamera* cameraPtr;
		PBRRenderer* rendererPtr;
	} userPtr;

	userPtr.cameraPtr = &camera;
	userPtr.rendererPtr = &renderer;

	glfwSetWindowUserPointer(window, &userPtr);

	glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods)
		{
			auto data = reinterpret_cast<UserPointer*>(glfwGetWindowUserPointer(window));

			if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
			{
				glfwGetCursorPos(window, &data->x, &data->y);

				data->lastX = data->x;
				data->lastY = data->y;

				data->rotate = !data->rotate;
			}

			if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
			{
				data->rotate = false;
			}
		});

	glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) 
		{
			auto data = reinterpret_cast<UserPointer*>(glfwGetWindowUserPointer(window));

			if (data->rotate)
			{
				data->x = xpos; data->y = ypos;
			}
		});

	glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset) 
		{
			auto data = reinterpret_cast<UserPointer*>(glfwGetWindowUserPointer(window));

			data->cameraPtr->zoom(static_cast<float>(yoffset));

		});

	glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) 
		{
			UserPointer* data = reinterpret_cast<UserPointer*>(glfwGetWindowUserPointer(window));

			data->rendererPtr->resize({ width < 1 ? 1 : width, height < 1 ? 1 : height });
		});

	InputHandler input(window);
	input.defineToggle("toggleOcclusion", { GLFW_KEY_O });
	input.defineToggle("toggleNormals", { GLFW_KEY_N });
	input.defineToggle("toggleShadows", { GLFW_KEY_M });

	input.defineAxis("forward", { GLFW_KEY_W }, 1.0f);
	input.defineAxis("forward", { GLFW_KEY_S }, -1.0f);

	input.defineAxis("right", { GLFW_KEY_D }, 1.0f);
	input.defineAxis("right", { GLFW_KEY_A }, -1.0f);

	input.defineAction("sprint", { GLFW_KEY_LEFT_SHIFT });
	input.defineAction("jump", { GLFW_KEY_SPACE });


	glm::mat4 projectionMatrix = glm::perspective(90.0f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.00001f, 10000.0f);

	while (!glfwWindowShouldClose(window))
	{
		t.tick();

		glfwPollEvents();

		input.pollInputs();

		renderer.setFlag(PBRRenderer::OCCLUSION_ENABLED, !input.getToggle("toggleOcclusion"));

		renderer.setFlag(PBRRenderer::NORMALS_ENABLED, !input.getToggle("toggleNormals"));

		renderer.setFlag(PBRRenderer::SHADOWS_ENABLED, !input.getToggle("toggleShadows"));

		if (input.getAxis("forward") != 0.0f && input.getAction("sprint"))
		{
			models[0]->selectAnimation("run", true);
		}
		else if (input.getAxis("forward") != 0.0f)
		{
			models[0]->selectAnimation("walk", true);
		}
		else if (input.getAxis("forward") == 0.0f
			&& input.getAxis("right") != 0.0f
			&& input.getAction("sprint"))
		{
			models[0]->selectAnimation(input.getAxis("right") > 0.0f ? "strafe_run_right" : "strafe_run_left", true);
		}
		else if (input.getAxis("forward") == 0.0f
			&& input.getAxis("right") != 0.0f)
		{
			models[0]->selectAnimation(input.getAxis("right") > 0.0f ? "strafe_right" : "strafe_left", true);
		}
		else
		{
			models[0]->selectAnimation("idle");
		}

		scene->sceneLights[0].position = glm::vec3(
			glm::sin(t.getTimeElapsed<Timer::f_seconds>().count()), 
			1,
			glm::cos(t.getTimeElapsed<Timer::f_seconds>().count())
		);

		// Move Camera
		if (userPtr.x != userPtr.lastX || userPtr.y != userPtr.lastY)
		{
			float dx = static_cast<float>(userPtr.x - userPtr.lastX) / static_cast<float>(WIDTH);
			float dy = static_cast<float>(userPtr.y - userPtr.lastY) / static_cast<float>(HEIGHT);

			if (userPtr.rotate)
			{
				camera.rotateAzimuth(dx * 10.0f);
				camera.rotatePolar(dy * 10.0f);
			}

			userPtr.lastX = userPtr.x; userPtr.lastY = userPtr.y;
		}

		models[0]->advanceAnimation(t.getDeltaTime<Timer::f_seconds>().count());

		models[0]->getTransform()->translation += glm::vec3(
			models[0]->getVelocity().x * t.getDeltaTime<Timer::f_seconds>().count(),
			0.0f,
			models[0]->getVelocity().z * input.getAxis("forward") * t.getDeltaTime<Timer::f_seconds>().count()
		);

		//camera.setViewPoint(head->translation);

		renderer.frame();

		glfwSwapBuffers(window);
	}

	glfwDestroyWindow(window);

	glfwTerminate();

	return EXIT_SUCCESS;
}
