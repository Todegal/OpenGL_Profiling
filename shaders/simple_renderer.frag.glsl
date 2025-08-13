#version 460 core

layout(std140) uniform RenderUniforms
{
	vec3 uCameraPosition;
	int uNumLights;
};

uniform sampler2D uAlbedoTexture;
uniform bool uAlbedoTextureEnabled;
uniform vec3 uAlbedoFactor;

struct Light
{
	vec4 position;
	vec4 colour;
};

layout(std430) buffer LightBuffer
{
	Light bLights[];
};

in VS_OUT
{
	vec3 worldPos;
	vec2 texCoords;
	vec3 normal;
} fs_in;

out vec4 vFragColour;

void main()
{
	const vec3 viewVector = normalize(uCameraPosition - fs_in.worldPos);
	
	const float ambientFactor = 0.15;

	const vec3 baseColour = (uAlbedoTextureEnabled) ? texture(uAlbedoTexture, fs_in.texCoords).rgb : uAlbedoFactor;

	vec3 Lo = vec3(0.0);
	for (int i = 0; i < uNumLights; i++)
	{
		const Light l = bLights[i];

		vec3 lightVector = l.position.xyz - fs_in.worldPos;
		const float lightDistance = length(lightVector);

		lightVector /= lightDistance;

		const float lambertianFactor = clamp(dot(lightVector, fs_in.normal), 0.0, 1.0);

		const vec3 halfVector = normalize(lightVector + viewVector);
		const float specularFactor = pow(clamp(dot(halfVector, fs_in.normal), 0.0, 1.0), 8.0);

		const float attenuationFactor = 1.0 / (lightDistance * lightDistance);

		Lo += ((baseColour * lambertianFactor) + (specularFactor * l.colour.rgb)) * attenuationFactor;
	}
	
	vFragColour = vec4(Lo + (baseColour * ambientFactor), 1.0);
}
