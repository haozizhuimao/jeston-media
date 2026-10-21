set(KIT_NAME "kit_jeston_media")

if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")))
    message(STATUS "[${KIT_NAME}] skip Jetson media target on non-Jetson platform: ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
    return()
endif()

file(GLOB SRC_CURRENT ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
message("---------- Building ${KIT_NAME}  ----------")
add_library(${KIT_NAME} OBJECT ${SRC_CURRENT})

target_include_directories(${KIT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${THIRD_PARTY_INCLUDE}/ffmpeg
)

if(TARGET kit_jeston_multimedia_api)
    target_sources(${KIT_NAME} PRIVATE
        $<TARGET_OBJECTS:kit_jeston_multimedia_api>
    )

    target_link_libraries(${KIT_NAME} PUBLIC
        shared_dependencies
        kit_log
        kit_jeston_multimedia_api
    )
else()
    message(WARNING "[${KIT_NAME}] kit_jeston_multimedia_api target not found, skipping linked Jetson objects")
endif()
