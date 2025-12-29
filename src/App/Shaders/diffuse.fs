#version 330 core

in vec2 vert_tex;

uniform float iterations;
uniform vec3 colorMult;
uniform vec2 pos;
uniform float zoomLog;

out vec4 out_col;

float compute_burning_ship(vec2 c) {
  c.y = -c.y;
	vec2 z = c;

  for (int i = 0; i < int(iterations); i++) {
    z = vec2(z.x * z.x - z.y * z.y, 2.0 * abs(z.x * z.y)) + c;
    // to see mandelbrot set uncomment this line and remove prev line:
    // z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;

    if (dot(z, z) > 4) {
      return float(i) / int(iterations);
    }
  }
  return 0.0f;
}

void main() {
	vec2 coords = vec2(vert_tex.xy) * 2.0 - vec2(1);

  vec2 ppc = pos + coords / exp(zoomLog);
	float res = compute_burning_ship(ppc);
  ppc /= 10;
  vec3 colorMult2 = colorMult + vec3(
    abs(sin(10 * ppc.x)),
    abs(sin(12 * ppc.y)),
    0.5 * abs(sin(ppc.x * 13) + cos(ppc.y * 13)));
	out_col = vec4(res * colorMult2, 1);
}