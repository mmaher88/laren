add_custom_target(copy-addon DEPENDS laren-addon.conf.in-fmt)
add_custom_command(TARGET copy-addon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_BINARY_DIR}/src/laren-addon.conf
        ${CMAKE_CURRENT_BINARY_DIR}/laren.conf)
