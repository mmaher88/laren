# Util library (no external deps)
add_library(laren_util STATIC
    util/unicode.cpp
    util/mmap_file.cpp
)
target_include_directories(laren_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Dictionary library
add_library(laren_dict STATIC
    dict/dictionary.cpp
    dict/trie.cpp
)
target_link_libraries(laren_dict PUBLIC laren_util)

# Core transliteration library
add_library(laren_core STATIC
    core/transliterator.cpp
    core/rule_engine.cpp
    core/ranker.cpp
    core/emoji_map.cpp
    core/word_history.cpp
)
target_link_libraries(laren_core PUBLIC laren_dict)

# Fcitx5 engine plugin (shared library)
add_library(laren MODULE
    engine/laren_engine.cpp
    engine/laren_state.cpp
)
target_link_libraries(laren PRIVATE laren_core Fcitx5::Core)
set_target_properties(laren PROPERTIES PREFIX "" NO_SONAME ON INSTALL_RPATH "")

# Install targets
install(TARGETS laren DESTINATION "${FCITX_INSTALL_ADDONDIR}")

install(FILES "${CMAKE_SOURCE_DIR}/resources/laren-addon.conf.in"
        RENAME laren.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

install(FILES "${CMAKE_SOURCE_DIR}/resources/laren-im.conf"
        RENAME laren.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")

install(FILES "${CMAKE_SOURCE_DIR}/data/dictionary.tsv"
              "${CMAKE_SOURCE_DIR}/data/emoji.tsv"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/laren")

install(FILES "${CMAKE_SOURCE_DIR}/resources/default-profile"
        RENAME profile
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/default")

install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/fcitx-laren.svg"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")

foreach(SIZE 16 22 24 32 48 128)
    install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/${SIZE}x${SIZE}/fcitx-laren.png"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/apps")
endforeach()
