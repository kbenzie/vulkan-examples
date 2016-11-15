#version 450

layout (std430, set=0, binding=0) buffer inA { int a[]; };
layout (std430, set=0, binding=1) buffer inB { int b[]; };
layout (std430, set=0, binding=2) buffer outR { int result[]; };

void main() {
  const uint i = gl_GlobalInvocationID.x;
  result[i] = a[i] + b[i];
}
