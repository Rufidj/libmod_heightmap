/*
 * Heightmap Module Implementation for BennuGD2 - Con soporte de luz, agua y color de cielo
 */

#include "libmod_heightmap.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "libbggfx.h"  
#include <GL/glew.h>
#include <inttypes.h>  

// Si aún tienes problemas, puedes definir manualmente los offsets:  
#ifndef CTYPE  
enum {  
    CTYPE = 0,  
    CNUMBER,  
    COORDX,  
    COORDY,  
    COORDZ,  
    FILEID,  
    GRAPHID,  
    GRAPHSIZE,  
    ANGLE,  
    FLAGS,  
    REGIONID,  
    RESOLUTION,  
    GRAPHSIZEX,  
    GRAPHSIZEY,  
    XGRAPH,  
    _OBJECTID,  
    XGRAPH_FLAGS,  
    STATUS,  
    PROCESS_ID,  
    RENDER_FILEID,  
    RENDER_GRAPHID  
};  
#endif

#define MAX_BILLBOARDS 1000  
#define MAX_STATIC_BILLBOARDS 500  
#define MAX_DYNAMIC_BILLBOARDS 500  
  
typedef struct {    
    int active;    
    float world_x, world_y, world_z;    
    int graph_id;    
    int64_t process_id;  
    float scale;  
    int billboard_type;  // NUEVO CAMPO  
} VOXEL_BILLBOARD;

typedef struct {  
    uint32_t final_color;  // Color final con transparencia aplicada  
    uint8_t has_water;     // 1 si hay agua, 0 si no  
    float water_depth;     // Profundidad del agua para efectos  
} PRECALC_WATER_DATA;  

// Estructura temporal para ordenamiento de billboards  
typedef struct {  
    VOXEL_BILLBOARD *billboard;  
    BILLBOARD_PROJECTION projection;  
    GRAPH *graph;  
    float distance;  
} BILLBOARD_RENDER_DATA;
  

// Variables para textura del cielo  
static GRAPH *sky_texture = NULL;  
static float sky_texture_scale = 1.0f;  
static float sky_animation_speed = 0.3f;  
static float sky_texture_offset_x = 0.0f;  
static float sky_texture_offset_y = 0.0f;

// Variables globales para máscaras de overlay  
static GRAPH* overlay_mask = NULL;  
static GRAPH* bridge_texture = NULL;  
static float bridge_height_offset = 5.0f; // Altura adicional para puentes  


static PRECALC_WATER_DATA *precalc_water = NULL;
// Variables globales para el sistema de texturas de agua  
static GRAPH *water_texture = NULL;  
static int64_t water_texture_id = 0;  
static float water_texture_scale = 1.0f;  
static float water_texture_offset_x = 0.0f;  
static float water_texture_offset_y = 0.0f;  
static int water_texture_alpha_override = -1; // -1 = usar alpha de la textura, 0-255 = override
static float wave_amplitude = 0.1f; // Nueva variable para controlar el tamaño de las olas
  
static VOXEL_BILLBOARD static_billboards[MAX_STATIC_BILLBOARDS];  
static VOXEL_BILLBOARD dynamic_billboards[MAX_DYNAMIC_BILLBOARDS];  
static int static_billboard_count = 0;  
static int dynamic_billboard_count = 0;  

static int current_heightmap_id = 0; 
static GRAPH *render_buffer = NULL;
static float *fog_table = NULL;      
static int fog_table_size = 0;      
static int fog_table_initialized = 0;   

static float cached_fog_intensity = -1.0f;  
static Uint8 cached_fog_r = 0, cached_fog_g = 0, cached_fog_b = 0;

// Variables globales para renderizado GPU  
static BGD_SHADER *voxel_shader = NULL;  
static BGD_SHADER_PARAMETERS *voxel_params = NULL;  
static int glew_initialized = 0;  
static int gpu_textures_uploaded = 0;  // NUEVO: flag para evitar subir texturas cada frame  
static int64_t gpu_current_heightmap_id = -1;  // NUEVO: track cual heightmap está en GPU
static GLuint voxel_fbo = 0;    
static GLuint voxel_render_texture = 0;
// NUEVO: Variables para OpenGL directo  
static GLuint gl_shader_program = 0;  
static GLuint gl_vertex_shader = 0;  
static GLuint gl_fragment_shader = 0;  
static GLuint gl_vao = 0;  
static GLuint gl_vbo = 0;  
static GLuint gl_heightmap_tex = 0;  
static GLuint gl_texturemap_tex = 0;
static int64_t cached_heightmap_id = -1;  
static int upload_textures_to_opengl(HEIGHTMAP *hm);

#define BILLBOARD_TYPE_STATIC     0  
#define BILLBOARD_TYPE_PLAYER     1  
#define BILLBOARD_TYPE_ENEMY      2  
#define BILLBOARD_TYPE_PROJECTILE 3

#define C_BILLBOARD 999  
  
// Declaración forward corregida  
int64_t libmod_heightmap_world_to_screen(INSTANCE *my, int64_t *params);
uint32_t calculate_water_color(float world_x, float world_y, float water_depth, float time);
static float calculated_bridge_height = -1.0f;  

  
#ifndef M_PI_2
#define M_PI_2 1.57079632679f
#endif

#define RGBA32_R(color) ((color >> 16) & 0xFF)
#define RGBA32_G(color) ((color >> 8) & 0xFF)
#define RGBA32_B(color) (color & 0xFF)
#define RGBA32_A(color) ((color >> 24) & 0xFF)

// Variables globales para configuración de renderizado
static float max_render_distance = 8000.0f;
static int chunk_size = 512;
static int chunk_radius = 15;

// Variables globales para el color del cielo
static Uint8 sky_color_r = 135; // Azul cielo por defecto
static Uint8 sky_color_g = 206;
static Uint8 sky_color_b = 235;
static Uint8 sky_color_a = 255; // Alpha del cielo

// Variables para niebla mejorada  
static float fog_vertical_gradient = 0.5f;  
static Uint8 fog_color_r = 200;  
static Uint8 fog_color_g = 200;   
static Uint8 fog_color_b = 200;  
static float fog_intensity = 0.0f;


// Variables globales para el color y la transparencia del agua
static Uint8 water_color_r = 64;
static Uint8 water_color_g = 128;
static Uint8 water_color_b = 255;
static Uint8 water_color_a = 128; // Valor inicial para transparencia (0-255)
static float water_time = 0.0f;

// Variable global para almacenar el ID del sprite a seguir
static int64_t camera_follow_sprite_id = 0;
static float camera_follow_offset_x = 0.0f;
static float camera_follow_offset_y = 0.0f;
static float camera_follow_offset_z = 10.0f;
static float camera_follow_speed = 5.0f;

// fov para billboards
static float billboard_render_fov = 1.5f;  // FOV independiente para billboards


// Declaración por si no está en el header
void gr_alpha_put_pixel(GRAPH *dest, int x, int y, uint32_t color, uint8_t alpha);

HEIGHTMAP heightmaps[MAX_HEIGHTMAPS];
CAMERA_3D camera = {0, 0, 0, 0, 0, DEFAULT_FOV, DEFAULT_NEAR, DEFAULT_FAR};
int64_t next_heightmap_id = 1;
float water_level = -1.0f;
int light_intensity = 255;

static float mouse_sensitivity = 50.0f;
static float move_speed = 3.0f;
static float height_speed = 3.0f;

float get_height_at(HEIGHTMAP *hm, float x, float y);
void build_height_cache(HEIGHTMAP *hm);
void clamp_camera_to_terrain(HEIGHTMAP *hm);
void clamp_camera_to_bounds(HEIGHTMAP *hm);
uint32_t get_texture_color_bilinear(GRAPH *texture, float x, float y);
int64_t libmod_heightmap_project_billboard(INSTANCE *my, int64_t *params);
static HEIGHTMAP* find_heightmap_by_id(int64_t hm_id);  
static int apply_terrain_collision(HEIGHTMAP *hm, float new_x, float new_y);  
static int move_camera_with_collision(int64_t hm_id, float angle_offset, float speed_factor, float speed);
static float convert_screen_to_world_coordinate(int heightmap_id, float screen_coord, int is_x_axis);
static void collect_visible_billboards_from_array(VOXEL_BILLBOARD *billboard_array, int array_size,   
                                                  BILLBOARD_RENDER_DATA *visible_billboards,   
                                                  int *visible_count, float terrain_fov);

static int initialize_glew_if_needed() {  
    if (glew_initialized) return 1;  
      
    GLenum err = glewInit();  
    if (err != GLEW_OK) {  
        fprintf(stderr, "ERROR: glewInit falló: %s\n", glewGetErrorString(err));  
        return 0;  
    }  
      
    fprintf(stderr, "GLEW inicializado correctamente\n");  
    glew_initialized = 1;  
    return 1;  
}

static void cleanup_gpu_resources() {  
    voxel_shader = NULL;  
    voxel_params = NULL;  
      
    if (voxel_fbo) {  
        glDeleteFramebuffers(1, &voxel_fbo);  
        voxel_fbo = 0;  
    }  
      
    if (voxel_render_texture) {  
        glDeleteTextures(1, &voxel_render_texture);  
        voxel_render_texture = 0;  
    }  
      
    if (gl_shader_program) {  
        glDeleteProgram(gl_shader_program);  
        gl_shader_program = 0;  
    }  
      
    if (gl_vertex_shader) {  
        glDeleteShader(gl_vertex_shader);  
        gl_vertex_shader = 0;  
    }  
      
    if (gl_fragment_shader) {  
        glDeleteShader(gl_fragment_shader);  
        gl_fragment_shader = 0;  
    }  
      
    if (gl_vao) {  
        glDeleteVertexArrays(1, &gl_vao);  
        gl_vao = 0;  
    }  
      
    if (gl_vbo) {  
        glDeleteBuffers(1, &gl_vbo);  
        gl_vbo = 0;  
    }  
      
    if (gl_heightmap_tex) {  
        glDeleteTextures(1, &gl_heightmap_tex);  
        gl_heightmap_tex = 0;  
    }  
      
    if (gl_texturemap_tex) {  
        glDeleteTextures(1, &gl_texturemap_tex);  
        gl_texturemap_tex = 0;  
    }  
      
    cached_heightmap_id = -1;  
}

void libmod_heightmap_destroy_render_buffer() {  
    if (render_buffer) {  
        bitmap_destroy(render_buffer);  
        render_buffer = NULL;  
    }  
}

void __bgdexport(libmod_heightmap, module_initialize)()  
{  
    memset(heightmaps, 0, sizeof(heightmaps));  
    memset(static_billboards, 0, sizeof(static_billboards));  
    memset(dynamic_billboards, 0, sizeof(dynamic_billboards));  
    static_billboard_count = 0;  
    next_heightmap_id = 1;  
}

void __bgdexport(libmod_heightmap, module_finalize)()  
{  
    // Liberar recursos asociados a cada heightmap  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)  
    {  
        if (heightmaps[i].heightmap) // Verificar si el heightmap existe/está en uso  
        {  
            // Liberar el cache de altura si existe  
            if (heightmaps[i].height_cache)  
            {  
                free(heightmaps[i].height_cache);  
                heightmaps[i].height_cache = NULL;  
            }  
  
            // Destruir el GRAPH del heightmap principal  
            bitmap_destroy(heightmaps[i].heightmap);  
            heightmaps[i].heightmap = NULL;  
  
            // Destruir el GRAPH del texturemap si existe  
            if (heightmaps[i].texturemap)  
            {  
                bitmap_destroy(heightmaps[i].texturemap);  
                heightmaps[i].texturemap = NULL;  
            }  
        }  
        cleanup_gpu_resources();
    }  
  
    // Liberar el render_buffer global una sola vez  
    libmod_heightmap_destroy_render_buffer();  
  
    // Liberar el fog_table global si existe (como se discutió previamente)  
    if (fog_table) {  
        free(fog_table);  
        fog_table = NULL;  
    }  
  
    // Limpiar la estructura global heightmaps (opcional, pero buena práctica)  
    memset(heightmaps, 0, sizeof(heightmaps));  
}


int64_t libmod_heightmap_set_light(INSTANCE *my, int64_t *params)
{
    int level = params[0];
    if (level < 0)
        level = 0;
    if (level > 255)
        level = 255;
    light_intensity = level;
    return 1;
}

int64_t libmod_heightmap_set_water_level(INSTANCE *my, int64_t *params)
{
    water_level = (float)params[0];
    return 1;
}

/* Cargar mapa de altura desde archivo */
int64_t libmod_heightmap_load(INSTANCE *my, int64_t *params)
{
    const char *filename = string_get(params[0]);
    int64_t map_id = gr_load_img(filename);
    string_discard(params[0]);

    if (!map_id)
        return 0;
    GRAPH *graph = bitmap_get(0, map_id);
    if (!graph)
        return 0;

    int slot = -1;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (!heightmaps[i].heightmap)
        {
            slot = i;
            break;
        }
    }

        if (slot == -1) {  
        fprintf(stderr, "Error: MAX_HEIGHTMAPS (%d) alcanzado\n", MAX_HEIGHTMAPS);  
        return 0;  
    }  
      
    // Verificar que next_heightmap_id no desborde  
    if (next_heightmap_id >= INT64_MAX - 1) {  
        fprintf(stderr, "Error: next_heightmap_id overflow\n");  
        return 0;  
    }

    heightmaps[slot].id = next_heightmap_id++;
    heightmaps[slot].heightmap = graph;
    heightmaps[slot].texturemap = NULL;
    heightmaps[slot].width = graph->width;
    heightmaps[slot].height = graph->height;
    heightmaps[slot].height_cache = NULL;
    heightmaps[slot].cache_valid = 0;

    build_height_cache(&heightmaps[slot]);

    return heightmaps[slot].id;
}


/* Crear mapa de altura en memoria */
int64_t libmod_heightmap_create(INSTANCE *my, int64_t *params)
{
    int64_t width = params[0];
    int64_t height = params[1];

    GRAPH *graph = bitmap_new_syslib(width, height);
    if (!graph)
        return 0;

    int slot = -1;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (!heightmaps[i].heightmap)
        {
            slot = i;
            break;
        }
    }

       if (slot == -1) {  
        fprintf(stderr, "Error: MAX_HEIGHTMAPS (%d) alcanzado\n", MAX_HEIGHTMAPS);  
        return 0;  
    }  
      
    // Verificar que next_heightmap_id no desborde  
    if (next_heightmap_id >= INT64_MAX - 1) {  
        fprintf(stderr, "Error: next_heightmap_id overflow\n");  
        return 0;  
    }

    heightmaps[slot].id = next_heightmap_id++;
    heightmaps[slot].heightmap = graph;
    heightmaps[slot].texturemap = NULL;
    heightmaps[slot].width = width;
    heightmaps[slot].height = height;
    heightmaps[slot].height_cache = NULL;
    heightmaps[slot].cache_valid = 0;

    build_height_cache(&heightmaps[slot]);
    if (!heightmaps[slot].cache_valid)
    {
        bitmap_destroy(heightmaps[slot].heightmap);
        memset(&heightmaps[slot], 0, sizeof(HEIGHTMAP));
        return 0;
    }

    return heightmaps[slot].id; // ← Esta línea faltaba
} // ← Esta llave de cierre faltaba

/* Descargar mapa de altura */
int64_t libmod_heightmap_unload(INSTANCE *my, int64_t *params)  
{  
    int64_t hm_id = params[0];  
  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)  
    {  
        if (heightmaps[i].id == hm_id)  
        {  
            if (heightmaps[i].height_cache)  
            {  
                free(heightmaps[i].height_cache);  
                heightmaps[i].height_cache = NULL;  
            }  
  
            // Destruir correctamente la estructura GRAPH  
            if (heightmaps[i].heightmap)  
            {  
                bitmap_destroy(heightmaps[i].heightmap);  
                heightmaps[i].heightmap = NULL;  
            }  
  
            // Limpiar mapa de textura si existe  
            if (heightmaps[i].texturemap)  
            {  
                bitmap_destroy(heightmaps[i].texturemap);  
                heightmaps[i].texturemap = NULL;  
            }  
  
            memset(&heightmaps[i], 0, sizeof(HEIGHTMAP));  
            return 1;  
        }  
    }  
      
    return 0;  
}

/* Obtener altura */
int64_t libmod_heightmap_get_height(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    float x = (float)params[1];
    float y = (float)params[2];

  HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
if (hm) {  
    float height = get_height_at(hm, x, y);  
    return (int64_t)(height * 1000);  
}  
return 0;
}

float get_height_at(HEIGHTMAP *hm, float x, float y) {    
    // Código original para heightmaps tradicionales    
    if (!hm->cache_valid)    
        return 0;    
    
    int ix = (int)x;    
    int iy = (int)y;    
    
    if (ix < 0 || ix >= hm->width - 1 || iy < 0 || iy >= hm->height - 1)    
        return 0;    
    
    float fx = x - ix;    
    float fy = y - iy;    
    
    float h00 = hm->height_cache[iy * hm->width + ix];    
    float h10 = hm->height_cache[iy * hm->width + (ix + 1)];    
    float h01 = hm->height_cache[(iy + 1) * hm->width + ix];    
    float h11 = hm->height_cache[(iy + 1) * hm->width + (ix + 1)];    
    
    float h0 = h00 + fx * (h10 - h00);    
    float h1 = h01 + fx * (h11 - h01);    
    float h = h0 + fy * (h1 - h0);    
    
    return h;    
}


/* Configurar cámara 3D - Original */
int64_t libmod_heightmap_set_camera(INSTANCE *my, int64_t *params)
{
    camera.x = (float)params[0];
    camera.y = (float)params[1];
    camera.z = (float)params[2];
    camera.angle = (float)params[3] / 1000.0f;
    camera.pitch = (float)params[4] / 1000.0f;
    camera.fov = (params[5] > 0) ? (float)params[5] : DEFAULT_FOV;

    // Limitación de pitch original
    const float max_pitch = M_PI_2 * 0.99f;
    if (camera.pitch > max_pitch)
        camera.pitch = max_pitch;
    if (camera.pitch < -max_pitch)
        camera.pitch = -max_pitch;

    return 1;
}

/* Cargar textura */
int64_t libmod_heightmap_load_texture(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    const char *filename = string_get(params[1]);

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm)
    {
        string_discard(params[1]);
        return 0;
    }

    int64_t map_id = gr_load_img(filename);
    string_discard(params[1]);
    if (!map_id)
        return 0;

    GRAPH *graph = bitmap_get(0, map_id);
    if (!graph)
        return 0;

    hm->texturemap = graph;
    return 1;
}

int64_t libmod_heightmap_set_sky_texture(INSTANCE *my, int64_t *params)  
{  
    const char *filename = string_get(params[0]);  
    float scale = (params[1] > 0) ? (float)params[1] / 1000.0f : 1.0f;  
      
    if (sky_texture) {  
        bitmap_destroy(sky_texture);  
        sky_texture = NULL;  
    }  
      
    int64_t map_id = gr_load_img(filename);  
    string_discard(params[0]);  
    if (!map_id) return 0;  
      
    sky_texture = bitmap_get(0, map_id);  
    if (!sky_texture) return 0;  
      
    sky_texture_scale = scale;  
    return 1;  
}
 
// Función auxiliar para samplear la textura del cielo con proyección esférica corregida  
static uint32_t sample_sky_texture(float screen_x, float screen_y, float camera_angle, float camera_pitch, float time) {  
    if (!sky_texture) {  
        return SDL_MapRGBA(gPixelFormat, sky_color_r, sky_color_g, sky_color_b, sky_color_a);  
    }  
      
    // Sincronizar FOV con el sistema voxelspace  
    float terrain_fov = 0.1f; // Mismo valor que el renderizado principal  
    float fov_horizontal = terrain_fov;  
    float fov_vertical = terrain_fov * 0.75f; // Proporción ajustada  
      
    // Calcular ángulos del rayo de manera consistente con voxelspace  
    float ray_angle_h = ((screen_x - 160.0f) / 320.0f) * fov_horizontal;  
    float ray_angle_v = ((120.0f - screen_y) / 240.0f) * fov_vertical;  
      
    // Aplicar rotación de cámara  
    float world_angle_h = camera_angle + ray_angle_h;  
    float world_angle_v = camera_pitch + ray_angle_v;  
      
    // Mapeo UV estabilizado  
    float u = (world_angle_h + M_PI) / (2.0f * M_PI);  
    float v = (world_angle_v + M_PI_2) / M_PI;  
      
    // Clamping para evitar repetición  
    if (u < 0.0f) u = 0.0f;  
    if (u > 1.0f) u = 1.0f;  
    if (v < 0.0f) v = 0.0f;  
    if (v > 1.0f) v = 1.0f;  
      
    // Sampling de textura  
    int tex_x = (int)(u * sky_texture->width) % sky_texture->width;  
    int tex_y = (int)(v * sky_texture->height) % sky_texture->height;  
      
    return gr_get_pixel(sky_texture, tex_x, tex_y);  
}

// Función de renderizado del skybox corregida  
static void render_skybox(float camera_angle, float camera_pitch, float time, int quality_step) {  
    if (!sky_texture) {  
        // Fallback al sistema actual  
        uint32_t background_color = SDL_MapRGBA(gPixelFormat, sky_color_r, sky_color_g, sky_color_b, sky_color_a);  
        gr_clear_as(render_buffer, background_color);  
        return;  
    }  
      
    // Primero limpiar solo con el color de fondo, sin sobrescribir  
    uint32_t background_color = SDL_MapRGBA(gPixelFormat, sky_color_r, sky_color_g, sky_color_b, sky_color_a);  
    gr_clear_as(render_buffer, background_color);  
      
    // Renderizar skybox por sectores para optimización  
    // IMPORTANTE: Solo renderizar en áreas que realmente son cielo  
    for (int y = 0; y < 240; y += quality_step) {  
        for (int x = 0; x < 320; x += quality_step) {  
            // Verificar si este píxel debería mostrar cielo  
            // (esto se determinará después en el pipeline de renderizado principal)  
            uint32_t sky_color = sample_sky_texture((float)x, (float)y, camera_angle, camera_pitch, time);  
              
            // Rellenar el área del quality_step SOLO como fondo inicial  
            // El renderizado del terreno y agua sobrescribirá estos píxeles según corresponda  
            for (int dy = 0; dy < quality_step && (y + dy) < 240; dy++) {  
                for (int dx = 0; dx < quality_step && (x + dx) < 320; dx++) {  
                    gr_put_pixel(render_buffer, x + dx, y + dy, sky_color);  
                }  
            }  
        }  
    }  
}

static BILLBOARD_PROJECTION calculate_proyection(VOXEL_BILLBOARD *bb, GRAPH *billboard_graph, float terrain_fov) {    
    BILLBOARD_PROJECTION result = {0};    
        
    // Cálculo de distancia 3D real    
    float dx = bb->world_x - camera.x;    
    float dy = bb->world_y - camera.y;    
    float dz = bb->world_z - camera.z;    
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);    
        
    // MODIFICADO: Extender el rango para permitir fade-out  
    if (distance > max_render_distance * 1.1f || distance < 0.5f) {    
        result.valid = 0;    
        return result;    
    }    
        
    result.distance = distance;  
      
    // Usar el mismo FOV que el terreno  
    float effective_fov = terrain_fov;  
    float billboard_angle = atan2f(dy, dx);  
    float angle_diff = billboard_angle - camera.angle;  
      
    while (angle_diff > M_PI) angle_diff -= 2.0f * M_PI;  
    while (angle_diff < -M_PI) angle_diff += 2.0f * M_PI;  
      
    float terrain_fov_half = effective_fov * 0.5f;  
      
    // MODIFICADO: Extender FOV para permitir fade-out en bordes  
    if (fabs(angle_diff) > terrain_fov_half * 1.2f) {  
        result.valid = 0;  
        return result;  
    }  
      
    // Proyección de pantalla  
    float screen_x_float = 160.0f + (angle_diff / effective_fov) * 320.0f;  
      
    // MODIFICADO: Extender límites de pantalla para fade-out  
    if (screen_x_float < -400.0f || screen_x_float >= 720.0f) {  
        result.valid = 0;  
        return result;  
    }  
      
    result.screen_x = (int)screen_x_float;  
      
    // Resto de la proyección vertical y escalado...  
    float cos_angle = cosf(camera.angle);    
    float sin_angle = sinf(camera.angle);    
    float cam_forward = dx * cos_angle + dy * sin_angle;  
      
    if (cam_forward <= 0.1f) {    
        result.valid = 0;    
        return result;    
    }  
      
    float height_on_screen = 120.0f + (camera.z - bb->world_z) / cam_forward * 300.0f;  
    height_on_screen += camera.pitch * 40.0f;  
      
    if (height_on_screen < -400.0f || height_on_screen >= 640.0f) {  
        result.valid = 0;  
        return result;  
    }  
      
    result.screen_y = (int)height_on_screen;  
      
    // Escalado (sin cambios)  
    float base_scale_factor;  
    float max_scale, min_scale;  
      
    switch(bb->billboard_type) {  
        case BILLBOARD_TYPE_PLAYER:  
            base_scale_factor = 30.0f;  
            max_scale = 1.2f;  
            min_scale = 0.3f;  
            break;  
        case BILLBOARD_TYPE_ENEMY:  
            base_scale_factor = 120.0f;  
            max_scale = 6.0f;  
            min_scale = 0.1f;  
            break;  
        case BILLBOARD_TYPE_PROJECTILE:  
            base_scale_factor = 80.0f;  
            max_scale = 4.0f;  
            min_scale = 0.05f;  
            break;  
        default:  
            base_scale_factor = 150.0f;  
            max_scale = 8.0f;  
            min_scale = 0.05f;  
    }  
      
    result.distance_scale = base_scale_factor / cam_forward;  
    if (result.distance_scale > max_scale) result.distance_scale = max_scale;  
    if (result.distance_scale < min_scale) result.distance_scale = min_scale;  
      
    result.scaled_width = (int)(result.distance_scale * billboard_graph->width);  
    result.scaled_height = (int)(result.distance_scale * billboard_graph->height);  
      
    if (result.scaled_width < 1) result.scaled_width = 1;  
    if (result.scaled_height < 1) result.scaled_height = 1;  
      
    // CLAVE: Cálculo de alpha con fade-out REAL  
    int fog_index = (int)distance;  
    if (fog_index >= fog_table_size) fog_index = fog_table_size - 1;  
    float fog = fog_table[fog_index];  
      
    // Fade-out por distancia  
    if (distance > max_render_distance * 0.7f) {  
        float fade_range = max_render_distance * 0.3f; // 30% del rango para fade  
        float fade_progress = (distance - max_render_distance * 0.7f) / fade_range;  
        float distance_fade = 1.0f - fade_progress;  
        distance_fade = fmaxf(0.0f, fminf(1.0f, distance_fade));  
        fog *= distance_fade;  
    }  
      
    // Fade-out por FOV  
    if (fabs(angle_diff) > terrain_fov_half * 0.7f) {  
        float fov_fade_range = terrain_fov_half * 0.3f;  
        float fov_fade_progress = (fabs(angle_diff) - terrain_fov_half * 0.7f) / fov_fade_range;  
        float fov_fade = 1.0f - fov_fade_progress;  
        fov_fade = fmaxf(0.0f, fminf(1.0f, fov_fade));  
        fog *= fov_fade;  
    }  
      
    result.alpha = (Uint8)(255 * fog);  
    // NUEVO: Aplicar tintado de niebla a billboards  
if (fog_intensity > 0.0f && distance > max_render_distance * 0.3f) {  
    float fog_start = max_render_distance * 0.3f;  
    float fog_range = max_render_distance - fog_start;  
    float fog_progress = (distance - fog_start) / fog_range;  
    fog_progress = fog_progress * fog_progress;  
      
    float fog_tint_factor = fog_progress * fog_intensity * 0.3f;  
    if (fog_tint_factor > 0.5f) fog_tint_factor = 0.5f;  
      
    // Almacenar el factor de tintado para usar en el renderizado  
    result.fog_tint_factor = fog_tint_factor;  
} else {  
    result.fog_tint_factor = 0.0f;  
}
      
    result.valid = 1;  
    return result;  
}

// Función de comparación para qsort (más lejanos primero)  
int compare_billboards_by_distance(const void *a, const void *b) {  
    BILLBOARD_RENDER_DATA *bb_a = (BILLBOARD_RENDER_DATA *)a;  
    BILLBOARD_RENDER_DATA *bb_b = (BILLBOARD_RENDER_DATA *)b;  
      
    // Renderizar primero los más lejanos (mayor distancia)  
    if (bb_a->distance > bb_b->distance) return -1;  
    if (bb_a->distance < bb_b->distance) return 1;  
    return 0;  
}

int64_t libmod_heightmap_render_voxelspace(INSTANCE *my, int64_t *params) {    
    int64_t hm_id = params[0];    
    HEIGHTMAP *hm = NULL;    
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {    
        if (heightmaps[i].id == hm_id) {    
            hm = &heightmaps[i];    
            break;    
        }    
    }    
        
    if (!hm || !hm->cache_valid)    
        return 0;    
            
    if (!render_buffer) {    
        render_buffer = bitmap_new_syslib(320, 240);    
        if (!render_buffer) return 0;    
    }    
        
    static float *depth_buffer = NULL;    
    if (!depth_buffer) {    
        depth_buffer = malloc(320 * 240 * sizeof(float));    
    }    
        
    for (int i = 0; i < 320 * 240; i++) {    
        depth_buffer[i] = max_render_distance;    
    }    
        
    uint32_t background_color = SDL_MapRGBA(gPixelFormat, sky_color_r, sky_color_g, sky_color_b, sky_color_a);    
        
    static float last_camera_x = 0, last_camera_y = 0, last_camera_angle = 0;    
    float movement = fabs(camera.x - last_camera_x) + fabs(camera.y - last_camera_y) + fabs(camera.angle - last_camera_angle);    
    int quality_step = (movement > 15.0f) ? 2 : 1;  // Umbral más alto
    last_camera_x = camera.x;    
    last_camera_y = camera.y;    
    last_camera_angle = camera.angle;    
        
    int chunk_x = (int)(camera.x / chunk_size);    
    int chunk_y = (int)(camera.y / chunk_size);    
    int min_chunk_x = chunk_x - chunk_radius;    
    int max_chunk_x = chunk_x + chunk_radius;    
    int min_chunk_y = chunk_y - chunk_radius;    
    int max_chunk_y = chunk_y + chunk_radius;    
        
    float terrain_fov = 0.7f;    
    float angle_step = terrain_fov / 320.0f;    
    float base_angle = camera.angle - terrain_fov * 0.5f;    
    float pitch_offset = camera.pitch * 40.0f;    
    float light_factor = light_intensity / 255.0f;    
        
    float time = SDL_GetTicks() / 1000.0f;    
    static float cached_water_time = 0.0f;    
    static int water_frame_counter = 0;    
        
    if (water_frame_counter % 4 == 0) {    
        cached_water_time = time;    
    }    
    water_frame_counter++;    
    render_skybox(camera.angle, camera.pitch, cached_water_time, quality_step);  
      
   
        
    int camera_underwater = (camera.z < water_level);    
        
    if (camera_underwater) {    
        light_factor *= 0.7f;    
        background_color = SDL_MapRGBA(gPixelFormat,    
            sky_color_r * 0.5f, sky_color_g * 0.7f, sky_color_b * 1.0f, sky_color_a);    
        gr_clear_as(render_buffer, background_color);    
    }    
        
    if (!fog_table_initialized || fog_table_size != (int)max_render_distance) {    
        if (fog_table)    
            free(fog_table);    
        fog_table_size = (int)max_render_distance;    
        fog_table = malloc(fog_table_size * sizeof(float));    
            
        for (int i = 0; i < fog_table_size; i++) {      
        float fog = 1.0f - (i / (float)fog_table_size);  
        // Solo aplicar mínimo si fog_intensity > 0  
        if (fog_intensity > 0.0f) {  
        fog_table[i] = (fog < 0.6f) ? 0.6f : fog;  
        } else {  
        fog_table[i] = fog;  // Sin mínimo cuando fog_intensity = 0  
    }  
}
        fog_table_initialized = 1;    
    }    
     
        // NUEVO: Precalcular cos/sin para todas las columnas  
        static float cos_cache[320];  
        static float sin_cache[320];  
        for (int i = 0; i < 320; i++) {  
        float angle = base_angle + i * angle_step;  
        cos_cache[i] = cosf(angle);  
        sin_cache[i] = sinf(angle);  
    }
        // Precalcular límites de FOV (ANTES del loop)  
        float camera_fov_half = terrain_fov * 0.5f;      
        float min_angle = camera.angle - camera_fov_half;      
        float max_angle = camera.angle + camera_fov_half;  
  
        for (int screen_x = 0; screen_x < 320; screen_x += quality_step) {      
        float angle = base_angle + screen_x * angle_step;      
          
        if (angle < min_angle || angle > max_angle)      
        continue;      
          
        float cos_angle = cos_cache[screen_x];  
        float sin_angle = sin_cache[screen_x];  
        int lowest_y = 240;    
            
        for (float distance = 1.0f; distance < max_render_distance;   
        distance += (distance < 50.0f ? 0.3f : distance < 200.0f ? 0.8f : 1.5f)){    
            float world_x = camera.x + cos_angle * distance;    
            float world_y = camera.y + sin_angle * distance;    
            
            if (world_x < 0 || world_x >= hm->width - 1 || world_y < 0 || world_y >= hm->height - 1)    
                continue;    
            
            int current_chunk_x = (int)(world_x / chunk_size);    
            int current_chunk_y = (int)(world_y / chunk_size);    
            if (current_chunk_x < min_chunk_x || current_chunk_x > max_chunk_x ||    
                current_chunk_y < min_chunk_y || current_chunk_y > max_chunk_y) {    
                continue;    
            }    
            
            float terrain_height = get_height_at(hm, world_x, world_y);    
              
            // Renderizar terreno/agua según su altura real    
            float render_height;    
            int render_water = 0;    
                
            if (water_level > 0 && terrain_height < water_level) {    
                float simple_wave = sin(cached_water_time + world_x * 0.05f) * wave_amplitude;  
                render_height = water_level + simple_wave;    
                render_water = 1;    
            } else {    
                
                render_height = terrain_height;    
            }    
                
            float height_on_screen = (camera.z - render_height) / distance * 300.0f + 120.0f;    
            height_on_screen += pitch_offset;    
                
            int screen_y = (int)height_on_screen;    
            if (screen_y < 0)    
                screen_y = 0;    
            if (screen_y >= 240)    
                continue;    
                
            if (screen_y < lowest_y) {    
                int fog_index = (int)distance;    
                if (fog_index >= fog_table_size)    
                    fog_index = fog_table_size - 1;    
                float fog = fog_table[fog_index];    
                    
                if (render_water) {    
                    // Renderizar agua (sin cambios)  
                if (water_texture && water_texture->width > 0 && water_texture->height > 0) {  
                float u = (world_x * 0.01f + cached_water_time * 0.1f);  
                float v = (world_y * 0.01f + cached_water_time * 0.05f);  
      
                u = u - floor(u);  
                v = v - floor(v);  
      
                int tex_x = (int)(u * water_texture->width);  
                int tex_y = (int)(v * water_texture->height);  
      
                 // Clamp en lugar de modulo para evitar valores negativos  
                tex_x = (tex_x < 0) ? 0 : (tex_x >= water_texture->width) ? water_texture->width - 1 : tex_x;  
                tex_y = (tex_y < 0) ? 0 : (tex_y >= water_texture->height) ? water_texture->height - 1 : tex_y;   
                            
                      //                        uint32_t water_color = gr_get_pixel(water_texture, tex_x, tex_y);
//
                        BGD_Rect clip;
                        clip.x = tex_x;
                        clip.y = tex_y;
                        clip.w = 1;
                        clip.h = 1;

//                        {
//                            uint8_t r, g, b;
//                            SDL_GetRGB(water_color, gPixelFormat, &r, &g, &b);
//                            water_color = SDL_MapRGBA(gPixelFormat, r, g, b, 128);
//                        }

                        gr_blit(render_buffer, NULL,
                               screen_x, screen_y,
                               0,
                               0,
                               100, 100*(lowest_y-screen_y+1),
                               0, 0,
                               water_texture, &clip, 128, 255, 255, 255, BLEND_NORMAL, NULL);

                        for (int y = screen_y; y < lowest_y; y++) {
//                            gr_put_pixel(render_buffer, screen_x, y, water_color);
                            int depth_index = y * 320 + screen_x;
                            if (depth_index >= 0 && depth_index < 320 * 240) {
                                depth_buffer[depth_index] = distance;
                            }
                        } 
                    }
                } else {    
                    // Renderizar terreno normal con efectos atmosféricos avanzados  
                    GRAPH* texture_to_use = hm->texturemap;    
                    Uint8 terrain_r = 0, terrain_g = 0, terrain_b = 0;    
                        
                    if (texture_to_use) {    
                        uint32_t tex = get_texture_color_bilinear(texture_to_use, world_x, world_y);    
                        if (tex == 0) {    
                            int tx = (int)world_x;    
                            int ty = (int)world_y;    
                            while (tx >= texture_to_use->width)    
                                tx -= texture_to_use->width;    
                            while (ty >= texture_to_use->height)    
                                ty -= texture_to_use->height;    
                            while (tx < 0)    
                                tx += texture_to_use->width;    
                            while (ty < 0)    
                                ty += texture_to_use->height;    
                            tex = gr_get_pixel(texture_to_use, tx, ty);    
                        }    

                    // DESPUÉS (versión correcta):  
                    terrain_r = (tex >> gPixelFormat->Rshift) & 0xFF;  
                    terrain_g = (tex >> gPixelFormat->Gshift) & 0xFF;  
                    terrain_b = (tex >> gPixelFormat->Bshift) & 0xFF;

                    } else {    
                        int base = (int)(terrain_height * 2.5f) + 20;    
                        if (base > 255) base = 255;    
                        if (base < 0) base = 0;    
                            
                        int grid_x = (int)(world_x * 2.0f) % 8;    
                        int grid_y = (int)(world_y * 2.0f) % 8;    
                        int grid_variation = (grid_x + grid_y) % 3 - 1;    
                        base += grid_variation * 15;    
                        if (base > 255) base = 255;    
                        if (base < 0) base = 0;    
                            
                        terrain_r = (Uint8)((base + 60));    
                        terrain_g = (Uint8)((base + 30));    
                        terrain_b = (Uint8)(base);    
                    }    
                        
                 uint32_t terrain_color = SDL_MapRGB(gPixelFormat, terrain_r, terrain_g, terrain_b);  
// NIEBLA FORZADA - después de la línea 946  
if (fog_intensity > 0.0f) {  
    // Aplicar niebla desde distancias más cercanas  
    if (distance > max_render_distance * 0.2f) {  
        float fog_progress = (distance - max_render_distance * 0.2f) / (max_render_distance * 0.8f);  
          
        // Factor de niebla MUY agresivo para que se vea  
        float fog_factor = fog_progress * fog_intensity * 2.0f; // Multiplicador x2  
          
        if (fog_factor > 0.9f) fog_factor = 0.9f; // Permitir hasta 90% de niebla  
          
        if (fog_factor > 0.1f) { // Aplicar desde 10% en adelante  
            float terrain_factor = 1.0f - fog_factor;  
              
            Uint8 final_r = (Uint8)(terrain_r * terrain_factor + fog_color_r * fog_factor);  
            Uint8 final_g = (Uint8)(terrain_g * terrain_factor + fog_color_g * fog_factor);  
            Uint8 final_b = (Uint8)(terrain_b * terrain_factor + fog_color_b * fog_factor);  
              
            terrain_color = SDL_MapRGB(gPixelFormat, final_r, final_g, final_b);  
        }  
    }  
}
                    for (int y = screen_y; y < lowest_y; y++) {    
                        gr_put_pixel(render_buffer, screen_x, y, terrain_color);    
                        int depth_index = y * 320 + screen_x;    
                        if (depth_index >= 0 && depth_index < 320 * 240) {    
                            depth_buffer[depth_index] = distance;    
                        }    
                    }    
                }    
                    
                lowest_y = screen_y;    
            }    
            if (lowest_y <= 0)    
                break;    
        }    
    }    
        
        
// Array temporal para todos los billboards visibles    
BILLBOARD_RENDER_DATA visible_billboards[MAX_STATIC_BILLBOARDS + MAX_DYNAMIC_BILLBOARDS];    
int visible_count = 0;    
    
// PASO 4A: Recopilar billboards estáticos visibles    
collect_visible_billboards_from_array(static_billboards, static_billboard_count,   
                                     visible_billboards, &visible_count, terrain_fov); 
    
// PASO 4B: Recopilar billboards dinámicos visibles    
collect_visible_billboards_from_array(dynamic_billboards, MAX_DYNAMIC_BILLBOARDS,   
                                     visible_billboards, &visible_count, terrain_fov);
    
// PASO 4C: Ordenar por distancia (más lejanos primero)    
qsort(visible_billboards, visible_count, sizeof(BILLBOARD_RENDER_DATA), compare_billboards_by_distance);    
    
// PASO 4D: Renderizar en orden correcto con fade-out mejorado  
for (int i = 0; i < visible_count; i++) {    
    BILLBOARD_RENDER_DATA *render_data = &visible_billboards[i];    
    BILLBOARD_PROJECTION proj = render_data->projection;    
        
    // Aplicar la verificación de oclusión mejorada    
    int billboard_visible = 1;    
    int center_x = proj.screen_x;    
    int center_y = proj.screen_y;    
    int half_width = proj.scaled_width / 2;    
    int half_height = proj.scaled_height / 2;    
        
    float depth_tolerance = 2.0f;    
    int occlusion_samples = 0;    
    int total_samples = 0;    
    
    for (int check_y = center_y - half_height; check_y <= center_y + half_height; check_y += 8) {    
        for (int check_x = center_x - half_width; check_x <= center_x + half_width; check_x += 8) {    
            if (check_x >= 0 && check_x < 320 && check_y >= 0 && check_y < 240) {    
                int depth_index = check_y * 320 + check_x;    
                total_samples++;    
                if (proj.distance > depth_buffer[depth_index] + depth_tolerance) {    
                    occlusion_samples++;    
                }    
            }    
        }    
    }    
        
    if (total_samples > 0 && (float)occlusion_samples / total_samples > 0.75f) {    
        billboard_visible = 0;    
    }    
    
    if (!billboard_visible) continue;    
    
    // Renderizar billboard con alpha mejorado (incluye fade-out)  
    gr_blit(render_buffer, NULL,    
           proj.screen_x - proj.scaled_width/2,    
           proj.screen_y - proj.scaled_height/2,    
           0, 0, proj.scaled_width, proj.scaled_height,    
           render_data->graph->width/2, render_data->graph->height/2,    
           render_data->graph, NULL, 255, 255, 255, proj.alpha, 0, NULL);    
    
    // Actualizar depth buffer    
    int update_radius = 2;    
    for (int y = center_y - update_radius; y <= center_y + update_radius; y++) {    
        for (int x = center_x - update_radius; x <= center_x + update_radius; x++) {    
            if (x >= 0 && x < 320 && y >= 0 && y < 240) {    
                int idx = y * 320 + x;    
                if (proj.distance < depth_buffer[idx]) {    
                    depth_buffer[idx] = proj.distance;    
                }    
            }    
        }    
    }    
}
      
    return render_buffer->code;  
}
 
// ============================================================================  
// RENDERIZADO GPU CON SHADERS  
// ============================================================================  
  


// Vertex shader (igual a tu original, pasa UV)
static const char* voxel_vertex_shader_source =   
"#version 120\n"  
"attribute vec2 position;\n"      // CAMBIO CRÍTICO: usar 'position' no 'bgd_Vertex'  
"attribute vec2 texCoord;\n"      // CAMBIO CRÍTICO: agregar texCoord explícito  
"varying vec2 v_uv;\n"  
"void main() {\n"  
"    v_uv = texCoord;\n"          // CAMBIO CRÍTICO: pasar texCoord directamente  
"    gl_Position = vec4(position, 0.0, 1.0);\n"  
"}\n";

// fragment shader
static const char* voxel_fragment_shader_source =   
"#version 120\n"  
"varying vec2 v_uv;\n"  
"\n"  
"uniform sampler2D u_heightmap;\n"  
"uniform sampler2D u_texturemap;\n"  
"uniform vec3 u_camera_pos;\n"  
"uniform float u_camera_angle;\n"  
"uniform float u_camera_pitch;\n"  
"uniform float u_fov;\n"  
"uniform float u_max_distance;\n"  
"uniform float u_water_level;\n"  
"uniform float u_light_intensity;\n"  
"uniform vec2 u_heightmap_size;\n"  
"uniform vec3 u_sky_color;\n"  
"\n"  
"void main() {\n"  
"    float screen_x = v_uv.x * 320.0;\n"  
"    float angle_step = u_fov / 320.0;\n"  
"    float base_angle = u_camera_angle - u_fov * 0.5;\n"  
"    float column_angle = base_angle + screen_x * angle_step;\n"  
"    \n"  
"    float cos_angle = cos(column_angle);\n"  
"    float sin_angle = sin(column_angle);\n"  
"    float pitch_offset = u_camera_pitch * 40.0;\n"  
"    \n"  
"    float lowest_y = 1.0;\n"  
"    vec3 final_color = u_sky_color;\n"  
"    \n"  
"    for (float distance = 1.0; distance < u_max_distance; distance += 1.5) {\n"  
"        vec2 world_pos = u_camera_pos.xy + vec2(cos_angle, sin_angle) * distance;\n"  
"        \n"  
"        // CRÍTICO: Normalizar coordenadas correctamente\n"  
"        vec2 uv = mod(world_pos / u_heightmap_size, 1.0);\n"  
"        \n"  
"        float terrain_height = texture2D(u_heightmap, uv).r * 255.0;\n"  
"        \n"  
"        bool is_water = false;\n"  
"        float render_height;\n"  
"        \n"  
"        if (terrain_height < u_water_level) {\n"  
"            render_height = u_water_level;\n"  
"            is_water = true;\n"  
"        } else {\n"  
"            render_height = terrain_height;\n"  
"        }\n"  
"        \n"  
"        float height_on_screen = (u_camera_pos.z - render_height) / distance * 300.0 + 120.0;\n"  
"        height_on_screen += pitch_offset;\n"  
"        \n"  
"        float screen_y = height_on_screen / 240.0;\n"  
"        \n"  
"        if (screen_y < 0.0) screen_y = 0.0;\n"  
"        if (screen_y >= 1.0) continue;\n"  
"        \n"  
"        if (screen_y < lowest_y) {\n"  
"            float fog = 1.0 - (distance / u_max_distance);\n"  
"            fog = max(fog, 0.3);\n"  
"            \n"  
"            if (is_water) {\n"  
"                float depth = (u_water_level - terrain_height) * 0.01;\n"  
"                vec3 water_color = mix(vec3(0.25, 0.5, 1.0), vec3(0.1, 0.2, 0.5), clamp(depth, 0.0, 1.0));\n"  
"                final_color = water_color * fog + u_sky_color * (1.0 - fog);\n"  
"            } else {\n"  
"                // CRÍTICO: Leer texturemap con las mismas coordenadas UV\n"  
"                vec3 terrain_color = texture2D(u_texturemap, uv).rgb;\n"  
"                terrain_color *= u_light_intensity;\n"  
"                terrain_color = terrain_color * fog + u_sky_color * (1.0 - fog);\n"  
"                final_color = terrain_color;\n"  
"            }\n"  
"            \n"  
"            lowest_y = screen_y;\n"  
"        }\n"  
"        \n"  
"        if (lowest_y <= 0.0) break;\n"  
"    }\n"  
"    \n"  
"    gl_FragColor = vec4(final_color, 1.0);\n"  
"}\n";

// Compilar un shader  
static GLuint compile_gl_shader(GLenum type, const char* source) {  
    GLuint shader = glCreateShader(type);  
    glShaderSource(shader, 1, &source, NULL);  
    glCompileShader(shader);  
      
    GLint success;  
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);  
    if (!success) {  
        char info_log[512];  
        glGetShaderInfoLog(shader, 512, NULL, info_log);  
        fprintf(stderr, "ERROR compilando shader OpenGL:\n%s\n", info_log);  
        glDeleteShader(shader);  
        return 0;  
    }  
      
    fprintf(stderr, "Shader OpenGL compilado exitosamente\n");  
    return shader;  
}
  
// Crear programa de shaders  
static int create_gl_shader_program() {  
       if (gl_shader_program) return 1;  
      
    gl_vertex_shader = compile_gl_shader(GL_VERTEX_SHADER, voxel_vertex_shader_source);  
    if (!gl_vertex_shader) {  
        fprintf(stderr, "ERROR: No se pudo compilar vertex shader\n");  
        return 0;  
    }  
      
    gl_fragment_shader = compile_gl_shader(GL_FRAGMENT_SHADER, voxel_fragment_shader_source);  
    if (!gl_fragment_shader) {  
        fprintf(stderr, "ERROR: No se pudo compilar fragment shader\n");  
        glDeleteShader(gl_vertex_shader);  
        return 0;  
    }  
      
    gl_shader_program = glCreateProgram();  
    glAttachShader(gl_shader_program, gl_vertex_shader);  
    glAttachShader(gl_shader_program, gl_fragment_shader);  
      
    // CRÍTICO: Vincular atributos ANTES de linkear  
    glBindAttribLocation(gl_shader_program, 0, "position");  
    glBindAttribLocation(gl_shader_program, 1, "texCoord");  
    glLinkProgram(gl_shader_program);  
      
    GLint success;  
    glGetProgramiv(gl_shader_program, GL_LINK_STATUS, &success);  
    if (!success) {  
        char info_log[512];  
        glGetProgramInfoLog(gl_shader_program, 512, NULL, info_log);  
        fprintf(stderr, "ERROR linkeando programa shader:\n%s\n", info_log);  
        glDeleteShader(gl_vertex_shader);  
        glDeleteShader(gl_fragment_shader);  
        glDeleteProgram(gl_shader_program);  
        return 0;  
    }  
      
    fprintf(stderr, "Programa shader OpenGL creado exitosamente\n");  
    return 1;  
}


static int setup_shader_parameters(HEIGHTMAP *hm) {  
    if (!voxel_shader) return 0;  
      
    if (!voxel_params) {  
        voxel_params = shader_create_parameters(11);  
        if (!voxel_params) {  
            return 0;  
        }  
    }  
      
    // Obtener ubicaciones de uniforms  
    int loc_heightmap = shader_getuniformlocation(voxel_shader, "u_heightmap");  
    int loc_texturemap = shader_getuniformlocation(voxel_shader, "u_texturemap");  
    int loc_camera_pos = shader_getuniformlocation(voxel_shader, "u_camera_pos");  
    int loc_camera_angle = shader_getuniformlocation(voxel_shader, "u_camera_angle");  
    int loc_camera_pitch = shader_getuniformlocation(voxel_shader, "u_camera_pitch");  
    int loc_fov = shader_getuniformlocation(voxel_shader, "u_fov");  
    int loc_max_distance = shader_getuniformlocation(voxel_shader, "u_max_distance");  
    int loc_water_level = shader_getuniformlocation(voxel_shader, "u_water_level");  
    int loc_light_intensity = shader_getuniformlocation(voxel_shader, "u_light_intensity");  
    int loc_heightmap_size = shader_getuniformlocation(voxel_shader, "u_heightmap_size");  
    int loc_sky_color = shader_getuniformlocation(voxel_shader, "u_sky_color");  
      
    // Clamping de cámara  
    if (camera.x < 0.0f) camera.x = 0.0f;  
    if (camera.y < 0.0f) camera.y = 0.0f;  
    if (camera.z < 0.0f) camera.z = 0.0f;  
    if (camera.x >= hm->width) camera.x = hm->width - 1.0f;  
    if (camera.y >= hm->height) camera.y = hm->height - 1.0f;  
      
    // Conversión de miliradianes a radianes  
    float angle_radians = camera.angle / 1000.0f;  
    float pitch_radians = camera.pitch / 1000.0f;  
      
    // Calcular max_distance seguro  
    float dist_to_right = hm->width - camera.x;  
    float dist_to_left = camera.x;  
    float dist_to_bottom = hm->height - camera.y;  
    float dist_to_top = camera.y;  
      
    float min_dist_to_edge = dist_to_right;  
    if (dist_to_left < min_dist_to_edge) min_dist_to_edge = dist_to_left;  
    if (dist_to_bottom < min_dist_to_edge) min_dist_to_edge = dist_to_bottom;  
    if (dist_to_top < min_dist_to_edge) min_dist_to_edge = dist_to_top;  
      
    float safe_max_distance = min_dist_to_edge * 0.9f;  
    if (safe_max_distance > max_render_distance) safe_max_distance = max_render_distance;  
    if (safe_max_distance < 100.0f) safe_max_distance = 100.0f;  
      
    // CORRECCIÓN CRÍTICA: Pasar GRAPH* directamente, no texture IDs  
    if (loc_heightmap >= 0) {  
        shader_set_param(voxel_params, SHADER_IMAGE, loc_heightmap, 0,   
                        hm->heightmap, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_texturemap >= 0 && hm->texturemap) {  
        shader_set_param(voxel_params, SHADER_IMAGE, loc_texturemap, 1,   
                        hm->texturemap, 0, 0, 0, 0, 0);  
    }  
      
    // Configurar vec3 u_camera_pos  
    if (loc_camera_pos >= 0) {  
        float camera_pos[3] = { camera.x, camera.y, camera.z };  
        shader_set_param(voxel_params, UNIFORM_FLOAT3_ARRAY, loc_camera_pos, 1,   
                        camera_pos, 0, 0, 0, 0, 0);  
    }  
      
    // Configurar floats individuales  
    if (loc_camera_angle >= 0) {  
        float angle_val = angle_radians;  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_camera_angle, 0,   
                        (void*)(intptr_t)*(int32_t*)&angle_val, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_camera_pitch >= 0) {  
        float pitch_val = pitch_radians;  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_camera_pitch, 0,   
                        (void*)(intptr_t)*(int32_t*)&pitch_val, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_fov >= 0) {  
        float fov_val = 0.7f;  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_fov, 0,   
                        (void*)(intptr_t)*(int32_t*)&fov_val, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_max_distance >= 0) {  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_max_distance, 0,   
                        (void*)(intptr_t)*(int32_t*)&safe_max_distance, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_water_level >= 0) {  
        float water_val = water_level;  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_water_level, 0,   
                        (void*)(intptr_t)*(int32_t*)&water_val, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_light_intensity >= 0) {  
        float light_val = light_intensity / 255.0f;  
        shader_set_param(voxel_params, UNIFORM_FLOAT, loc_light_intensity, 0,   
                        (void*)(intptr_t)*(int32_t*)&light_val, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_heightmap_size >= 0) {  
        float heightmap_size[2] = { (float)hm->width, (float)hm->height };  
        shader_set_param(voxel_params, UNIFORM_FLOAT2_ARRAY, loc_heightmap_size, 1,   
                        heightmap_size, 0, 0, 0, 0, 0);  
    }  
      
    if (loc_sky_color >= 0) {  
        float sky_color[3] = {   
            sky_color_r / 255.0f,   
            sky_color_g / 255.0f,   
            sky_color_b / 255.0f   
        };  
        shader_set_param(voxel_params, UNIFORM_FLOAT3_ARRAY, loc_sky_color, 1,   
                        sky_color, 0, 0, 0, 0, 0);  
    }  
      
    return 1;  
}



// Función principal de renderizado con shader  
static int64_t render_voxelspace_with_shader(HEIGHTMAP *hm, int width, int height) {  
    if (!voxel_shader) {  
        if (!create_voxelspace_shader()) {  
            return 0;  
        }  
    }  
      
    if (!setup_shader_parameters(hm)) {  
        return 0;  
    }  
      
    // Crear render_buffer si no existe o si el tamaño cambió  
    if (!render_buffer || render_buffer->width != width || render_buffer->height != height) {  
        if (render_buffer) {  
            bitmap_destroy(render_buffer);  
        }  
        render_buffer = bitmap_new_syslib(width, height);  
        if (!render_buffer) {  
            return 0;  
        }  
    }  
      
    // Crear un GRAPH dummy de 1x1 píxel como source  
    // El shader ignorará este contenido y generará su propia salida  
    static GRAPH *dummy_source = NULL;  
    if (!dummy_source) {  
        dummy_source = bitmap_new_syslib(1, 1);  
        if (!dummy_source) {  
            return 0;  
        }  
    }  
      
    // Activar shader y aplicar parámetros  
    shader_activate(voxel_shader);  
    shader_apply_parameters(voxel_params);  
      
    // Renderizar usando gr_blit con el shader activo  
    // El shader se ejecutará durante el blit y generará el terreno voxel  
gr_blit(  
    render_buffer,  
    NULL,  
    width / 2.0,             // ✓ Usar 'width' del parámetro  
    height / 2.0,            // ✓ Usar 'height' del parámetro  
    0,  
    0,  
    width * 100.0,           // ✓ Usar 'width' del parámetro  
    height * 100.0,          // ✓ Usar 'height' del parámetro  
    0.5,  
    0.5,  
    dummy_source,  
    NULL,  
    255,  
    255, 255, 255,  
    BLEND_DISABLED,  
    NULL  
);
      
    shader_deactivate();  
    fprintf(stderr, "DEBUG: Shader desactivado, verificando render_buffer...\n");  
  
// Verificar que render_buffer tiene contenido  
uint32_t test_pixel = gr_get_pixel(render_buffer, 160, 120);  
fprintf(stderr, "DEBUG: Pixel central del render_buffer: 0x%08X\n", test_pixel);  
    return render_buffer->code;  
}
  
// Función principal exportada a BennuGD2  
int64_t libmod_heightmap_render_voxelspace_gpu(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    int64_t render_width = params[1];  
    int64_t render_height = params[2];  
      
    // Inicializar GLEW si es necesario  
    if (!initialize_glew_if_needed()) {  
        fprintf(stderr, "ERROR: No se pudo inicializar GLEW\n");  
        return 0;  
    }  
      
    // Buscar heightmap  
    HEIGHTMAP *hm = NULL;  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == hm_id) {  
            hm = &heightmaps[i];  
            break;  
        }  
    }  
      
    if (!hm || !hm->cache_valid) {  
        fprintf(stderr, "ERROR: Heightmap no encontrado o cache inválido\n");  
        return 0;  
    }  
      
    // Crear shader program si no existe  
    if (!create_gl_shader_program()) {  
        fprintf(stderr, "ERROR: No se pudo crear shader program\n");  
        return 0;  
    }  
      
    // Subir texturas a OpenGL  
    if (!upload_textures_to_opengl(hm)) {  
        fprintf(stderr, "ERROR: No se pudieron subir texturas\n");  
        return 0;  
    }  
      
    // Crear FBO si no existe o cambió el tamaño  
    static int64_t last_width = 0, last_height = 0;  
    if (!voxel_fbo || !voxel_render_texture || last_width != render_width || last_height != render_height) {  
        if (voxel_fbo) glDeleteFramebuffers(1, &voxel_fbo);  
        if (voxel_render_texture) glDeleteTextures(1, &voxel_render_texture);  
          
        glGenFramebuffers(1, &voxel_fbo);  
        glGenTextures(1, &voxel_render_texture);  
          
        glBindTexture(GL_TEXTURE_2D, voxel_render_texture);  
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, render_width, render_height, 0,  
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);  
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
          
        glBindFramebuffer(GL_FRAMEBUFFER, voxel_fbo);  
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,  
                              GL_TEXTURE_2D, voxel_render_texture, 0);  
          
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);  
        if (status != GL_FRAMEBUFFER_COMPLETE) {  
            fprintf(stderr, "ERROR: Framebuffer incompleto: 0x%x\n", status);  
            return 0;  
        }  
          
        glBindFramebuffer(GL_FRAMEBUFFER, 0);  
        last_width = render_width;  
        last_height = render_height;  
        fprintf(stderr, "FBO creado: %ldx%ld\n", render_width, render_height);  
    }  
      
    // Crear render_buffer si no existe  
    if (!render_buffer || render_buffer->width != render_width || render_buffer->height != render_height) {  
        if (render_buffer) {  
            bitmap_destroy(render_buffer);  
        }  
        render_buffer = bitmap_new_syslib(render_width, render_height);  
        if (!render_buffer) {  
            fprintf(stderr, "ERROR: No se pudo crear render_buffer\n");  
            return 0;  
        }  
    }  
      
    // Crear VAO/VBO para quad fullscreen si no existe  
    if (!gl_vao) {  
        float quad_vertices[] = {  
            // pos      // texCoords  
            -1.0f,  1.0f,  0.0f, 1.0f,  
            -1.0f, -1.0f,  0.0f, 0.0f,  
             1.0f, -1.0f,  1.0f, 0.0f,  
            -1.0f,  1.0f,  0.0f, 1.0f,  
             1.0f, -1.0f,  1.0f, 0.0f,  
             1.0f,  1.0f,  1.0f, 1.0f  
        };  
          
        glGenVertexArrays(1, &gl_vao);  
        glGenBuffers(1, &gl_vbo);  
          
        glBindVertexArray(gl_vao);  
        glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);  
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);  
          
        // Atributo de posición (location = 0)  
        glEnableVertexAttribArray(0);  
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);  
          
        // Atributo de texCoords (location = 1)  
        glEnableVertexAttribArray(1);  
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));  
          
        glBindVertexArray(0);  
        fprintf(stderr, "VAO/VBO creado para quad fullscreen\n");  
    }  
      
    // Renderizar a FBO usando OpenGL directo  
    glBindFramebuffer(GL_FRAMEBUFFER, voxel_fbo);  
    glViewport(0, 0, render_width, render_height);  
    glClearColor(sky_color_r / 255.0f, sky_color_g / 255.0f, sky_color_b / 255.0f, 1.0f);  
    glClear(GL_COLOR_BUFFER_BIT);  
      
    // Usar shader program  
    glUseProgram(gl_shader_program);  
      
    // Configurar uniforms  
    glUniform1i(glGetUniformLocation(gl_shader_program, "u_heightmap"), 0);  
    glUniform1i(glGetUniformLocation(gl_shader_program, "u_texturemap"), 1);  
    glUniform3f(glGetUniformLocation(gl_shader_program, "u_camera_pos"), camera.x, camera.y, camera.z);  
    glUniform1f(glGetUniformLocation(gl_shader_program, "u_camera_angle"), camera.angle / 1000.0f);  
    glUniform1f(glGetUniformLocation(gl_shader_program, "u_camera_pitch"), camera.pitch / 1000.0f);  
    glUniform1f(glGetUniformLocation(gl_shader_program, "u_fov"), 0.7f);  
    glUniform1f(glGetUniformLocation(gl_shader_program, "u_max_distance"), max_render_distance);  
    glUniform1f(glGetUniformLocation(gl_shader_program, "u_water_level"), water_level);  
    glUniform1f(glGetUniformLocation(gl_shader_program, "u_light_intensity"), light_intensity / 255.0f);  
    glUniform2f(glGetUniformLocation(gl_shader_program, "u_heightmap_size"), (float)hm->width, (float)hm->height);  
    glUniform3f(glGetUniformLocation(gl_shader_program, "u_sky_color"),   
                sky_color_r / 255.0f, sky_color_g / 255.0f, sky_color_b / 255.0f);  
      
    // Bind texturas  
    glActiveTexture(GL_TEXTURE0);  
    glBindTexture(GL_TEXTURE_2D, gl_heightmap_tex);  
    glActiveTexture(GL_TEXTURE1);  
    glBindTexture(GL_TEXTURE_2D, gl_texturemap_tex);  
      
    // Renderizar quad fullscreen  
    glBindVertexArray(gl_vao);  
    glDrawArrays(GL_TRIANGLES, 0, 6);  
    glBindVertexArray(0);  
      
    // Desactivar shader  
    glUseProgram(0);  
      
    // Copiar resultado del FBO a render_buffer píxel por píxel  
    uint8_t *pixel_data = malloc(render_width * render_height * 4);  
    if (!pixel_data) {  
        fprintf(stderr, "ERROR: No se pudo asignar memoria para pixel_data\n");  
        return 0;  
    }  
      
    glBindFramebuffer(GL_FRAMEBUFFER, voxel_fbo);  
    glReadPixels(0, 0, render_width, render_height, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);  
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  
      
    // Copiar píxeles a render_buffer usando gr_put_pixel  
    for (int y = 0; y < render_height; y++) {  
        for (int x = 0; x < render_width; x++) {  
            int idx = (y * render_width + x) * 4;  
            uint8_t r = pixel_data[idx + 0];  
            uint8_t g = pixel_data[idx + 1];  
            uint8_t b = pixel_data[idx + 2];  
            uint8_t a = pixel_data[idx + 3];  
              
            uint32_t color = SDL_MapRGBA(gPixelFormat, r, g, b, a);  
            gr_put_pixel(render_buffer, x, y, color);  
        }  
    }  
      
    free(pixel_data);  
      
    fprintf(stderr, "DEBUG: Renderizado GPU completado\n");  
      
    return render_buffer->code;  
}

static int upload_textures_to_opengl(HEIGHTMAP *hm) {  
    // Solo subir si cambió el heightmap  
    if (cached_heightmap_id == hm->id && gl_heightmap_tex != 0 && gl_texturemap_tex != 0) {  
        return 1;  // Ya están subidas  
    }  
      
    // Subir heightmap  
    if (gl_heightmap_tex == 0) {  
        glGenTextures(1, &gl_heightmap_tex);  
    }  
      
    uint8_t *heightmap_buffer = malloc(hm->width * hm->height * 4);  
    if (!heightmap_buffer) {  
        fprintf(stderr, "ERROR: No se pudo asignar memoria para heightmap_buffer\n");  
        return 0;  
    }  
      
    for (int y = 0; y < hm->height; y++) {  
        for (int x = 0; x < hm->width; x++) {  
            uint32_t pixel = gr_get_pixel(hm->heightmap, x, y);  
            uint8_t height = (pixel >> 16) & 0xFF;  
              
            int idx = (y * hm->width + x) * 4;  
            heightmap_buffer[idx + 0] = height;  
            heightmap_buffer[idx + 1] = height;  
            heightmap_buffer[idx + 2] = height;  
            heightmap_buffer[idx + 3] = 255;  
        }  
    }  
      
    glBindTexture(GL_TEXTURE_2D, gl_heightmap_tex);  
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, hm->width, hm->height, 0,  
                 GL_RGBA, GL_UNSIGNED_BYTE, heightmap_buffer);  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);  
      
    free(heightmap_buffer);  
    fprintf(stderr, "Heightmap subido a GPU: %ldx%ld (texture ID: %u)\n",   
            hm->width, hm->height, gl_heightmap_tex);  
      
    // Subir texturemap  
    if (hm->texturemap) {  
        if (gl_texturemap_tex == 0) {  
            glGenTextures(1, &gl_texturemap_tex);  
        }  
          
        uint32_t *texture_buffer = malloc(hm->texturemap->width * hm->texturemap->height * sizeof(uint32_t));  
        if (!texture_buffer) {  
            fprintf(stderr, "ERROR: No se pudo asignar memoria para texture_buffer\n");  
            return 0;  
        }  
          
        for (int y = 0; y < hm->texturemap->height; y++) {  
            for (int x = 0; x < hm->texturemap->width; x++) {  
                texture_buffer[y * hm->texturemap->width + x] = gr_get_pixel(hm->texturemap, x, y);  
            }  
        }  
          
        glBindTexture(GL_TEXTURE_2D, gl_texturemap_tex);  
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, hm->texturemap->width,  
                     hm->texturemap->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_buffer);  
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);  
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);  
          
        free(texture_buffer);  
        fprintf(stderr, "Texturemap subido a GPU: %lux%lu (texture ID: %u)\n",   
                hm->texturemap->width, hm->texturemap->height, gl_texturemap_tex);  
    } else {  
        fprintf(stderr, "ADVERTENCIA: hm->texturemap es NULL - no se subió textura de color\n");  
    }  
      
    cached_heightmap_id = hm->id;  
    return 1;  
}

/* Funciones auxiliares */
void build_height_cache(HEIGHTMAP *hm)

{

    if (!hm->heightmap)

        return;


   if (hm->height_cache)  
{  
    free(hm->height_cache);  
    hm->height_cache = NULL;  
    hm->cache_valid = 0;  
}  
  
hm->height_cache = malloc(hm->width * hm->height * sizeof(float));  
if (!hm->height_cache)  
{  
    hm->cache_valid = 0;  
    fprintf(stderr, "Error: No se pudo asignar height_cache para heightmap %ldx%ld\n",   
            (int)hm->width, (int)hm->height);  
    return;  
}


    for (int y = 0; y < hm->height; y++)

    {

        for (int x = 0; x < hm->width; x++)

        {

            uint32_t pixel = gr_get_pixel(hm->heightmap, x, y);

            float height = (float)((pixel >> 16) & 0xFF);

            hm->height_cache[y * hm->width + x] = height;

        }

    }


    hm->cache_valid = 1;

}


/* Devuelve un color RGB interpolado usando funciones SDL */
uint32_t get_texture_color_bilinear(GRAPH *texture, float x, float y) {  
    if (!texture)  
        return 0;  
  
    // Implementar wrapping de coordenadas para tiling  
    while (x < 0) x += texture->width;  
    while (y < 0) y += texture->height;  
    x = fmod(x, texture->width);  
    y = fmod(y, texture->height);  
  
    int ix = (int)x;  
    int iy = (int)y;  
      
    // Asegurar que estamos dentro de los límites para interpolación  
    if (ix >= texture->width - 1) ix = texture->width - 2;  
    if (iy >= texture->height - 1) iy = texture->height - 2;  
    if (ix < 0) ix = 0;  
    if (iy < 0) iy = 0;  
  
    float fx = x - ix;  
    float fy = y - iy;  
  
    // Obtener los 4 píxeles para interpolación  
    uint32_t c00 = gr_get_pixel(texture, ix, iy);  
    uint32_t c10 = gr_get_pixel(texture, (ix + 1) % texture->width, iy);  
    uint32_t c01 = gr_get_pixel(texture, ix, (iy + 1) % texture->height);  
    uint32_t c11 = gr_get_pixel(texture, (ix + 1) % texture->width, (iy + 1) % texture->height);  
  
    // Verificar si algún pixel es inválido  
    if (c00 == 0 && c10 == 0 && c01 == 0 && c11 == 0) {  
        return 0; // Fallback al sistema original  
    }  
  
    // Extraer componentes RGB y Alpha si está disponible  
    Uint8 r00, g00, b00, a00 = 255;  
    Uint8 r10, g10, b10, a10 = 255;  
    Uint8 r01, g01, b01, a01 = 255;  
    Uint8 r11, g11, b11, a11 = 255;  
  
  // Al inicio de la función, después de las validaciones  
static int cached_bytes_per_pixel = 0;  
if (cached_bytes_per_pixel == 0) {  
    cached_bytes_per_pixel = gPixelFormat->BytesPerPixel;  
}  
  
// Extraer componentes usando los shifts correctos del formato  
if (cached_bytes_per_pixel == 4) {  
    // RGBA de 32 bits  
    r00 = (c00 >> gPixelFormat->Rshift) & 0xFF;   
    g00 = (c00 >> gPixelFormat->Gshift) & 0xFF;   
    b00 = (c00 >> gPixelFormat->Bshift) & 0xFF;   
    a00 = (c00 >> gPixelFormat->Ashift) & 0xFF;  
      
    r10 = (c10 >> gPixelFormat->Rshift) & 0xFF;   
    g10 = (c10 >> gPixelFormat->Gshift) & 0xFF;   
    b10 = (c10 >> gPixelFormat->Bshift) & 0xFF;   
    a10 = (c10 >> gPixelFormat->Ashift) & 0xFF;  
      
    r01 = (c01 >> gPixelFormat->Rshift) & 0xFF;   
    g01 = (c01 >> gPixelFormat->Gshift) & 0xFF;   
    b01 = (c01 >> gPixelFormat->Bshift) & 0xFF;   
    a01 = (c01 >> gPixelFormat->Ashift) & 0xFF;  
      
    r11 = (c11 >> gPixelFormat->Rshift) & 0xFF;   
    g11 = (c11 >> gPixelFormat->Gshift) & 0xFF;   
    b11 = (c11 >> gPixelFormat->Bshift) & 0xFF;   
    a11 = (c11 >> gPixelFormat->Ashift) & 0xFF;  
} else {  
    // RGB de 24 bits  
    r00 = (c00 >> gPixelFormat->Rshift) & 0xFF;   
    g00 = (c00 >> gPixelFormat->Gshift) & 0xFF;   
    b00 = (c00 >> gPixelFormat->Bshift) & 0xFF;  
      
    r10 = (c10 >> gPixelFormat->Rshift) & 0xFF;   
    g10 = (c10 >> gPixelFormat->Gshift) & 0xFF;   
    b10 = (c10 >> gPixelFormat->Bshift) & 0xFF;  
      
    r01 = (c01 >> gPixelFormat->Rshift) & 0xFF;   
    g01 = (c01 >> gPixelFormat->Gshift) & 0xFF;   
    b01 = (c01 >> gPixelFormat->Bshift) & 0xFF;  
      
    r11 = (c11 >> gPixelFormat->Rshift) & 0xFF;   
    g11 = (c11 >> gPixelFormat->Gshift) & 0xFF;   
    b11 = (c11 >> gPixelFormat->Bshift) & 0xFF;  
}
  
    // Interpolación bilineal mejorada  
    float r0 = r00 + fx * (r10 - r00);  
    float r1 = r01 + fx * (r11 - r01);  
    float rf = r0 + fy * (r1 - r0);  
  
    float g0 = g00 + fx * (g10 - g00);  
    float g1 = g01 + fx * (g11 - g01);  
    float gf = g0 + fy * (g1 - g0);  
  
    float b0 = b00 + fx * (b10 - b00);  
    float b1 = b01 + fx * (b11 - b01);  
    float bf = b0 + fy * (b1 - b0);  
  
    float a0 = a00 + fx * (a10 - a00);  
    float a1 = a01 + fx * (a11 - a01);  
    float af = a0 + fy * (a1 - a0);  
  
    // Clamp values  
    rf = fmaxf(0, fminf(255, rf));  
    gf = fmaxf(0, fminf(255, gf));  
    bf = fmaxf(0, fminf(255, bf));  
    af = fmaxf(0, fminf(255, af));  
  
    // Retornar el color apropiado según el formato  
    if (gPixelFormat->BytesPerPixel == 4) {  
        return SDL_MapRGBA(gPixelFormat, (Uint8)rf, (Uint8)gf, (Uint8)bf, (Uint8)af);  
    } else {  
        return SDL_MapRGB(gPixelFormat, (Uint8)rf, (Uint8)gf, (Uint8)bf);  
    }  
}

/* Función para mantener cámara sobre el terreno */
void clamp_camera_to_terrain(HEIGHTMAP *hm)
{
    if (!hm || !hm->cache_valid)
        return;

    // Obtener altura del terreno en posición de cámara
    float terrain_height = get_height_at(hm, camera.x, camera.y);

    // Altura mínima sobre el terreno (por ejemplo, 5 unidades)
    float min_height_above_terrain = 5.0f;

    // Ajustar Z de cámara si está muy cerca del terreno
    if (camera.z < terrain_height + min_height_above_terrain)
    {
        camera.z = terrain_height + min_height_above_terrain;
    }
}

/* Función para mantener cámara dentro de límites */
void clamp_camera_to_bounds(HEIGHTMAP *hm)
{
    if (!hm)
        return;

    // Margen desde los bordes
    float margin = 2.0f;

    // Limitar X
    if (camera.x < margin)
        camera.x = margin;
    if (camera.x >= hm->width - margin)
        camera.x = hm->width - margin - 0.1f;

    // Limitar Y
    if (camera.y < margin)
        camera.y = margin;
    if (camera.y >= hm->height - margin)
        camera.y = hm->height - margin - 0.1f;
}

/* Inicializar cámara en posición válida del terreno */
int64_t libmod_heightmap_init_camera_on_terrain(INSTANCE *my, int64_t *params)

{

    int64_t hm_id = params[0];


    HEIGHTMAP *hm = NULL;

    for (int i = 0; i < MAX_HEIGHTMAPS; i++)

    {

        if (heightmaps[i].id == hm_id)

        {

            hm = &heightmaps[i];

            break;

        }

    }


    if (!hm || !hm->cache_valid)

        return 0;


    // Colocar en el centro del mapa con margen de seguridad

    float margin = 10.0f;

    float x = hm->width / 2.0f;

    float y = hm->height / 2.0f;


    if (x < margin)

        x = margin;

    if (x >= hm->width - margin)

        x = hm->width - margin - 1;

    if (y < margin)

        y = margin;

    if (y >= hm->height - margin)

        y = hm->height - margin - 1;


    camera.x = x;

    camera.y = y;


    // Obtener altura del terreno y situar cámara justo encima

    float terrain_height = get_height_at(hm, camera.x, camera.y);

    camera.z = terrain_height + 10.0f;


    // Ángulos iniciales

    camera.angle = 0.0f;

    camera.pitch = 0.0f;

    camera.fov = DEFAULT_FOV;


    return 1;

}

/* Configurar sensibilidad de controles */
int64_t libmod_heightmap_set_control_sensitivity(INSTANCE *my, int64_t *params)
{
    mouse_sensitivity = (float)params[0];
    move_speed = (float)params[1];
    height_speed = (float)params[2];
    return 1;
}


/* Control de rotación horizontal */
int64_t libmod_heightmap_look_horizontal(INSTANCE *my, int64_t *params)
{
    float delta = (float)params[0];
    camera.angle += delta * mouse_sensitivity / 1000.0f;
    return 1;
}

/* Control de rotación vertical */
int64_t libmod_heightmap_look_vertical(INSTANCE *my, int64_t *params)
{
    float delta = (float)params[0];
    camera.pitch -= delta * mouse_sensitivity / 1000.0f;

    // Limitar pitch
    const float max_pitch = M_PI_2 * 0.99f;
    if (camera.pitch > max_pitch)
        camera.pitch = max_pitch;
    if (camera.pitch < -max_pitch)
        camera.pitch = -max_pitch;

    return 1;
}

/* Ajustar altura */
int64_t libmod_heightmap_adjust_height(INSTANCE *my, int64_t *params)
{
    float delta = (float)params[0];
    camera.z += delta * height_speed / 2.0f;
    if (camera.z < 20)
        camera.z = 20;
    return 1;
}

/* Crear heightmap procedural */
int64_t libmod_heightmap_create_procedural(INSTANCE *my, int64_t *params)
{
    int64_t width = params[0];
    int64_t height = params[1];

    GRAPH *graph = bitmap_new_syslib(width, height);
    if (!graph)
        return 0;

    // Generar directamente en el bitmap sin llamadas externas
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int color = 128 +
                        sin(x * 360 / 20) * 60 +
                        cos(y * 360 / 15) * 70 +
                        sin((x + y) * 360 / 12) * 40 +
                        sin(x * 360 / 6) * 25 +
                        cos(y * 360 / 8) * 30;

            if (rand() % 100 < 5)
            {
                color += rand() % 100 + 50;
            }

            if (color < 0)
                color = 0;
            if (color > 255)
                color = 255;

            gr_put_pixel(graph, x, y, (color << 16) | (color << 8) | color);
        }
    }

    // Crear entrada en el array de heightmaps
    int slot = -1;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (!heightmaps[i].heightmap)
        {
            slot = i;
            break;
        }
    }

    if (slot == -1) {  
        fprintf(stderr, "Error: MAX_HEIGHTMAPS (%d) alcanzado\n", MAX_HEIGHTMAPS);  
        return 0;  
    }  
      
    // Verificar que next_heightmap_id no desborde  
    if (next_heightmap_id >= INT64_MAX - 1) {  
        fprintf(stderr, "Error: next_heightmap_id overflow\n");  
        return 0;  
    }

    heightmaps[slot].id = next_heightmap_id++;
    heightmaps[slot].heightmap = graph;
    heightmaps[slot].width = width;
    heightmaps[slot].height = height;
    build_height_cache(&heightmaps[slot]);

    return heightmaps[slot].id;
}

/* Obtener posición actual de la cámara del módulo */
int64_t libmod_heightmap_get_camera_position(INSTANCE *my, int64_t *params)
{
    // Los parámetros son punteros a las variables del PRG
    int64_t *camera_x_ptr = (int64_t *)params[0];
    int64_t *camera_y_ptr = (int64_t *)params[1];
    int64_t *camera_z_ptr = (int64_t *)params[2];
    int64_t *camera_angle_ptr = (int64_t *)params[3];
    int64_t *camera_pitch_ptr = (int64_t *)params[4];

    // Actualizar las variables del PRG con los valores del módulo
    *camera_x_ptr = (int64_t)camera.x;
    *camera_y_ptr = (int64_t)camera.y;
    *camera_z_ptr = (int64_t)camera.z;
    *camera_angle_ptr = (int64_t)(camera.angle * 1000.0f);
    *camera_pitch_ptr = (int64_t)(camera.pitch * 1000.0f);

    return 1;
}

/* Verificar colisión con terreno y ajustar posición de cámara */
int64_t libmod_heightmap_check_terrain_collision(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];

    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);

    if (!hm || !hm->cache_valid)
        return 0;

    // Obtener altura del terreno en la posición actual de la cámara
    float terrain_height = get_height_at(hm, camera.x, camera.y);

    // Altura mínima sobre el terreno (altura del jugador)
    float min_height_above_terrain = 5.0f;

    // Si la cámara está por debajo del terreno + altura mínima, ajustar
    if (camera.z < terrain_height + min_height_above_terrain)
    {
        camera.z = terrain_height + min_height_above_terrain;
        return 1; // Colisión detectada y corregida
    }

    return 0; // Sin colisión
}


/* Obtener altura del terreno en coordenadas de sprite */
int64_t libmod_heightmap_get_terrain_height_at_sprite(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    int64_t sprite_x = params[1];
    int64_t sprite_y = params[2];

    HEIGHTMAP *hm = NULL;
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)
    {
        if (heightmaps[i].id == hm_id)
        {
            hm = &heightmaps[i];
            break;
        }
    }

    if (!hm || !hm->cache_valid)
        return 0;

    // Convertir coordenadas de sprite a coordenadas del heightmap
    float world_x = (float)sprite_x / WORLD_TO_SPRITE_SCALE;  
    float world_y = (float)sprite_y / WORLD_TO_SPRITE_SCALE;

    float terrain_height = get_height_at(hm, world_x, world_y);
    return (int64_t)(terrain_height * WORLD_TO_SPRITE_SCALE);// Devolver en coordenadas de sprite
}

/* Verificar si un sprite puede moverse a una posición sin atravesar terreno */
int64_t libmod_heightmap_can_sprite_move_to(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    int64_t new_x = params[1];
    int64_t new_y = params[2];
    int64_t sprite_height = params[3]; // Altura del sprite sobre el suelo

    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);

    if (!hm || !hm->cache_valid)
        return 1; // Permitir movimiento si no hay heightmap

    // Convertir a coordenadas del heightmap
    float world_x = (float)new_x / WORLD_TO_SPRITE_SCALE;  
    float world_y = (float)new_y / WORLD_TO_SPRITE_SCALE;

    // Verificar límites del mapa
    if (world_x < 0 || world_x >= hm->width - 1 ||
        world_y < 0 || world_y >= hm->height - 1)
        return 0; // No permitir movimiento fuera del mapa

    float terrain_height = get_height_at(hm, world_x, world_y);
    float required_height = terrain_height + (float)sprite_height / WORLD_TO_SPRITE_SCALE;

    // Verificar si hay suficiente espacio vertical
    // Puedes añadir lógica adicional aquí para verificar pendientes, etc.

    return 1; // Permitir movimiento
}

/* Ajustar posición Y de sprite según altura del terreno */  
/* Ajustar posición Z de sprite según altura del terreno */    
int64_t libmod_heightmap_adjust_sprite_to_terrain(INSTANCE *my, int64_t *params)    
{    
    int64_t hm_id = params[0];    
    int64_t sprite_x = params[1];    
    int64_t sprite_y = params[2];    
    int64_t *adjusted_z = (int64_t *)params[3]; // CAMBIO: ajustar Z, no Y  
    
    HEIGHTMAP *hm = NULL;    
    for (int i = 0; i < MAX_HEIGHTMAPS; i++)    
    {    
        if (heightmaps[i].id == hm_id)    
        {    
            hm = &heightmaps[i];    
            break;    
        }    
    }    
    
    if (!hm || !hm->cache_valid)    
        return 0;    
    
    // CORRECCIÓN: Usar coordenadas directas para heightmap 1024x1024  
    float world_x = (float)sprite_x / 10.0f;    
    float world_y = (float)sprite_y / 10.0f;    
    
    // CORRECCIÓN: Verificación de límites más permisiva  
    if (world_x < 0 || world_x >= hm->width - 1 ||    
        world_y < 0 || world_y >= hm->height - 1)    
    {  
        // Si está fuera de límites, usar altura por defecto  
        *adjusted_z = 0;  
        return 0;  
    }  
    
    float terrain_height = get_height_at(hm, world_x, world_y);    
    
    // CORRECCIÓN: Ajustar Z para altura sobre terreno en coordenadas de sprite  
    *adjusted_z = (int64_t)(terrain_height * 10.0f);    
    
    return 1;    
}

/* Convertir coordenadas del mundo a coordenadas de pantalla */
int64_t libmod_heightmap_world_to_screen(INSTANCE *my, int64_t *params)    
{    
    float world_x = (float)params[0] / 10.0f;    
    float world_y = (float)params[1] / 10.0f;    
    float world_z = (float)params[2] / 10.0f;    
    int64_t *screen_x = (int64_t *)params[3];    
    int64_t *screen_y = (int64_t *)params[4];    
        
    // Vector desde cámara al objeto  
    float dx = world_x - camera.x;    
    float dy = world_y - camera.y;    
    float dz = world_z - camera.z;    
        
    // Calcular distancia 3D real  
    float distance_3d = sqrtf(dx * dx + dy * dy + dz * dz);  
      
    // Transformación de vista  
    float cos_a = cosf(camera.angle);    
    float sin_a = sinf(camera.angle);    
        
    float forward = dx * cos_a + dy * sin_a;    
    float right = -dx * sin_a + dy * cos_a;    
        
    // Verificar que está delante de la cámara  
    if (forward > 1.0f && forward < max_render_distance) {    
        // Proyección perspectiva CORREGIDA con escalado real  
        float perspective_factor = 100.0f / distance_3d; // Usar distancia 3D real  
          
        float projected_x = 160.0f + (right * perspective_factor);    
        float projected_y = 120.0f - (dz * perspective_factor) + (camera.pitch * 30.0f);    
            
        if (projected_x >= -100.0f && projected_x <= 420.0f &&    
            projected_y >= -100.0f && projected_y <= 340.0f) {    
                
            *screen_x = (int64_t)projected_x;    
            *screen_y = (int64_t)projected_y;    
            return 1;    
        }    
    }    
        
    return 0;    
}

/* Obtener iluminación del terreno para aplicar al sprite */
int64_t libmod_heightmap_get_terrain_lighting(INSTANCE *my, int64_t *params)
{
    int64_t hm_id = params[0];
    int64_t sprite_x = params[1];
    int64_t sprite_y = params[2];

HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
  
if (!hm || !hm->cache_valid)  
    return 255;  
  
float world_x = (float)sprite_x / WORLD_TO_SPRITE_SCALE;  
float world_y = (float)sprite_y / WORLD_TO_SPRITE_SCALE;  
  
// Validar coordenadas  
if (world_x < 0 || world_x >= hm->width ||   
    world_y < 0 || world_y >= hm->height) {  
    return 255; // Fuera de límites, iluminación completa  
}  
  
// Calcular distancia a la cámara para fog  
float dx = world_x - camera.x;  
float dy = world_y - camera.y;  
float distance = sqrtf(dx * dx + dy * dy);  
  
float fog = 1.0f - (distance / FOG_MAX_DISTANCE);  
if (fog < FOG_MIN_VISIBILITY)  
    fog = FOG_MIN_VISIBILITY;

    float total_light = (light_intensity / 255.0f) * fog;

    return (int64_t)(total_light * 255.0f);
}

/* Asignar sprite para que la cámara lo siga */
int64_t libmod_heightmap_update_camera_follow(INSTANCE *my, int64_t *params)  

{  

    int64_t hm_id = params[0];  

    int64_t sprite_x = params[1];  

    int64_t sprite_y = params[2];  

    int64_t sprite_z = params[3];  

  

    if (camera_follow_sprite_id == 0)  

        return 0;  

  

    // NO dividir por 10.0f - usar coordenadas directas del sprite  

    float world_x = (float)sprite_x;  

    float world_y = (float)sprite_y;  

    float world_z = (float)sprite_z;  

  

    // Los offsets también deben estar en la misma escala  

  camera.x = world_x + camera_follow_offset_x;  // Sin multiplicar por 10  

camera.y = world_y + camera_follow_offset_y;  // Sin multiplicar por 10    

camera.z = world_z + camera_follow_offset_z;  // Sin multiplicar por 10

  

    // Calcular ángulo para que la cámara mire hacia la nave  

    float dx = world_x - camera.x;  

    float dy = world_y - camera.y;  

      

    if (dx != 0.0f || dy != 0.0f) {  

        camera.angle = atan2f(dy, dx);  

    }  

  

    // Pitch para tercera persona  

    camera.pitch = -0.3f;  

  

    return 1;  

}


/* Obtener ID del sprite que sigue la cámara */
int64_t libmod_heightmap_get_camera_follow(INSTANCE *my, int64_t *params)
{
    return camera_follow_sprite_id;
}


int64_t libmod_heightmap_update_water_time(INSTANCE *my, int64_t *params)
{
    static uint32_t last_ticks = 0;
    uint32_t current_ticks = SDL_GetTicks();

    if (last_ticks == 0)
        last_ticks = current_ticks;

    float delta_time = (current_ticks - last_ticks) / 1000.0f;
    water_time += delta_time;

    if (water_time > 100.0f)
        water_time = 0.0f;
    last_ticks = current_ticks;
    return 1;
}

int64_t libmod_heightmap_set_sky_color(INSTANCE *my, int64_t *params)
{
    sky_color_r = (Uint8)params[0];
    sky_color_g = (Uint8)params[1];
    sky_color_b = (Uint8)params[2];
    sky_color_a = (Uint8)params[3];
    return 1;
}

// Nueva función para configurar distancia de dibujado
int64_t libmod_heightmap_set_render_distance(INSTANCE *my, int64_t *params)  
{  
    float requested = (float)params[0];  
      
    if (requested < 0.0f) {  
        fprintf(stderr, "Error: render_distance no puede ser negativa\n");  
        return 0;  
    }  
      
    max_render_distance = requested;  
    if (max_render_distance < 100.0f)  
        max_render_distance = 100.0f;  
    if (max_render_distance > 2000.0f)  
        max_render_distance = 2000.0f;  
      
    return 1;  
}

// Nueva función para configurar chunks
int64_t libmod_heightmap_set_chunk_config(INSTANCE *my, int64_t *params)  
{  
    int requested_size = (int)params[0];  
    int requested_radius = (int)params[1];  
      
    if (requested_size <= 0 || requested_radius <= 0) {  
        fprintf(stderr, "Error: chunk_size y chunk_radius deben ser positivos\n");  
        return 0;  
    }  
      
    chunk_size = requested_size;  
    chunk_radius = requested_radius;  
  
    if (chunk_size < 32)  
        chunk_size = 32;  
    if (chunk_size > 256)  
        chunk_size = 256;  
    if (chunk_radius < 2)  
        chunk_radius = 2;  
    if (chunk_radius > 10)  
        chunk_radius = 10;  
  
    return 1;  
}

/* Marcar una instancia como billboard */  
int64_t libmod_heightmap_set_billboard(INSTANCE *my, int64_t *params)      
{      
    // Si no se pasa ID, usar la instancia actual    
    int64_t instance_id = (params[0] == 0) ? LOCQWORD(my, PROCESS_ID) : params[0];    
      
    // Usar la función nativa de BennuGD2 en lugar de iterar manualmente  
    INSTANCE *target = instance_get(instance_id);  
      
    if (target) {  
        LOCQWORD(target, CTYPE) = C_BILLBOARD;        
        return 1;    
    }   
    return 0;      
}
  
int64_t libmod_heightmap_unset_billboard(INSTANCE *my, int64_t *params)  
{  
    int64_t instance_id = params[0];  
      
    extern INSTANCE *first_instance;  
    INSTANCE *current = first_instance;  
      
    while (current) {  
        // CORRECCIÓN: Usar LOCQWORD para acceder al PROCESS_ID.  
        if (LOCQWORD(current, PROCESS_ID) == instance_id) {  
            // Si se encuentra la instancia, se restablece su CTYPE a 0.  
            LOCQWORD(current, CTYPE) = 0;  
            return 1;  
        }  
        current = current->next;  
    }  
      
    return 0;  
}  
  
int64_t libmod_heightmap_is_billboard(INSTANCE *my, int64_t *params)  
{  
    int64_t instance_id = params[0];  
      
    extern INSTANCE *first_instance;  
    INSTANCE *current = first_instance;  
      
    while (current) {  
        // CORRECCIÓN: Usar LOCQWORD para acceder al PROCESS_ID.  
        if (LOCQWORD(current, PROCESS_ID) == instance_id) {  
            // Se verifica si el CTYPE de la instancia es C_BILLBOARD.  
            return (LOCQWORD(current, CTYPE) == C_BILLBOARD) ? 1 : 0;  
        }  
        current = current->next;  
    }  
      
    return 0;  
}

int64_t libmod_heightmap_auto_adjust_billboard_to_terrain(INSTANCE *my, int64_t *params)  
{  
    int64_t hm_id = params[0];  
    int64_t instance_id = params[1];  
      
    HEIGHTMAP *hm = NULL;  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == hm_id) {  
            hm = &heightmaps[i];  
            break;  
        }  
    }  
      
    if (!hm || !hm->cache_valid)  
        return 0;  
      
    // Encontrar la instancia  
    extern INSTANCE *first_instance;  
    INSTANCE *current = first_instance;  
      
    while (current) {  
        if (LOCQWORD(current, PROCESS_ID) == instance_id) {  
            float world_x = LOCDOUBLE(current, COORDX) / 10.0f;  
            float world_y = LOCDOUBLE(current, COORDY) / 10.0f;  
              
            float terrain_height = get_height_at(hm, world_x, world_y);  
              
            // Ajustar Z para que esté sobre el terreno  
            LOCDOUBLE(current, COORDZ) = terrain_height * 10.0f + 50.0f;  
              
            return 1;  
        }  
        current = current->next;  
    }  
      
    return 0;  
}

// Devuelve screen_x, screen_y y z_cam para una posición 3D
int64_t libmod_heightmap_project_billboard(INSTANCE *my, int64_t *params)  
{  
    int heightmap_id = params[0];  
    float wx = *(float*)&params[1]; // Convertir correctamente desde int64_t  
    float wy = *(float*)&params[2];  
    float wz = *(float*)&params[3];  
  
    if (heightmap_id < 0 || heightmap_id >= MAX_HEIGHTMAPS)  
        return 0;  
  
    // Diferencia respecto a la cámara  
    float dx = wx - camera.x;  
    float dy = wy - camera.y;  
  
    // Convertir ángulo a radianes y aplicar rotación inversa  
    float angle_rad = -camera.angle * M_PI / 180.0f;  
    float x_cam = dx * cosf(angle_rad) - dy * sinf(angle_rad);  
    float z_cam = dx * sinf(angle_rad) + dy * cosf(angle_rad);  
  
    // Rechazar objetos detrás de la cámara  
    if (z_cam < 1.0f)  
        return 0;  
  
    // Proyección en perspectiva  
    float fov_scale = 240.0f / tanf(camera.fov * 0.5f * M_PI / 180.0f);  
    int screen_x = (int)(160 + x_cam * fov_scale / z_cam);  // Centrado en 160 para 320px  
    int screen_y = (int)(120 - (wz - camera.z) * fov_scale / z_cam); // Centrado en 120 para 240px  
  
    // Verificar límites de pantalla  
    if (screen_x < -50 || screen_x > 370 || screen_y < -50 || screen_y > 290)  
        return 0;  
  
    // Empaquetar: screen_x (16 bits) | screen_y (16 bits) | z_cam como float (32 bits)  
    uint32_t z_as_uint = *(uint32_t*)&z_cam;  
    return (int64_t)(((uint64_t)(uint16_t)screen_x << 48) |  
                     ((uint64_t)(uint16_t)screen_y << 32) |  
                     ((uint64_t)z_as_uint));  
}


// Función para añadir billboards directamente al sistema voxelspace  
int64_t libmod_heightmap_add_voxel_billboard(INSTANCE *my, int64_t *params) {      
    float world_x = *(float*)&params[0];      
    float world_y = *(float*)&params[1];         
    float height_offset = *(float*)&params[2];      
    int graph_id = params[3];      
    float scale = *(float*)&params[4];    
          
    // Buscar el primer heightmap válido disponible      
    HEIGHTMAP *hm = NULL;      
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {      
        if (heightmaps[i].cache_valid && heightmaps[i].width > 0 && heightmaps[i].height > 0) {      
            hm = &heightmaps[i];      
            break;      
        }      
    }      
      
    float terrain_height = 0.0f;      
    if (hm) {      
        terrain_height = get_height_at(hm, world_x, world_y);      
    }      
          
    float final_world_z = terrain_height + height_offset + 10.0f;      
          
 // USAR EL ARRAY static_billboards      
if (static_billboard_count < MAX_STATIC_BILLBOARDS) {          
    static_billboards[static_billboard_count].world_x = world_x;          
    static_billboards[static_billboard_count].world_y = world_y;          
    static_billboards[static_billboard_count].world_z = final_world_z;          
    static_billboards[static_billboard_count].graph_id = graph_id;          
    static_billboards[static_billboard_count].scale = scale;      
    static_billboards[static_billboard_count].active = 1;        
    static_billboards[static_billboard_count].process_id = 0;      
    // AGREGAR ESTA LÍNEA CRÍTICA:    
    static_billboards[static_billboard_count].billboard_type = 0; // Tipo por defecto (estático)    
              
    return static_billboard_count++;      
}          
return -1;
}

int64_t libmod_heightmap_water_texture(INSTANCE *my, int64_t *params) {  
    const char *texture_path = string_get(params[0]);  
    int alpha_override = params[1]; // Nuevo parámetro alpha (0-255)  
      
    water_texture_id = gr_load_img(texture_path);  
    string_discard(params[0]);  
      
    if (water_texture_id > 0) {  
        water_texture = bitmap_get(0, water_texture_id);  
          
        // Guardar el alpha override para usar en el renderizado  
        water_texture_alpha_override = alpha_override;  
          
        return 1;  
    }  
    return 0;  
}
   
int64_t libmod_heightmap_register_billboard(INSTANCE *my, int64_t *params) {  
    int64_t process_id = params[0];  
    float world_x = *(float*)&params[1];  
    float world_y = *(float*)&params[2];  
    float world_z = *(float*)&params[3];  
    int64_t graph_id = params[4];  
    int64_t billboard_type = params[5];  // NUEVO: tipo de billboard  
      
    // Buscar en billboards dinámicos  
    for (int i = 0; i < MAX_DYNAMIC_BILLBOARDS; i++) {  
        if (!dynamic_billboards[i].active) {  
            dynamic_billboards[i].active = 1;  
            dynamic_billboards[i].process_id = process_id;  
            dynamic_billboards[i].world_x = world_x;  
            dynamic_billboards[i].world_y = world_y;  
            dynamic_billboards[i].world_z = world_z;  
            dynamic_billboards[i].graph_id = graph_id;  
            dynamic_billboards[i].billboard_type = billboard_type;  // Almacenar tipo  
            return i + MAX_STATIC_BILLBOARDS;  
        }  
    }  
    return -1;  
}

int64_t libmod_heightmap_update_billboard(INSTANCE *my, int64_t *params) {    

    int64_t process_id = params[0];    

    float world_x = *(float*)&params[1];    

    float world_y = *(float*)&params[2];    

    float world_z = *(float*)&params[3];    

        

    // Buscar en billboards dinámicos  

    for (int i = 0; i < MAX_DYNAMIC_BILLBOARDS; i++) {    

        if (dynamic_billboards[i].active && dynamic_billboards[i].process_id == process_id) {    

            dynamic_billboards[i].world_x = world_x;    

            dynamic_billboards[i].world_y = world_y;    

            dynamic_billboards[i].world_z = world_z;    

            return 1;  

        }    

    }    

    return 0;  

}

int64_t libmod_heightmap_update_billboard_graph(INSTANCE *my, int64_t *params) {  
    int64_t process_id = params[0];  
    int64_t new_graph_id = params[1];  
      
    for (int i = 0; i < MAX_DYNAMIC_BILLBOARDS; i++) {  
        if (dynamic_billboards[i].active && dynamic_billboards[i].process_id == process_id) {  
            dynamic_billboards[i].graph_id = new_graph_id;  
            return 1;  
        }  
    }  
    return 0;  
}

int64_t libmod_heightmap_unregister_billboard(INSTANCE *my, int64_t *params) {      
    int64_t process_id = params[0];      
          
    for (int i = 0; i < MAX_DYNAMIC_BILLBOARDS; i++) {      
        if (dynamic_billboards[i].active && dynamic_billboards[i].process_id == process_id) {      
            dynamic_billboards[i].active = 0;      
            dynamic_billboards[i].process_id = 0; // Limpiar process_id    
            return 1; // Éxito      
        }      
    }      
    return 0; // No encontrado      
}

int64_t libmod_heightmap_set_wave_amplitude(INSTANCE *my, int64_t *params)  
{  
    float amplitude = *(float*)&params[0];  
    if (amplitude < 0.0f) amplitude = 0.0f;  
    if (amplitude > 5.0f) amplitude = 5.0f; // Límite razonable  
    wave_amplitude = amplitude;  
    return 1;  
}


int64_t libmod_heightmap_set_camera_follow(INSTANCE *my, int64_t *params)  

{  

    camera_follow_sprite_id = params[0]; // ID del sprite a seguir (0 = desactivar)  

  

    if (params[1] != -1)  

        camera_follow_offset_x = (float)params[1] / 10.0f;  

    if (params[2] != -1)  

        camera_follow_offset_y = (float)params[2] / 10.0f;  

    if (params[3] != -1)  

        camera_follow_offset_z = (float)params[3] / 10.0f;  

    if (params[4] != -1)  

        camera_follow_speed = (float)params[4];  

  

    return 1;  

}

int64_t libmod_heightmap_set_fog_color(INSTANCE *my, int64_t *params)  
{  
    Uint8 new_r = (Uint8)params[0];  
    Uint8 new_g = (Uint8)params[1];  
    Uint8 new_b = (Uint8)params[2];  
    float new_intensity = (float)params[3] / 1000.0f;  
      
    // Invalidar tabla si los parámetros cambiaron  
    if (new_r != fog_color_r || new_g != fog_color_g ||   
        new_b != fog_color_b || new_intensity != fog_intensity) {  
        fog_table_initialized = 0;  
    }  
      
    fog_color_r = new_r;  
    fog_color_g = new_g;  
    fog_color_b = new_b;  
    fog_intensity = new_intensity;  
      
    return 1;  
}


int64_t libmod_heightmap_set_billboard_fov(INSTANCE *my, int64_t *params) {  
    billboard_render_fov = (float)params[0] / 1000.0f;  // Convertir de milésimas  
    if (billboard_render_fov < 0.5f) billboard_render_fov = 0.5f;  
    if (billboard_render_fov > 3.0f) billboard_render_fov = 3.0f;  
    return 1;  
}


// Funciones helper internas - NO exportadas  
static void move_camera_direction(float angle_offset, float speed_factor, float speed) {  
    float final_speed = (speed > 0) ? speed : move_speed;  
    camera.x += cosf(camera.angle + angle_offset) * final_speed / speed_factor;  
    camera.y += sinf(camera.angle + angle_offset) * final_speed / speed_factor;  
}  
  
// Funciones exportadas simplificadas  
int64_t libmod_heightmap_move_forward(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_direction(0.0f, 1.5f, speed);  
    return 1;  
}  
  
int64_t libmod_heightmap_move_backward(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_direction(M_PI, 1.5f, speed);  
    return 1;  
}  
  
int64_t libmod_heightmap_strafe_left(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_direction(-M_PI_2, 2.0f, speed);  
    return 1;  
}  
  
int64_t libmod_heightmap_strafe_right(INSTANCE *my, int64_t *params) {  
    float speed = (params[0] > 0) ? (float)params[0] : move_speed;  
    move_camera_direction(M_PI_2, 2.0f, speed);  
    return 1;  
}


// Helper interno para movimiento con colisión - NO exportado  
static int move_camera_with_collision(int64_t hm_id, float angle_offset, float speed_factor, float speed) {  
    HEIGHTMAP *hm = find_heightmap_by_id(hm_id);  
    if (!hm || !hm->cache_valid) return 0;  
      
    float new_x = camera.x + cosf(camera.angle + angle_offset) * speed / speed_factor;  
    float new_y = camera.y + sinf(camera.angle + angle_offset) * speed / speed_factor;  
      
    return apply_terrain_collision(hm, new_x, new_y);  
}  
  
// Helper para búsqueda de heightmap  
static HEIGHTMAP* find_heightmap_by_id(int64_t hm_id) {  
    for (int i = 0; i < MAX_HEIGHTMAPS; i++) {  
        if (heightmaps[i].id == hm_id) {  
            return &heightmaps[i];  
        }  
    }  
    return NULL;  
}  
  
// Helper para aplicar colisión con terreno  
static int apply_terrain_collision(HEIGHTMAP *hm, float new_x, float new_y) {  
    if (new_x >= 2.0f && new_x < hm->width - 2.0f &&  
        new_y >= 2.0f && new_y < hm->height - 2.0f) {  
          
        float terrain_height = get_height_at(hm, new_x, new_y);  
        float current_terrain_height = get_height_at(hm, camera.x, camera.y);  
        float height_diff = fabs(terrain_height - current_terrain_height);  
          
        if (height_diff < 20.0f) {  
            camera.x = new_x;  
            camera.y = new_y;  
              
            float min_height = terrain_height + 5.0f;  
            if (camera.z < min_height) {  
                camera.z = min_height;  
            }  
        }  
    }  
    return 1;  
}

int64_t libmod_heightmap_move_forward_with_collision(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;  
    return move_camera_with_collision(hm_id, 0.0f, 1.5f, speed);  
}  
  
int64_t libmod_heightmap_move_backward_with_collision(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;  
    return move_camera_with_collision(hm_id, M_PI, 1.5f, speed);  
}  
  
int64_t libmod_heightmap_strafe_left_with_collision(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;  
    return move_camera_with_collision(hm_id, -M_PI_2, 2.0f, speed);  
}  
  
int64_t libmod_heightmap_strafe_right_with_collision(INSTANCE *my, int64_t *params) {  
    int64_t hm_id = params[0];  
    float speed = (params[1] > 0) ? (float)params[1] : move_speed;  
    return move_camera_with_collision(hm_id, M_PI_2, 2.0f, speed);  
}

static float convert_screen_to_world_coordinate(int heightmap_id, float screen_coord, int is_x_axis) {  
    HEIGHTMAP *hm = find_heightmap_by_id(heightmap_id);  
    if (!hm) return 0.0f;  
      
    float reference_size = is_x_axis ? 640.0f : 480.0f;  
    float map_size = is_x_axis ? hm->width : hm->height;  
    float factor = map_size / reference_size;  
      
    return screen_coord / factor;  
}

float libmod_heightmap_convert_screen_to_world_x(INSTANCE *my, int64_t *params) {  
    int heightmap_id = params[0];  
    float screen_x = *(float*)&params[1];  
    return convert_screen_to_world_coordinate(heightmap_id, screen_x, 1); // 1 = eje X  
}  
  
float libmod_heightmap_convert_screen_to_world_y(INSTANCE *my, int64_t *params) {  
    int heightmap_id = params[0];  
    float screen_y = *(float*)&params[1];  
    return convert_screen_to_world_coordinate(heightmap_id, screen_y, 0); // 0 = eje Y  
}

static void collect_visible_billboards_from_array(VOXEL_BILLBOARD *billboard_array, int array_size,   
                                                  BILLBOARD_RENDER_DATA *visible_billboards,   
                                                  int *visible_count, float terrain_fov) {  
    for (int i = 0; i < array_size; i++) {  
        if (!billboard_array[i].active) continue;  
          
        VOXEL_BILLBOARD *bb = &billboard_array[i];  
        GRAPH *billboard_graph = bitmap_get(0, bb->graph_id);  
          
        if (!billboard_graph) continue;  
          
        BILLBOARD_PROJECTION proj = calculate_proyection(bb, billboard_graph, terrain_fov);  
        if (!proj.valid) continue;  
          
        visible_billboards[*visible_count].billboard = bb;  
        visible_billboards[*visible_count].projection = proj;  
        visible_billboards[*visible_count].graph = billboard_graph;  
        visible_billboards[*visible_count].distance = proj.distance;  
        (*visible_count)++;  
    }  
}

#include "libmod_heightmap_exports.h"