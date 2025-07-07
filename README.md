# libmod_heightmap - Módulo de terreno 3D para BennuGD2

Este módulo permite trabajar con terrenos en 3D simulados mediante heightmaps (mapas de altura) y renderizarlos con estilo voxelspace. Incluye funciones de control de cámara, detección de colisiones, iluminación, nivel de agua, sprites en el mundo y seguimiento de cámara.

---

![Imagen 1 ](/images/screen1.png)
![Imagen 2](/images/screen2.png)
![Imagen 3](/images/screen3.png)
![Imagen 4](/images/screen4.png)
![Imagen 5](/images/screen5.png)
![Imagen 6](/images/screen6.png)

---

## Constantes Exportadas

| Constante                | Descripción                                      |
|-------------------------|--------------------------------------------------|
| `HEIGHTMAP_MAX`         | Número máximo de heightmaps permitidos          |
| `HEIGHTMAP_DEFAULT_FOV` | Campo de visión por defecto (FOV x1000)         |
| `C_BILLBOARD`           | Flag para identificar sprites como billboard    |

---

## Funciones Exportadas

### Carga y Creación de Terrenos

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_LOAD(filename)` | Carga un heightmap desde archivo. |
| `HEIGHTMAP_LOAD_TEXTURE(id, filename)` | Asocia una textura al terreno. |
| `HEIGHTMAP_CREATE(width, height)` | Crea un heightmap vacío. |
| `HEIGHTMAP_CREATE_PROCEDURAL(width, height)` | Crea un heightmap procedural. |
| `HEIGHTMAP_UNLOAD(id)` | Libera recursos del heightmap. |

---

### Renderizado

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_RENDER_3D(id, width, height)` | Renderiza el terreno a pantalla. |
| `HEIGHTMAP_SET_RENDER_DISTANCE(distance)` | Establece distancia máxima de renderizado. |
| `HEIGHTMAP_SET_CHUNK_CONFIG(width, height)` | Configura tamaño de los "chunks" internos. |

---

### Cámara

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_CAMERA(x, y, z, angle, pitch, zoom)` | Posiciona la cámara. |
| `HEIGHTMAP_GET_CAMERA_POSITION(&x, &y, &z, &angle, &pitch)` | Devuelve la posición de la cámara. |
| `HEIGHTMAP_INIT_CAMERA_ON_TERRAIN(id)` | Sitúa la cámara sobre el terreno. |

---

### Control de Movimiento

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_CONTROL_SENSITIVITY(forward, strafe, look)` | Ajusta sensibilidad de movimiento. |
| `HEIGHTMAP_MOVE_FORWARD()` / `HEIGHTMAP_MOVE_FORWARD(speed)` | Avanza la cámara. |
| `HEIGHTMAP_MOVE_BACKWARD()` / `HEIGHTMAP_MOVE_BACKWARD(speed)` | Retrocede la cámara. |
| `HEIGHTMAP_STRAFE_LEFT()` / `HEIGHTMAP_STRAFE_LEFT(speed)` | Desplaza a la izquierda. |
| `HEIGHTMAP_STRAFE_RIGHT()` / `HEIGHTMAP_STRAFE_RIGHT(speed)` | Desplaza a la derecha. |
| `HEIGHTMAP_LOOK_HORIZONTAL(value)` | Gira horizontalmente. |
| `HEIGHTMAP_LOOK_VERTICAL(value)` | Gira verticalmente. |
| `HEIGHTMAP_ADJUST_HEIGHT(value)` | Ajusta altura vertical de la cámara. |

---

### Iluminación y Cielo

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_LIGHT(intensity)` | Ajusta luz ambiental. |
| `HEIGHTMAP_SET_SKY_COLOR(r, g, b, alpha)` | Define el color del cielo. |
| `HEIGHTMAP_SET_SKY_TEXTURE(filename, id)` | Asigna una textura al cielo. |
| `HEIGHTMAP_GET_TERRAIN_LIGHTING(x, y, z)` | Valor de iluminación en posición dada. |

---

### Agua

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_WATER_LEVEL(level)` | Ajusta la altura del agua. |
| `HEIGHTMAP_SET_WATER_TEXTURE(filename, id)` | Asocia textura de agua. |
| `HEIGHTMAP_UPDATE_WATER_TIME()` | Anima el agua (olas). |
| `HEIGHTMAP_SET_WAVE_AMPLITUDE(amplitude)` | Controla la fuerza de las olas. |

---

### Colisión y Altura

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_GET_HEIGHT(id, x, y)` | Obtiene altura del terreno en X/Y. |
| `HEIGHTMAP_CHECK_TERRAIN_COLLISION(radius)` | Verifica colisión desde la cámara. |
| `HEIGHTMAP_MOVE_FORWARD_WITH_COLLISION(speed, radius)` | Avanza con colisión. |
| `HEIGHTMAP_MOVE_BACKWARD_WITH_COLLISION(speed, radius)` | Retrocede con colisión. |
| `HEIGHTMAP_STRAFE_LEFT_WITH_COLLISION(speed, radius)` | Izquierda con colisión. |
| `HEIGHTMAP_STRAFE_RIGHT_WITH_COLLISION(speed, radius)` | Derecha con colisión. |
| `HEIGHTMAP_CAN_SPRITE_MOVE_TO(x, y, z, radius)` | Verifica si un sprite puede ir a esa posición. |
| `HEIGHTMAP_GET_TERRAIN_HEIGHT_AT_SPRITE(id, x, y)` | Altura bajo un sprite. |
| `HEIGHTMAP_ADJUST_SPRITE_TO_TERRAIN(id, sprite_id, offset, &result)` | Ajusta el sprite al terreno. |

---

### Sprites en 3D

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_WORLD_TO_SCREEN(x, y, z, &sx, &sy)` | Convierte posición 3D a coordenadas 2D en pantalla. |

---

### Seguimiento de Cámara

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_SET_CAMERA_FOLLOW(sprite_id, ox, oy, oz, style)` | Hace que la cámara siga a un sprite. |
| `HEIGHTMAP_UPDATE_CAMERA_FOLLOW(offset_angle, offset_pitch, style)` | Ajusta seguimiento. |
| `HEIGHTMAP_GET_CAMERA_FOLLOW()` | Obtiene el sprite seguido. |

---

### Billboards

| Función | Descripción |
|--------|-------------|
| `HEIGHTMAP_CONVERT_SCREEN_TO_WORLD_X(id, screen_x)` | Convierte coordenada X de pantalla a mundo. |
| `HEIGHTMAP_CONVERT_SCREEN_TO_WORLD_Y(id, screen_y)` | Convierte coordenada Y de pantalla a mundo. |
| `HEIGHTMAP_ADD_VOXEL_BILLBOARD(x, y, graph_id, scale)` | Añade un sprite estilo voxel a la escena. |
| `HEIGHTMAP_REGISTER_BILLBOARD(id, x, y, z, graph)` | Registra billboard en el motor. |
| `HEIGHTMAP_UPDATE_BILLBOARD(id, x, y, z)` | Actualiza posición de billboard. |
| `HEIGHTMAP_UNREGISTER_BILLBOARD(id)` | Elimina billboard del motor. |

---

## Requisitos

- BennuGD2
- SDL2
- Texturas en formato PNG (u otros soportados)
- Heightmaps en formato RAW o PNG

---

## Créditos

Desarrollado como módulo experimental para simulación 3D estilo voxelspace en BennuGD2.

---

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Rufidj/libmod_heightmap)
