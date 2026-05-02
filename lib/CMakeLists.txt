
# Generate a pkg-config file for cog-zero
configure_file(cog-zero.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/cog-zero.pc
    @ONLY
)

INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cog-zero.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
