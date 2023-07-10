set(DIRS
    usdImaging
    usdImagingGL
    usdProcImaging
    usdRiImaging
    usdSkelImaging
    usdVolImaging
    usdAppUtils
    usdviewq
    plugin
    hdEmscripten
    bin
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()
