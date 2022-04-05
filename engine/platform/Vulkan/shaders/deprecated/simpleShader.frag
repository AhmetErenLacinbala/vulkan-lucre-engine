/* Engine Copyright (c) 2022 Engine Development Team 
   https://github.com/beaumanvienna/vulkan
   * 
   * litShader: Blinn Phong lighting (ambient, diffuse, and specular with a texture map)
   * 

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation files
   (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/

#version 450

layout(location = 0)      in vec3  fragColor;
layout(location = 1)      in vec3  fragPositionWorld;
layout(location = 2)      in vec3  fragNormalWorld;
layout(location = 3)      in vec2  fragUV;
layout(location = 4)      in float fragAmplification;
layout(location = 5) flat in int   fragUnlit;
layout(location = 6)      in vec3  toCameraDirection;

struct PointLight
{
    vec4 m_Position;  // ignore w
    vec4 m_Color;     // w is intensity
};

layout(set = 0, binding = 0) uniform GlobalUniformBuffer
{
    mat4 m_Projection;
    mat4 m_View;

    // point light
    vec4 m_AmbientLightColor;
    PointLight m_PointLights[10];
    int m_NumberOfActiveLights;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D diffuseMapSampler;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Push
{
    mat4 m_ModelMatrix;
    mat4 m_NormalMatrix;
} push;

void main()
{

    vec3 ambientLightColor = ubo.m_AmbientLightColor.xyz * ubo.m_AmbientLightColor.w;

    // ---------- lighting ----------
    vec3 diffusedLightColor = vec3(0.0);
    vec3 surfaceNormal;

    // blinn phong: theta between N and H
    vec3 specularLightColor = vec3(0.0, 0.0, 0.0);
    vec3 directionToCamera  = normalize(toCameraDirection);

    for (int i = 0; i < ubo.m_NumberOfActiveLights; i++)
    {
        PointLight light = ubo.m_PointLights[i];

        // normal in world space
        surfaceNormal = normalize(fragNormalWorld);
        vec3 directionToLight     = light.m_Position.xyz - fragPositionWorld;
        float distanceToLight     = length(directionToLight);
        float attenuation = 1.0 / (distanceToLight * distanceToLight);

        // ---------- diffused ----------
        float cosAngleOfIncidence = max(dot(surfaceNormal, normalize(directionToLight)), 0.0);
        vec3 intensity = light.m_Color.xyz * light.m_Color.w * attenuation;
        diffusedLightColor += intensity * cosAngleOfIncidence;

        // ---------- specular ----------
        if (cosAngleOfIncidence != 0.0)
        {
            vec3 incidenceVector      = - normalize(directionToLight);
            vec3 reflectedLightDir    = reflect(incidenceVector, surfaceNormal);

            // phong
            //float specularFactor      = max(dot(reflectedLightDir, directionToCamera),0.0);
            // blinn phong
            vec3 halfwayDirection     = normalize(-incidenceVector + directionToCamera);
            float specularFactor      = max(dot(surfaceNormal, halfwayDirection),0.0);

            float specularReflection  = pow(specularFactor, 128);
            vec3  intensity = light.m_Color.xyz * light.m_Color.w * attenuation;
            specularLightColor += intensity * specularReflection;
        }
    }
    // ------------------------------

    vec3 pixelColor = texture(diffuseMapSampler, fragUV).xyz;
    float alpha = texture(diffuseMapSampler, fragUV).w;

    outColor.xyz = ambientLightColor*pixelColor.xyz + (diffusedLightColor  * pixelColor.xyz) + specularLightColor;

    // reinhard tone mapping
    outColor.xyz = outColor.xyz / (outColor.xyz + vec3(1.0));

    outColor.w = alpha;

}
