set(KIT_NAME "kit_video_manage")
file(GLOB SRC_CURRENT ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
message("---------- Building ${KIT_NAME}  ----------")
add_library(${KIT_NAME} OBJECT ${SRC_CURRENT})

target_sources(${KIT_NAME} PUBLIC 
    $<TARGET_OBJECTS:kit_encode_video_operate>
)    

target_include_directories(${KIT_NAME} PUBLIC  
    ${CMAKE_CURRENT_SOURCE_DIR} 
)       


target_link_directories(${KIT_NAME} PUBLIC ${FFMPEG_LIBRARY_DIRS})

target_link_libraries(${KIT_NAME} PUBLIC
    shared_dependencies
    kit_log
    kit_encode_video_operate
)

message("---------- judge platform  ----------")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
   AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    target_sources(${KIT_NAME} PUBLIC 
        $<TARGET_OBJECTS:kit_jeston_media>
    )    
    target_link_libraries(${KIT_NAME} PUBLIC
        kit_jeston_media
    )
    # set(ARM64 TRUE)
    message("ARM kit_jeston_media")
# elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm|armv[0-9]")
#     set(ARM32 TRUE)
#     message(STATUS "ARM 32-bit")
endif()