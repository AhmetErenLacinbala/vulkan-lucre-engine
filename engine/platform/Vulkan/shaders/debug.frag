/* Engine Copyright (c) 2022 Engine Development Team 
   https://github.com/beaumanvienna/vulkan

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

// inputs
layout(location = 0)      in vec2  fragUV;

layout(set = 0, binding = 0) uniform sampler2D shadowMapTexture;

// outputs
layout (location = 0) out vec4 outColor;

void main()
{
    vec4 depthValue = texture(shadowMapTexture,fragUV);
    if (depthValue.x == 1.0)
    {
        outColor = vec4(1.0, 1.0, 1.0, 1.0);
    }
    else
    {
        outColor = vec4(0.0, 0.0, depthValue.x, 1.0);
    }
    //outColor = vec4(vec3(1.0 - (1.0 - depthValue) * 100.0), 1.0);
}