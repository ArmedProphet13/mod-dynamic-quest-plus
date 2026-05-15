#
# DynamicQuests+ Module
# A living world interaction engine for AzerothCore
#

# Install conf file to the appropriate directory
if (WIN32)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/conf/mod_dynamicquests.conf.dist
        DESTINATION ${CMAKE_INSTALL_PREFIX}/configs/modules)
else()
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/conf/mod_dynamicquests.conf.dist
        DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/modules)
endif()

# Install SQL base files alongside the binary for reference
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/sql/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/sql/mod-dynamic-quest-plus
    FILES_MATCHING PATTERN "*.sql")
