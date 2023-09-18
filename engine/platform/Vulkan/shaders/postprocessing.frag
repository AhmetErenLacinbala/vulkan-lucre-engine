/* Engine Copyright (c) 2023 Engine Development Team 
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

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput colorAttachment;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput emissiveMap;

layout(location = 0) out vec4 outColor;

void main()
{
    // retrieve G buffer data
    vec3 emissiveColor = subpassLoad(emissiveMap).rgb;
    // retrieve 3D pass main output color attachment
    vec3 inColor = subpassLoad(colorAttachment).rgb;

    if (emissiveColor == vec3(0,0,0))
    {
        outColor = vec4(inColor, 1.0);
    }
    else
    {
        outColor = vec4(inColor + emissiveColor, 1.0);
    }
}