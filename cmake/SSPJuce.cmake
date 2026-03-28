include_guard(GLOBAL)

include(FetchContent)

set(SSP_JUCE_VERSION "7.0.12")
set(SSP_JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout")

set(_ssp_juce_candidates)

if(SSP_JUCE_PATH)
    list(APPEND _ssp_juce_candidates "${SSP_JUCE_PATH}")
endif()

list(APPEND _ssp_juce_candidates
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/JUCE"
    "${CMAKE_CURRENT_SOURCE_DIR}/../SSP-Multichain/build/_deps/juce-src"
)

set(_ssp_juce_source_dir "")

foreach(_ssp_candidate IN LISTS _ssp_juce_candidates)
    if(_ssp_candidate AND EXISTS "${_ssp_candidate}/CMakeLists.txt")
        set(_ssp_juce_source_dir "${_ssp_candidate}")
        break()
    endif()
endforeach()

if(_ssp_juce_source_dir)
    message(STATUS "Using local JUCE from ${_ssp_juce_source_dir}")
    FetchContent_Declare(
        juce
        SOURCE_DIR "${_ssp_juce_source_dir}"
    )
else()
    message(STATUS "Fetching JUCE ${SSP_JUCE_VERSION} from GitHub")
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${SSP_JUCE_VERSION}
    )
endif()

FetchContent_MakeAvailable(juce)
