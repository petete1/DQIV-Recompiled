# Writes OUTPUT, a source file defining pc::version(): the git description of
# SOURCE ("1.8.1" on a tag, "1.8.1-7-g82bc2eb" after it, "-dirty" with local
# changes; "unknown" without git). Run at every build; the file only changes
# with the version.
execute_process(COMMAND git describe --tags --always --dirty WORKING_DIRECTORY "${SOURCE}"
    OUTPUT_VARIABLE version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT version)
    set(version unknown)
endif()
file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT "#include \"Version.hpp\"\nconst char* pc::version() {return \"@version@\";}\n" @ONLY)
