#version 330 core

in vec3 vColor;
out vec4 FragColor;
uniform int uPattern;
uniform float uTime;
uniform vec2 uResolution;

float stripes(vec2 uv){ return step(0.0, sin(uv.y*40.0 + uTime*4.0)); }
float dots(vec2 uv){ vec2 c = fract(uv*20.0)-0.5; return step(length(c), 0.25); }
float rings(vec2 uv){ float r = length(uv); return step(fract(r*30.0), 0.5); }

void main(){
    vec2 uv = gl_FragCoord.xy / uResolution;
    float m = 1.0;
    if (uPattern==1) m = stripes(uv);
    else if (uPattern==2) m = dots(uv);
    else if (uPattern==3) m = rings(uv);
    FragColor = vec4(vColor * m, 1.0);
}
