set(CMAKE_BUILD_TYPE Debug) 
add_definitions("-Wall -g")
add_executable( Shadows Shadows.c )
target_link_libraries( Shadows Common )
