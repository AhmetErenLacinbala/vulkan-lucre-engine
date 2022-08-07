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

// inputs 
layout(push_constant) uniform Push
{
    mat4 m_MVP;
    vec2 m_UV[2];
} push;

// outputs
layout(location = 0) out vec2  fragUV;

// 0 - 1
// | / |
// 3 - 2
vec2 positions[6] = vec2[]
(
    vec2(-0.5,  0.5), // 0
    vec2( 0.5,  0.5), // 1
    vec2(-0.5, -0.5), // 3
    
    vec2( 0.5,  0.5), // 1
    vec2( 0.5, -0.5), // 2
    vec2(-0.5, -0.5)  // 3
);

vec2 _UV[2] = vec2[]
(
    vec2( 0.418457, 0.926758),
    vec2( 0.449707, 0.895508)
);

void main()
{
    vec2 position = positions[gl_VertexIndex];
    switch (gl_VertexIndex)
    {
        case 0:
            fragUV = vec2(push.m_UV[0].x, push.m_UV[0].y);
            break;
        case 1:
            fragUV = vec2(push.m_UV[1].x, push.m_UV[0].y);
            break;
        case 2:
            fragUV = vec2(push.m_UV[0].x, push.m_UV[1].y);
            break;
        case 3:
            fragUV = vec2(push.m_UV[1].x, push.m_UV[0].y);
            break;
        case 4:
            fragUV = vec2(push.m_UV[1].x, push.m_UV[1].y);
            break;
        case 5:
            fragUV = vec2(push.m_UV[0].x, push.m_UV[1].y);
            break;
    }

    gl_Position = vec4(position, 0.0, 1.0);
}
