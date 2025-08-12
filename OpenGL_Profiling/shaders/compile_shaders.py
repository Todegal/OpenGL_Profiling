import os
import subprocess
import sys

# Directory to search
root_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
build_root = "build"

# Extension -> GLSLC parameters mapping
compile_options = {
	'.vert': ['-fshader-stage=vert'],
	'.frag': ['-fshader-stage=frag'],
	'.comp': ['-fshader-stage=comp'],
	'.geom': ['-fshader-stage=geom']
}

def compile_shader(file_path):
	ext = os.path.splitext(file_path)[1]
	if ext not in compile_options:
		print(f"Skipping unsupported file type: {file_path}")
		return

	working_dir = os.path.dirname(file_path)
	file_name = os.path.basename(file_path)

	rel_path = os.path.relpath(file_path, root_dir)
	output_path = os.path.join(build_root, rel_path) + '.spv'
	
	os.makedirs(os.path.dirname(output_path), exist_ok=True)

	output_path = os.path.relpath(output_path, working_dir)

	cmd = ['glslc', '-o', output_path, '-c', '--target-env=opengl', '-fauto-bind-uniforms', '-fauto-map-locations', file_name] + compile_options[ext]

	print(f"Compiling {file_path} -> {output_path}")
	try:
		subprocess.run(" ".join(cmd), check=True, cwd=working_dir)
	except subprocess.CalledProcessError as e:
		print(f"Error compiling {file_path}: {e}")

for root, dirs, files in os.walk(root_dir):
	for file in files:
		file_path = os.path.join(root, file)
		compile_shader(file_path)