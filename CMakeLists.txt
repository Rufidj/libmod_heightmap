project(mod_heightmap)  
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../bin/")  
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../bin/")  
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin/")  
  
set(CMAKE_MODULE_PATH ../../cmake/Modules)  
  
find_package(SDL2 REQUIRED)  
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
                   ${INCLUDE_DIRECTORIES})  
  
file(GLOB SOURCES_LIBMOD_HEIGHTMAP  
     "../../modules/libmod_heightmap/*.c"  
     )  
  
add_library(mod_heightmap ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMOD_HEIGHTMAP})  
  
# AGREGAR ESTA LÍNEA - enlazar con mod_gfx  
target_link_libraries(mod_heightmap ${SDL2_LIBRARY} ${SDL2_LIBRARIES} ${SDL_GPU_LIBRARY} -L../../bin bgdrtm bggfx mod_gfx ${STDLIBSFLAGS} -lm)