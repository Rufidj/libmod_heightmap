project(mod_heightmap)    
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../bin/")    
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../bin/")    
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin/")    
    
set(CMAKE_MODULE_PATH ../../cmake/Modules)    
  
# Establecer política para OpenGL  
cmake_policy(SET CMP0072 NEW)  
set(OpenGL_GL_PREFERENCE GLVND)  
  
# Buscar paquetes necesarios  
find_package(SDL2 REQUIRED)    
find_package(OpenGL REQUIRED)  
  
# Buscar GLEW usando pkg-config  
find_package(PkgConfig REQUIRED)  
pkg_check_modules(GLEW REQUIRED glew)  
  
if(USE_SDL2_GPU)    
    find_package(SDL_GPU REQUIRED)    
endif()    
    
add_definitions(-D__LIBMOD_HEIGHTMAP ${EXTRA_CFLAGS})    
    
include_directories(${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS}     
                   ../../core/include     
                   ../../core/bgdrtm     
                   ../../modules/libbggfx     
                   ../../modules/libmod_gfx    
                   ${SDL_GPU_INCLUDE_DIR}  
                   ${OPENGL_INCLUDE_DIR}  
                   ${GLEW_INCLUDE_DIRS}  
                   ${INCLUDE_DIRECTORIES})    
    
file(GLOB SOURCES_LIBMOD_HEIGHTMAP    
     "../../modules/libmod_heightmap/*.c"    
     )    
    
add_library(mod_heightmap ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMOD_HEIGHTMAP})    
    
target_link_libraries(mod_heightmap   
    ${SDL2_LIBRARY}   
    ${SDL2_LIBRARIES}   
    ${SDL_GPU_LIBRARY}   
    ${OPENGL_LIBRARIES}  
    ${GLEW_LIBRARIES}  
    -L../../bin   
    bgdrtm   
    bggfx   
    mod_gfx   
    ${STDLIBSFLAGS}   
    -lm)