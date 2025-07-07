# libmod_heightmap - Módulo de terreno 3D para BennuGD2

Este módulo permite trabajar con terrenos en 3D simulados mediante heightmaps (mapas de altura) y renderizarlos con estilo voxelspace. Incluye funciones de control de cámara, detección de colisiones, iluminación, nivel de agua y seguimiento de sprites.

---
![Imagen 1 ](/images/screen1.png)
![Imagen 2](/images/screen2.png)
![Imagen 3](/images/screen3.png)
![Imagen 4](/images/screen4.png)
![Imagen 5](/images/screen5.png)
![Imagen 6](/images/screen6.png)

# libmod_heightmap for BennuGD2

Este módulo proporciona un sistema de renderizado tipo "voxelspace" con heightmaps para BennuGD2, permitiendo simulación de terrenos 3D, efectos de agua, iluminación, texturas y una cámara en primera persona, así como integración de sprites en el mundo simulado.

## Constantes Exportadas

| Constante                  | Descripción                                      |
|---------------------------|--------------------------------------------------|
| `HEIGHTMAP_MAX`           | Número máximo de heightmaps permitidos          |
| `HEIGHTMAP_DEFAULT_FOV`   | Campo de visión por defecto (FOV x1000)         |

---

## Funciones Exportadas

### Carga y Creación de Terrenos

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_LOAD(filename)` | Carga un heightmap desde archivo. Retorna ID. |
| `HEIGHTMAP_LOAD_TEXTURE(id, filename)` | Asocia una textura al heightmap especificado. |
| `HEIGHTMAP_CREATE(width, height)` | Crea un heightmap vacío de dimensiones dadas. |
| `HEIGHTMAP_CREATE_PROCEDURAL(width, height)` | Crea un terreno con generación procedural. |
| `HEIGHTMAP_UNLOAD(id)` | Libera el heightmap de la memoria. |

---

### Renderizado

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_RENDER_3D(id, width, height)` | Renderiza el terreno en pantalla. |
| `HEIGHTMAP_SET_RENDER_DISTANCE(distance)` | Define la distancia máxima de renderizado. |
| `HEIGHTMAP_SET_CHUNK_CONFIG(width, height)` | Configura el tamaño de los "chunks" internos. |

---

### Cámara

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_CAMERA(x, y, z, angle, pitch, zoom)` | Define la posición y orientación de la cámara. |
| `HEIGHTMAP_INIT_CAMERA_ON_TERRAIN(id)` | Inicializa la cámara sobre el terreno dado. |
| `HEIGHTMAP_GET_CAMERA_POSITION(&x, &y, &z, &angle, &pitch)` | Devuelve la posición actual de la cámara. |

---

### Control de Movimiento

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_CONTROL_SENSITIVITY(forward, strafe, look)` | Ajusta la sensibilidad del control. |
| `HEIGHTMAP_MOVE_FORWARD()` | Avanza la cámara. |
| `HEIGHTMAP_MOVE_FORWARD(speed)` | Avanza con velocidad específica. |
| `HEIGHTMAP_MOVE_BACKWARD()` | Retrocede. |
| `HEIGHTMAP_MOVE_BACKWARD(speed)` | Retrocede con velocidad específica. |
| `HEIGHTMAP_STRAFE_LEFT()` | Se mueve lateralmente a la izquierda. |
| `HEIGHTMAP_STRAFE_LEFT(speed)` | Igual, con velocidad. |
| `HEIGHTMAP_STRAFE_RIGHT()` | Se mueve a la derecha. |
| `HEIGHTMAP_STRAFE_RIGHT(speed)` | Igual, con velocidad. |
| `HEIGHTMAP_LOOK_HORIZONTAL(value)` | Rota la cámara en horizontal. |
| `HEIGHTMAP_LOOK_VERTICAL(value)` | Rota la cámara en vertical. |
| `HEIGHTMAP_ADJUST_HEIGHT(value)` | Ajusta la altura de la cámara. |

---

### Iluminación y Cielo

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_LIGHT(intensity)` | Define la iluminación general del terreno. |
| `HEIGHTMAP_SET_SKY_COLOR(r, g, b, alpha)` | Color del cielo. |
| `HEIGHTMAP_GET_TERRAIN_LIGHTING(x, y, z)` | Luz del terreno en esa posición. |

---

### Agua

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_WATER_LEVEL(level)` | Define la altura del agua. |
| `HEIGHTMAP_SET_WATER_COLOR(r, g, b, alpha)` | Define el color y transparencia del agua. |
| `HEIGHTMAP_UPDATE_WATER_TIME()` | Actualiza la animación de las olas. |

---

### Detección de Altura y Colisión

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_GET_HEIGHT(id, x, y)` | Obtiene altura del terreno en esas coordenadas. |
| `HEIGHTMAP_CHECK_TERRAIN_COLLISION(radius)` | Verifica colisión del punto actual de la cámara. |
| `HEIGHTMAP_MOVE_FORWARD_WITH_COLLISION(speed, radius)` | Avanza con detección de colisiones. |
| `HEIGHTMAP_MOVE_BACKWARD_WITH_COLLISION(speed, radius)` | Retrocede con detección. |
| `HEIGHTMAP_STRAFE_LEFT_WITH_COLLISION(speed, radius)` | Mueve a la izquierda con colisión. |
| `HEIGHTMAP_STRAFE_RIGHT_WITH_COLLISION(speed, radius)` | Mueve a la derecha con colisión. |
| `HEIGHTMAP_CAN_SPRITE_MOVE_TO(x, y, z, radius)` | Verifica si un sprite puede moverse a la posición. |
| `HEIGHTMAP_GET_TERRAIN_HEIGHT_AT_SPRITE(id, x, y)` | Altura del terreno bajo un sprite. |
| `HEIGHTMAP_ADJUST_SPRITE_TO_TERRAIN(id, sprite_id, offset, &result)` | Ajusta sprite a la superficie. |

---

### Sprites en el Mundo 3D

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_GET_SPRITE_SCALE(sprite_id, base_height)` | Escala un sprite basado en profundidad. |
| `HEIGHTMAP_WORLD_TO_SCREEN(x, y, z, &screen_x, &screen_y)` | Convierte coordenadas del mundo a pantalla. |

---

### Seguimiento de Cámara

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_CAMERA_FOLLOW(sprite_id, offset_x, offset_y, offset_z, style)` | Hace que la cámara siga a un sprite. |
| `HEIGHTMAP_UPDATE_CAMERA_FOLLOW(offset_angle, offset_pitch, style)` | Actualiza parámetros de seguimiento. |
| `HEIGHTMAP_GET_CAMERA_FOLLOW()` | Devuelve el ID del sprite seguido. |

---

## Requisitos

- **BennuGD2**
- **SDL2**
- Texturas en formato soportado (ej: PNG)
- Heightmaps en formato RAW u otro compatible

## Créditos

Desarrollado como módulo adicional para BennuGD2 por la comunidad.
## Ejemplo básico de uso

```bennu
// Programa simple: control directo de cámara como nave con ratón  
import "libmod_gfx";  
import "libmod_input";  
import "libmod_misc";  
import "libmod_heightmap";  
  
GLOBAL  
    int heightmap_id;  
    int nave_graph;  
      
    // Variables para mostrar información de cámara  
    int cam_x, cam_y, cam_z;  
    int cam_angle, cam_pitch;  
      
    // Variables para control de ratón  
    int mouse_last_x, mouse_last_y;  
    int mouse_delta_x, mouse_delta_y;  
    int mouse_sensitivity = 2;  
  
PROCESS main()  
BEGIN  
    // Configuración inicial de pantalla  
    set_mode(640, 480);  
    set_fps(60, 0);  
    window_set_title("Nave sobre Terreno Voxelspace - Control Directo + Ratón");  
      
    // Cargar recursos  
    nave_graph = map_load("sprite.png");  
    if (!nave_graph)  
        say("Error: No se pudo cargar sprite.png");  
        return;  
    end  
      
    // Cargar terreno  
    heightmap_id = heightmap_load("terrain.png");  
    if (!heightmap_id)  
        say("Error: No se pudo cargar terrain.png");  
        return;  
    end  
      
    // Cargar textura del terreno (opcional)  
    heightmap_load_texture(heightmap_id, "terrain_texture.png");  
      
    // Configurar sistema  
    heightmap_init_camera_on_terrain(heightmap_id);  
    heightmap_set_control_sensitivity(30, 3, 2);  
    heightmap_set_light(200);  
      
    // Inicializar posición del ratón  
    mouse_last_x = mouse.x;  
    mouse_last_y = mouse.y;  
      
    // Crear terreno y nave  
    terrain_display();  
    nave_display();  
      
    // Bucle principal con controles directos  
    LOOP  
        // Control de movimiento con teclado  
        if (key(_up) || key(_w))  
            heightmap_move_forward_with_collision(heightmap_id, 0);  
        end  
          
        if (key(_down) || key(_s))  
            heightmap_move_backward_with_collision(heightmap_id, 0);  
        end  
          
        if (key(_left) || key(_a))  
            heightmap_strafe_left_with_collision(heightmap_id, 0);  
        end  
          
        if (key(_right) || key(_d))  
            heightmap_strafe_right_with_collision(heightmap_id, 0);  
        end  
          
        // Movimiento vertical  
        if (key(_space))  
            heightmap_adjust_height(10);  
        end  
          
        if (key(_c))  
            heightmap_adjust_height(-10);  
        end  
          
        // Control de rotación con ratón  
        mouse_delta_x = mouse.x - mouse_last_x;  
        mouse_delta_y = mouse.y - mouse_last_y;  
          
        if (mouse_delta_x != 0)  
            heightmap_look_horizontal(mouse_delta_x * mouse_sensitivity);  
        end  
          
        if (mouse_delta_y != 0)  
            heightmap_look_vertical(mouse_delta_y * mouse_sensitivity);  
        end  
          
        // Actualizar posición anterior del ratón  
        mouse_last_x = mouse.x;  
        mouse_last_y = mouse.y;  
          
        // Rotación adicional con teclado (opcional)  
        if (key(_q))  
            heightmap_look_horizontal(-10);  
        end  
          
        if (key(_e))  
            heightmap_look_horizontal(10);  
        end  
          
        // Control de pantalla completa  
        if (key(_f))  
            toggle_fullscreen();  
        end  
          
        // Salir con ESC  
        if (key(_esc))  
            exit();  
        end  
          
        // Obtener posición de cámara para mostrar  
        heightmap_get_camera_position(&cam_x, &cam_y, &cam_z, &cam_angle, &cam_pitch);  
          
        // Mostrar información  
        display_info();  
          
        FRAME;  
    END  
END  
  
PROCESS terrain_display()  
BEGIN  
    x = 320;  
    y = 240;  
    z = 1000; // Fondo  
    size = 200;  
      
    LOOP  
        // Renderizar terreno voxelspace  
        graph = heightmap_render_3d(heightmap_id, 320, 240);  
          
        FRAME;  
    END  
END  
  
PROCESS nave_display()  
BEGIN  
    // La nave siempre está en el centro de la pantalla  
    x = 320;  
    y = 240;  
    z = -10; // Delante del terreno  
    size = 80;  
      
    graph = nave_graph;  
      
    LOOP  
        // La nave no se mueve, la cámara se mueve alrededor  
        // Esto simula que la nave vuela por el mundo  
          
        FRAME;  
    END  
END  
  
PROCESS display_info()  
BEGIN  
    LOOP  
        // Información de posición  
        write(0, 10, 10, 0, "Posicion X: " + cam_x);  
        write(0, 10, 25, 0, "Posicion Y: " + cam_y);  
        write(0, 10, 40, 0, "Altura Z: " + cam_z);  
        write(0, 10, 55, 0, "Angulo: " + cam_angle);  
        write(0, 10, 70, 0, "Pitch: " + cam_pitch);  
          
        // Controles  
        write(0, 10, 100, 0, "CONTROLES:");  
        write(0, 10, 115, 0, "WASD: Mover nave");  
        write(0, 10, 130, 0, "Ratón: Mirar alrededor");  
        write(0, 10, 145, 0, "Espacio/C: Subir/Bajar");  
        write(0, 10, 160, 0, "Q/E: Rotar (teclado)");  
        write(0, 10, 175, 0, "F: Pantalla completa");  
        write(0, 10, 190, 0, "ESC: Salir");  
          
        write(0, 10, 220, 0, "Mouse Delta X: " + mouse_delta_x + " Y: " + mouse_delta_y);  
        write(0, 10, 235, 0, "Sensibilidad: " + mouse_sensitivity);  
          
        FRAME;  
        write_delete(all_text);  
    END  
END  
  
FUNCTION toggle_fullscreen()  
BEGIN  
    if (screen.fullscreen == 0)  
        set_mode(640,480,mode_fullscreen);
    else  
        screen.fullscreen = 0;  
    end  
END
```

---

## Autor

Módulo desarrollado para experimentación 3D en BennuGD2 con motor estilo voxelspace, renderizado por software.

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Rufidj/libmod_heightmap)
