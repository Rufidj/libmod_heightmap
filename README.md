# libmod_heightmap  
  
Módulo de renderizado de terrenos 3D estilo voxelspace para BennuGD2  
  
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Rufidj/libmod_heightmap)  
  
## 🎮 Descripción  
  
`libmod_heightmap` es un módulo avanzado para BennuGD2 que permite crear y renderizar terrenos 3D utilizando técnicas de voxelspace. [1-cite-0](#1-cite-0)  El módulo ofrece aproximadamente 130 funciones para gestión completa de terrenos, control de cámara, efectos ambientales y sprites 3D integrados. [1-cite-1](#1-cite-1)   
  
### Características Principales  
  
- ✨ **Renderizado Dual**: CPU (raycasting) y GPU (shaders)   
- 🗺️ **Hasta 512 heightmaps simultáneos** 
- 🎥 **Sistema de cámara 3D completo** con seguimiento automático  
- 🌊 **Efectos ambientales**: agua animada, niebla, skybox, iluminación  
- 🌲 **Sistema de billboards**: 500 estáticos + 500 dinámicos [   
- 💥 **Detección de colisiones** con el terreno 
- 🎨 **Generación procedural** de terrenos  
  
## 📸 Capturas de Pantalla  
  
![Demo 1](/images/screen1.png)  
![Demo 2](/images/screen2.png)  
![Demo 3](/images/screen3.png)  
  
## 🎥 Videos de Demostración  
  
[![Demo Coche](https://img.youtube.com/vi/-aPED4Rgk2E/0.jpg)](https://www.youtube.com/watch?v=-aPED4Rgk2E)  
[![Demo Efectos de Agua](https://img.youtube.com/vi/CiJBRTUzQIA/0.jpg)](https://www.youtube.com/watch?v=CiJBRTUzQIA)  
[![Demo modo CPU VS GPU](https://img.youtube.com/vi/UkpF9E0wFJ4/0.jpg)](https://youtu.be/UkpF9E0wFJ4)  
[![Demo modo GPU Efecto transparencia de Agua](https://img.youtube.com/vi/pfPku6mO9xM/0.jpg)](https://youtu.be/pfPku6mO9xM)     
## 🚀 Inicio Rápido  
  
### Requisitos  
  
- BennuGD2  
- SDL2  
- OpenGL + GLEW (para renderizado GPU)  
- Imágenes PNG para heightmaps y texturas  
  
### Ejemplo basico
  
```


import "libmod_heightmap";  
  
GLOBAL  
    int heightmap_id;  
END  
  
PROCESS main()  
BEGIN  
    set_mode(640, 480);  
      
    // Cargar terreno  
    heightmap_id = HEIGHTMAP_LOAD("terrain.png");  
    HEIGHTMAP_LOAD_TEXTURE(heightmap_id, "texture.png");  
      
    // Configurar cámara  
    HEIGHTMAP_INIT_CAMERA_ON_TERRAIN(heightmap_id);  
      
    // Configurar efectos  
    HEIGHTMAP_SET_LIGHT(200);  
    HEIGHTMAP_SET_WATER_LEVEL(20);  
    HEIGHTMAP_SET_SKY_COLOR(135, 206, 235, 255);  
      
    // Iniciar renderizado  
    terrain_display();  
      
    LOOP  
        HEIGHTMAP_UPDATE_WATER_TIME();  
          
        // Controles de cámara  
        if (key(_w)) HEIGHTMAP_MOVE_FORWARD_WITH_COLLISION(5, heightmap_id); end  
        if (key(_s)) HEIGHTMAP_MOVE_BACKWARD_WITH_COLLISION(5, heightmap_id); end  
        if (key(_left)) HEIGHTMAP_LOOK_HORIZONTAL(-5); end  
        if (key(_right)) HEIGHTMAP_LOOK_HORIZONTAL(5); end  
          
        FRAME;  
    END  
END  
  
PROCESS terrain_display()  
BEGIN  
    LOOP  
        graph = HEIGHTMAP_RENDER_3D(heightmap_id, 320, 240);  
        x = 320; y = 240; size = 200;  
        FRAME;  
    END  
END

```

### 📚 API Principal
## Gestión de Terrenos
## Función	Descripción
HEIGHTMAP_LOAD(filename)	Carga heightmap desde archivo PNG/RAW
HEIGHTMAP_CREATE(width, height)	Crea heightmap vacío
HEIGHTMAP_CREATE_PROCEDURAL(w, h)	Genera terreno procedural
HEIGHTMAP_LOAD_TEXTURE(id, file)	Asocia textura de color
HEIGHTMAP_UNLOAD(id)	Libera recursos
Renderizado
Función	Descripción
HEIGHTMAP_RENDER_3D(id, w, h)	Renderizado CPU (320 columnas)
HEIGHTMAP_RENDER_3D_GPU(id, w, h)	Renderizado GPU acelerado
HEIGHTMAP_SET_RENDER_DISTANCE(d)	Distancia máxima de dibujado
HEIGHTMAP_SET_CHUNK_CONFIG(size, r)	Configuración de chunks
Control de Cámara
Función	Descripción
HEIGHTMAP_SET_CAMERA(x,y,z,angle,pitch,fov)	Posiciona cámara manualmente
HEIGHTMAP_INIT_CAMERA_ON_TERRAIN(id)	Inicializa sobre terreno
HEIGHTMAP_MOVE_FORWARD_WITH_COLLISION(speed, id)	Avanza con colisión
HEIGHTMAP_SET_CAMERA_FOLLOW(sprite_id, ox,oy,oz, style)	Seguimiento automático
## Efectos Ambientales

## Agua

HEIGHTMAP_SET_WATER_LEVEL(20);  
HEIGHTMAP_SET_WATER_TEXTURE("water.png", 30);  
HEIGHTMAP_SET_WAVE_AMPLITUDE(20.0);  
HEIGHTMAP_UPDATE_WATER_TIME(); // Llamar cada frame


## Cielo y Niebla

HEIGHTMAP_SET_SKY_COLOR(135, 206, 235, 255);  
HEIGHTMAP_SET_SKY_TEXTURE("skybox.png", 1000);  
HEIGHTMAP_SET_FOG_COLOR(255, 255, 255, 200);


## Billboards (Sprites 3D)

// Billboard estático (árboles, rocas)  
HEIGHTMAP_ADD_VOXEL_BILLBOARD(x, y, 10.0, tree_graph, 1.0);  
  
// Billboard dinámico (jugador, enemigos)  
billboard_id = HEIGHTMAP_REGISTER_BILLBOARD(id, x, y, z, graph, layer);  
HEIGHTMAP_UPDATE_BILLBOARD(id, new_x, new_y, new_z);  
HEIGHTMAP_UNREGISTER_BILLBOARD(id);


## Colisiones

// Obtener altura del terreno  
height = HEIGHTMAP_GET_HEIGHT(id, x, y) / 1000.0;  
  
// Verificar colisión  
if (HEIGHTMAP_CHECK_TERRAIN_COLLISION(radius))  
    // Hay colisión  
end  
  
// Validar movimiento de sprite  
if (HEIGHTMAP_CAN_SPRITE_MOVE_TO(x, y, z, radius))  
    // Movimiento válido  
end

### 🎯 Características Técnicas

    Resolución de renderizado: 320x240 píxeles (escalable)
    Interpolación bilineal para consultas de altura suaves 
    Depth buffer para oclusión correcta de billboards
    Sistema de chunks para culling eficiente 
    Shaders embebidos para renderizado GPU 
    Caché de alturas en punto flotante para rendimiento óptimo l

### 📖 Documentación Completa

Para documentación detallada de todas las funciones, consulta:

    README.md - Lista completa de funciones
    test.prg - Aplicación de demostración completa
    Wiki del proyecto

### 🎮 Aplicación de Demostración

El archivo test.prg incluye un ejemplo completo con :

    Nave controlable con WASD
    Sistema de enemigos
    Disparos y combate
    Cámara con seguimiento automático
    Árboles distribuidos en grid
    Efectos de agua animada
    Toggle CPU/GPU con tecla G

# Ejecutar demo  
bgdi test.dcb

### 🔧 Configuración Avanzada
Optimización de Rendimiento

// Reducir distancia de renderizado  
HEIGHTMAP_SET_RENDER_DISTANCE(1000);  
  
// Ajustar tamaño de chunks  
HEIGHTMAP_SET_CHUNK_CONFIG(128, 5);  
  
// Usar renderizado GPU  
graph = HEIGHTMAP_RENDER_3D_GPU(id, 320, 240);

## Sistema de Coordenadas

    X, Y: Coordenadas del mundo (0 a ancho/alto del heightmap)
    Z: Altura (0-255 desde heightmap, extensible)
    Ángulos: Multiplicados por 1000 (360° = 360000) 

### 🤝 Contribuciones

Las contribuciones son bienvenidas. Por favor:

    Fork el proyecto
    Crea una rama para tu feature (git checkout -b feature/AmazingFeature)
    Commit tus cambios (git commit -m 'Add some AmazingFeature')
    Push a la rama (git push origin feature/AmazingFeature)
    Abre un Pull Request

📝 Licencia

Copyright (C) 2025 - Heightmap Module for BennuGD2

### 👤 Autor

Rufidj

    GitHub: @Rufidj

### 🙏 Agradecimientos

    Comunidad de BennuGD2
    Inspirado en técnicas de voxelspace clásicas
    SDL2 y OpenGL por las capacidades gráficas


  
## Notas  
  
Este README incluye: 
  
- Descripción clara del módulo y sus capacidades principales  
- Capturas de pantalla y videos de demostración existentes  
- Guía de inicio rápido con ejemplo funcional  
- Referencia completa de la API organizada por categorías  
- Especificaciones técnicas del sistema  
- Instrucciones de instalación y uso  
- Documentación de la aplicación de demostración  
- Sección de contribuciones y licencia  
  

