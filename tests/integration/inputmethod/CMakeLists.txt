add_custom_target(copy-im DEPENDS laren-im.conf.in-fmt)
add_custom_command(TARGET copy-im POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_BINARY_DIR}/src/laren-im.conf
        ${CMAKE_CURRENT_BINARY_DIR}/laren.conf)
