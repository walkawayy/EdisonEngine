if( TARGET glm::glm )
    return()
endif()

set( _glm_version 0.9.9.8 )

include( ./dl_unpack )

dl_unpack(
        URL https://github.com/g-truc/glm/archive/refs/tags/${_glm_version}.tar.gz
        FILENAME glm-${_glm_version}.tar.gz
        TEST_DIR glm-${_glm_version}
)

add_library( glm::glm INTERFACE IMPORTED )
target_include_directories( glm::glm SYSTEM BEFORE INTERFACE "${EXTERNAL_SRC_ROOT}/glm-${_glm_version}" )
