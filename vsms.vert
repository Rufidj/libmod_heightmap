#version 330 core

in vec2 bgd_Vertex;
in vec2 bgd_TexCoord;

out vec2 v_uv;

void main()
{
    // Convertir coordenadas de pantalla [-1,1] → [0,1]
    v_uv = bgd_Vertex * 0.5 + 0.5;

    gl_Position = vec4(bgd_Vertex, 0.0, 1.0);
}
