/*
 * Heightmap Module Exports for BennuGD2
 */

#ifndef __LIBMOD_HEIGHTMAP_EXPORTS
#define __LIBMOD_HEIGHTMAP_EXPORTS

#include "bgddl.h"

#if defined(__BGDC__) || !defined(__STATIC__)

#include "libmod_heightmap.h"

/* Constantes exportadas */
DLCONSTANT __bgdexport(libmod_heightmap, constants_def)[] = {
    {"HEIGHTMAP_MAX", TYPE_INT, MAX_HEIGHTMAPS},
    {"HEIGHTMAP_DEFAULT_FOV", TYPE_FLOAT, (int64_t)(DEFAULT_FOV * 1000)},
    {NULL, 0, 0}};

DLSYSFUNCS __bgdexport(libmod_heightmap, functions_exports)[] = {

    // Funciones existentes del heightmap
    FUNC("HEIGHTMAP_LOAD", "S", TYPE_INT, libmod_heightmap_load),
    FUNC("HEIGHTMAP_RENDER_3D", "III", TYPE_INT, libmod_heightmap_render_voxelspace),
    FUNC("HEIGHTMAP_SET_CAMERA", "IIIIII", TYPE_INT, libmod_heightmap_set_camera),
    FUNC("HEIGHTMAP_SET_LIGHT", "I", TYPE_INT, libmod_heightmap_set_light),
    FUNC("HEIGHTMAP_SET_WATER_LEVEL", "I", TYPE_INT, libmod_heightmap_set_water_level),
    FUNC("HEIGHTMAP_SET_WATER_COLOR" , "IIII" , TYPE_INT , libmod_heightmap_set_water_color ),  
    FUNC("HEIGHTMAP_UPDATE_WATER_TIME", "" , TYPE_INT , libmod_heightmap_update_water_time ),
    FUNC("HEIGHTMAP_SET_SKY_COLOR", "IIII", TYPE_INT, libmod_heightmap_set_sky_color),
    FUNC("HEIGHTMAP_LOAD_TEXTURE", "IS", TYPE_INT, libmod_heightmap_load_texture),
    FUNC("HEIGHTMAP_GET_HEIGHT", "III", TYPE_INT, libmod_heightmap_get_height),
    FUNC("HEIGHTMAP_CREATE", "II", TYPE_INT, libmod_heightmap_create),
    FUNC("HEIGHTMAP_UNLOAD", "I", TYPE_INT, libmod_heightmap_unload),
    FUNC( "HEIGHTMAP_SET_RENDER_DISTANCE", "I", TYPE_INT, libmod_heightmap_set_render_distance ),  
    FUNC( "HEIGHTMAP_SET_CHUNK_CONFIG", "II", TYPE_INT, libmod_heightmap_set_chunk_config ), 

    // Nuevas funciones de control (solo las que están implementadas)
    FUNC("HEIGHTMAP_SET_CONTROL_SENSITIVITY", "III", TYPE_INT, libmod_heightmap_set_control_sensitivity),
    FUNC("HEIGHTMAP_MOVE_FORWARD", "", TYPE_INT, libmod_heightmap_move_forward),
    FUNC("HEIGHTMAP_MOVE_FORWARD", "I", TYPE_INT, libmod_heightmap_move_forward),
    FUNC("HEIGHTMAP_MOVE_BACKWARD", "", TYPE_INT, libmod_heightmap_move_backward),
    FUNC("HEIGHTMAP_MOVE_BACKWARD", "I", TYPE_INT, libmod_heightmap_move_backward),
    FUNC("HEIGHTMAP_STRAFE_LEFT", "", TYPE_INT, libmod_heightmap_strafe_left),
    FUNC("HEIGHTMAP_STRAFE_LEFT", "I", TYPE_INT, libmod_heightmap_strafe_left),
    FUNC("HEIGHTMAP_STRAFE_RIGHT", "", TYPE_INT, libmod_heightmap_strafe_right),
    FUNC("HEIGHTMAP_STRAFE_RIGHT", "I", TYPE_INT, libmod_heightmap_strafe_right),
    FUNC("HEIGHTMAP_LOOK_HORIZONTAL", "I", TYPE_INT, libmod_heightmap_look_horizontal),
    FUNC("HEIGHTMAP_LOOK_VERTICAL", "I", TYPE_INT, libmod_heightmap_look_vertical),
    FUNC("HEIGHTMAP_ADJUST_HEIGHT", "I", TYPE_INT, libmod_heightmap_adjust_height),
    FUNC("HEIGHTMAP_CREATE_PROCEDURAL", "II", TYPE_INT, libmod_heightmap_create_procedural),
    FUNC("HEIGHTMAP_GET_CAMERA_POSITION", "PPPPP", TYPE_INT, libmod_heightmap_get_camera_position),
    FUNC("HEIGHTMAP_INIT_CAMERA_ON_TERRAIN", "I", TYPE_INT, libmod_heightmap_init_camera_on_terrain),

    // funciones de colosion con el terreno (afecta a camara y sprites)
    FUNC("HEIGHTMAP_CHECK_TERRAIN_COLLISION", "I", TYPE_INT, libmod_heightmap_check_terrain_collision),
    FUNC("HEIGHTMAP_MOVE_FORWARD_WITH_COLLISION", "II", TYPE_INT, libmod_heightmap_move_forward_with_collision),
    FUNC("HEIGHTMAP_MOVE_BACKWARD_WITH_COLLISION", "II", TYPE_INT, libmod_heightmap_move_backward_with_collision),
    FUNC("HEIGHTMAP_STRAFE_LEFT_WITH_COLLISION", "II", TYPE_INT, libmod_heightmap_strafe_left_with_collision),
    FUNC("HEIGHTMAP_STRAFE_RIGHT_WITH_COLLISION", "II", TYPE_INT, libmod_heightmap_strafe_right_with_collision),
    FUNC("HEIGHTMAP_GET_TERRAIN_HEIGHT_AT_SPRITE", "III", TYPE_INT, libmod_heightmap_get_terrain_height_at_sprite),
    FUNC("HEIGHTMAP_CAN_SPRITE_MOVE_TO", "IIII", TYPE_INT, libmod_heightmap_can_sprite_move_to),
    FUNC("HEIGHTMAP_ADJUST_SPRITE_TO_TERRAIN", "IIIP", TYPE_INT, libmod_heightmap_adjust_sprite_to_terrain),

    // funciones para que los sprites se integren correctamente en el mundo "3d"
    FUNC("HEIGHTMAP_GET_SPRITE_SCALE", "II", TYPE_INT, libmod_heightmap_get_sprite_scale),
    FUNC("HEIGHTMAP_WORLD_TO_SCREEN", "IIIPP", TYPE_INT, libmod_heightmap_world_to_screen),
    FUNC("HEIGHTMAP_GET_TERRAIN_LIGHTING", "III", TYPE_INT, libmod_heightmap_get_terrain_lighting),

    // funciones de camara para los sprites
    FUNC("HEIGHTMAP_SET_CAMERA_FOLLOW", "IIIII", TYPE_INT, libmod_heightmap_set_camera_follow),
    FUNC("HEIGHTMAP_UPDATE_CAMERA_FOLLOW", "III", TYPE_INT, libmod_heightmap_update_camera_follow),
    FUNC("HEIGHTMAP_GET_CAMERA_FOLLOW", "", TYPE_INT, libmod_heightmap_get_camera_follow),
    FUNC(0, 0, 0, 0)};

#endif

/* Hooks del módulo */
void __bgdexport(libmod_heightmap, module_initialize)();
void __bgdexport(libmod_heightmap, module_finalize)();

#endif