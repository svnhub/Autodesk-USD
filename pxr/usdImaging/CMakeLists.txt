set(DIRS
    usdImaging
    usdImagingGL
    usdRiImaging
    usdSkelImaging
    usdVolImaging
    usdAppUtils
    usdviewq
    bin
    plugin
    hdEmscripten
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()
