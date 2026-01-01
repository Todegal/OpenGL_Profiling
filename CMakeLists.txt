cmake_minimum_required(VERSION 3.5)

project(graphics_engine LANGUAGES C CXX)

list(APPEND source
	src/render_context.cpp
	src/raw_data.cpp
	# src/pbr_renderer_old.cpp
	src/pbr_renderer.cpp
	src/orbit_camera.cpp
	# src/model.cpp
	src/main.cpp
	src/input_handler.cpp
	src/imgui_context.cpp
	src/hdr_render_pass.cpp
	src/forward_render_pass.cpp
	src/scene_graph.cpp
	# src/character_controller.cpp
	# src/animation_controller.cpp
	src/window.cpp
	src/opengl_context.cpp
	src/opengl_shader.cpp
	src/profiler.cpp
	src/imgui_profile_visualiser.cpp
	src/imgui_scene_graph_visualiser.cpp
	# src/simple_renderer.cpp
)

list(APPEND include
	src/transform.h
	src/timer.h
	# src/scene.h
	src/render_context.h
	src/raw_data.h
	# src/pbr_renderer_old.h
	src/pbr_renderer.h
	src/orbit_camera.h
	# src/model.h
	src/input_handler.h
	src/imgui_context.h
	src/hdr_render_pass.h
	src/forward_render_pass.h
	src/character_controller.h
	src/camera.h
	src/animation_controller.h
	src/window.h
	src/opengl_context.h
	src/opengl_shader.h
	src/profiler.h
	src/imgui_profile_visualiser.h
	# src/simple_renderer.h
	src/mem_print.h
	src/scene_graph.h
	src/imgui_scene_graph_visualiser.h
)

if (MSVC)
	add_compile_options(/W4 /WX)
else()
	add_compile_options(-pedantic -Wall -Wextra -Werror -Wconversion -fdiagnostics-color=always)
endif()

add_executable(graphics_engine ${source} ${include})

set_property(TARGET graphics_engine PROPERTY CXX_STANDARD 20)

find_package(Stb REQUIRED)
target_include_directories(graphics_engine PRIVATE ${Stb_INCLUDE_DIR})

find_package(glfw3 3.4 REQUIRED)
find_package(glbinding CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)

target_link_libraries(graphics_engine PRIVATE argparse::argparse glfw glbinding::glbinding glbinding::glbinding-aux assimp::assimp glm::glm-header-only imgui::imgui spdlog::spdlog)
