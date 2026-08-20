# Generate a pkg-config file for cog-zero
include(GNUInstallDirs)

configure_file(cog-zero.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/cog-zero.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cog-zero.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT OpenCog
)

# Install per-module pkg-config templates when present at repo root
foreach(_pc agentzero-communication agentzero-knowledge agentzero-learning
            agentzero-memory agentzero-tools)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${_pc}.pc.in")
        configure_file(
            "${CMAKE_SOURCE_DIR}/${_pc}.pc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/${_pc}.pc"
            @ONLY
        )
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${_pc}.pc"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
            COMPONENT OpenCog
        )
    endif()
endforeach()
