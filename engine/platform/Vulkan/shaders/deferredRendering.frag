/* Engine Copyright (c) 2022 Engine Development Team 
   https://github.com/beaumanvienna/vulkan
   *
   * PBR rendering; parts of this code are based on https://learnopengl.com/PBR/Lighting
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
#define MAX_LIGHTS 128

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput positionMap;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput normalMap;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput diffuseMap;
layout(input_attachment_index = 3, set = 1, binding = 3) uniform subpassInput roughnessMetallicMap;

layout(location = 0) out vec4 outColor;

struct PointLight
{
    vec4 m_Position;  // ignore w
    vec4 m_Color;     // w is intensity
};

struct DirectionalLight
{
    vec4 m_Direction;  // ignore w
    vec4 m_Color;     // w is intensity
};

layout(set = 0, binding = 0) uniform GlobalUniformBuffer
{
    mat4 m_Projection;
    mat4 m_View;

    // point light
    vec4 m_AmbientLightColor;
    PointLight m_PointLights[MAX_LIGHTS];
    DirectionalLight m_DirectionalLight;
    int m_NumberOfActivePointLights;
    int m_NumberOfActiveDirectionalLights;
} ubo;


layout(set = 2, binding = 0) uniform sampler2D shadowMapTextureHiRes;
layout(set = 2, binding = 1) uniform sampler2D shadowMapTextureLowRes;
layout(set = 2, binding = 2) uniform ShadowUniformBuffer0
{
    mat4 m_Projection;
    mat4 m_View;
} lightUboHiRes;

layout(set = 2, binding = 3) uniform ShadowUniformBuffer1
{
    mat4 m_Projection;
    mat4 m_View;
} lightUboLowRes;

const float PI = 3.14159265359;

vec3 ACESFilm(vec3 color) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((color*(a*color+b))/(color*(c*color+d)+e), 0.0, 1.0);
}
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------
void main()
{
    // retrieve G buffer data
    vec3 fragPosition = subpassLoad(positionMap).rgb;
    vec3 normal       = subpassLoad(normalMap).rgb;
    vec4 albedo       = subpassLoad(diffuseMap);
    vec4 material     = subpassLoad(roughnessMetallicMap);

    float roughness           = material.g;
    float metallic            = material.r;
    vec3  ambientLightColor   = ubo.m_AmbientLightColor.xyz * ubo.m_AmbientLightColor.w;

    vec3 camPos = (inverse(ubo.m_View) * vec4(0.0,0.0,0.0,1.0)).xyz;

    vec3 N = normalize(normal);
    vec3 V = normalize(camPos - fragPosition);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 fragColor = albedo.rgb;
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, fragColor, metallic);
    // reflectance equation
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < ubo.m_NumberOfActivePointLights; i++)
    {
        PointLight light = ubo.m_PointLights[i];
        // calculate per-light radiance
        vec3 L = normalize(light.m_Position.xyz - fragPosition);
        vec3 H = normalize(V + L);
        float distance = length(light.m_Position.xyz - fragPosition);
        float attenuation = 1.0 / (distance * distance);
        float lightIntensity = light.m_Color.w;
        vec3 radiance = light.m_Color.rgb * lightIntensity * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);  
        vec3 F    = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;

        // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;  

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);

        // add to outgoing radiance Lo
        Lo += (kD * fragColor / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    }

    if(ubo.m_NumberOfActiveDirectionalLights > 0)
    {
        // calculate radiance for a directional light
        vec3 L = normalize(-ubo.m_DirectionalLight.m_Direction.xyz);
        vec3 H = normalize(V + L);
        float lightIntensity = ubo.m_DirectionalLight.m_Color.w;
        vec3 radiance = ubo.m_DirectionalLight.m_Color.rgb * lightIntensity;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);  
        vec3 F    = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;

        // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;  

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);
        float shadowPercentage;
        int PCF_SIZE = 3;
        int SHADOWMAP_SIZE_HIRES_RES = 4096;
        int SHADOWMAP_SIZE_LOW_RES   = 1024;

        vec4 lightSpacePosistionHiRes = lightUboHiRes.m_Projection * lightUboHiRes.m_View * vec4(fragPosition, 1.0);
        vec3 lightSpacePosistionNDCHiRes = lightSpacePosistionHiRes.xyz / lightSpacePosistionHiRes.w;
        if (
                abs(lightSpacePosistionNDCHiRes.x) > 1.0 ||
                abs(lightSpacePosistionNDCHiRes.y) > 1.0 ||
                abs(lightSpacePosistionNDCHiRes.z) > 1.0
            )
        {
            // check low-resolution shadow map
            vec4 lightSpacePosistionLowRes = lightUboLowRes.m_Projection * lightUboLowRes.m_View * vec4(fragPosition, 1.0);
            vec3 lightSpacePosistionNDCLowRes = lightSpacePosistionLowRes.xyz / lightSpacePosistionLowRes.w;
            if (
                    abs(lightSpacePosistionNDCLowRes.x) > 1.0 ||
                    abs(lightSpacePosistionNDCLowRes.y) > 1.0 ||
                    abs(lightSpacePosistionNDCLowRes.z) > 1.0
                )
            {
                shadowPercentage = 1.0;
            }
            else
            {
                // Translate from NDC to shadow map space (Vulkan's Z is already in [0..1])
                vec2 shadowMapCoord = lightSpacePosistionNDCLowRes.xy * 0.5 + 0.5;

                // compute total number of samples to take from the shadow map
                int pcfSizeMinus1 = int(PCF_SIZE - 1);
                float kernelSize = 2.0 * pcfSizeMinus1 + 1.0;
                float numSamples = kernelSize * kernelSize;

                // Counter for the shadow map samples not in the shadow
                float litCount = 0.0;

                // Take samples from the shadow map
                float shadowmapTexelSize = 1.0 / SHADOWMAP_SIZE_LOW_RES;
                for (int x = -pcfSizeMinus1; x <= pcfSizeMinus1; x++)
                {
                    for (int y = -pcfSizeMinus1; y <= pcfSizeMinus1; y++)
                    {
                        // Compute coordinate for this PFC sample
                        vec2 pcfCoordinate = shadowMapCoord + vec2(x, y) * shadowmapTexelSize;

                        // Check if the sample is in light or in the shadow
                        if (lightSpacePosistionNDCLowRes.z <= texture(shadowMapTextureLowRes, pcfCoordinate).x)
                        {
                            litCount += 1.0;
                        }
                    }
                }
                shadowPercentage = litCount / numSamples;
            }
        }
        else
        {
            // Translate from NDC to shadow map space (Vulkan's Z is already in [0..1])
            vec2 shadowMapCoord = lightSpacePosistionNDCHiRes.xy * 0.5 + 0.5;

            // compute total number of samples to take from the shadow map
            int pcfSizeMinus1 = int(PCF_SIZE - 1);
            float kernelSize = 2.0 * pcfSizeMinus1 + 1.0;
            float numSamples = kernelSize * kernelSize;

            // Counter for the shadow map samples not in the shadow
            float litCount = 0.0;

            // Take samples from the shadow map
            float shadowmapTexelSize = 1.0 / SHADOWMAP_SIZE_HIRES_RES;
            for (int x = -pcfSizeMinus1; x <= pcfSizeMinus1; x++)
            {
                for (int y = -pcfSizeMinus1; y <= pcfSizeMinus1; y++)
                {
                    // Compute coordinate for this PFC sample
                    vec2 pcfCoordinate = shadowMapCoord + vec2(x, y) * shadowmapTexelSize;

                    // Check if the sample is in light or in the shadow
                    if (lightSpacePosistionNDCHiRes.z <= texture(shadowMapTextureHiRes, pcfCoordinate).x)
                    {
                        litCount += 1.0;
                    }
                }
            }
            shadowPercentage = litCount / numSamples;
        }

        // add to outgoing radiance Lo
        Lo += (kD * fragColor / PI + specular) * radiance * NdotL * shadowPercentage;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    }

    vec3 color = ambientLightColor + Lo;

    // HDR tonemapping
    //color = color / (color + vec3(1.0));
    color = ACESFilm(color);

    outColor = albedo * vec4(color, 1.0);

    // color correction
    outColor.xyz = pow(outColor.xyz, vec3(1.0/1.5)); 
}