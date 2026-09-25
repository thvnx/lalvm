# LALVM's runtimes. `babbage` is the Ada runtime and is not built yet.
# `gnatstub` is a temporary substitute for the GNAT entry points.
add_subdirectory(gnatstub)

# Babbage's Ada sources, laid out like GNAT's adainclude/adalib pair.
install(DIRECTORY babbage/adainclude/
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/lalvm/babbage/adainclude
        FILES_MATCHING PATTERN "*.ad[sb]")
install(FILES babbage/babbage.gpr
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/lalvm/babbage)
# TODO: Once Babbage is compiled, install its archive next to the sources:
# install(TARGETS babbage
#         ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/lalvm/babbage/adalib)
