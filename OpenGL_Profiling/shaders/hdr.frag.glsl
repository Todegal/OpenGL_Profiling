#version 460

uniform sampler2D uColour;
uniform vec2 uScreenDimensions;

out vec4 vFragColour;

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Converts a color from linear light gamma to sRGB gamma
vec3 fromLinear(vec3 linearRGB)
{
    bvec3 cutoff = lessThan(linearRGB.rgb, vec3(0.0031308));
    vec3 higher = vec3(1.055) * pow(linearRGB.rgb, vec3(1.0 / 2.4)) - vec3(0.055);
    vec3 lower = linearRGB.rgb * vec3(12.92);

    return mix(higher, lower, cutoff);
}

void main()
{

	vec2 uvs = (gl_FragCoord.xy / uScreenDimensions);

    vec3 colour = texture(uColour, uvs).rgb;

    colour = ACESFilm(colour);
    colour = fromLinear(colour);

	vFragColour = vec4(colour, 1.0);
    // vFragColour = vec4(1, 0, 0, 1);
}