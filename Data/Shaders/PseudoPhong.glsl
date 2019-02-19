-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;

out vec4 fragmentColor;
out vec3 fragmentNormal;
out vec3 fragmentPositonLocal;
out vec3 fragmentPositonWorld;
out vec3 screenSpacePosition;

// Color of the object
uniform vec4 colorGlobal;

void main()
{
    fragmentColor = colorGlobal;
    fragmentNormal = vertexNormal;
    fragmentPositonLocal = (vec4(vertexPosition, 1.0)).xyz;
    fragmentPositonWorld = (mMatrix * vec4(vertexPosition, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(vertexPosition, 1.0)).xyz;
    gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

in vec3 screenSpacePosition;

#if !defined(DIRECT_BLIT_GATHER) || defined(SHADOW_MAPPING_MOMENTS_GENERATE)
#include OIT_GATHER_HEADER
#endif

in vec4 fragmentColor;
in vec3 fragmentNormal;
in vec3 fragmentPositonLocal;
in vec3 fragmentPositonWorld;

#if defined(DIRECT_BLIT_GATHER) && !defined(SHADOW_MAPPING_MOMENTS_GENERATE)
out vec4 fragColor;
#endif


#include "AmbientOcclusion.glsl"
#include "Shadows.glsl"

uniform vec3 lightDirection = vec3(1.0,0.0,0.0);
uniform vec3 cameraPosition; // world space

uniform vec3 ambientColor;
uniform vec3 diffuseColor;
uniform vec3 specularColor;
uniform float specularExponent;
uniform float opacity;
uniform int bandedColorShading = 1;

void main()
{
    float occlusionFactor = getAmbientOcclusionFactor(vec4(fragmentPositonWorld, 1.0));
    float shadowFactor = getShadowFactor(vec4(fragmentPositonWorld, 1.0));

    // Pseudo Phong shading
    vec4 bandColor = fragmentColor;
    float stripWidth = 2.0;
    if (mod(fragmentPositonLocal.x, 2.0*stripWidth) < stripWidth) {
        bandColor = vec4(1.0,1.0,1.0,1.0);
    }
    vec4 color = vec4(bandColor.rgb * (dot(normalize(fragmentNormal), lightDirection)/4.0+0.75
            * occlusionFactor * shadowFactor), fragmentColor.a);

    if (bandedColorShading == 0) {
        vec3 ambientShading = ambientColor * 0.1 * occlusionFactor * shadowFactor;
        vec3 diffuseShading = diffuseColor * clamp(dot(fragmentNormal, lightDirection)/2.0+0.75
                * occlusionFactor * shadowFactor, 0.0, 1.0);
        vec3 specularShading = specularColor * specularExponent * 0.00001; // In order not to get an unused warning
        color = vec4(ambientShading + diffuseShading + specularShading, opacity * fragmentColor.a);
    }
    //color.rgb = vec3(vec3(shadowFactor));

#if defined(DIRECT_BLIT_GATHER) && !defined(SHADOW_MAPPING_MOMENTS_GENERATE)
    // Direct rendering
    fragColor = color;
#else
#if defined(REQUIRE_INVOCATION_INTERLOCK) && !defined(TEST_NO_INVOCATION_INTERLOCK)
    // Area of mutual exclusion for fragments mapping to the same pixel
    beginInvocationInterlockARB();
    gatherFragment(color);
    endInvocationInterlockARB();
#else
    gatherFragment(color);
#endif
#endif
}
