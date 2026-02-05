# CPack Source Package Configuration
# Replicates `make rpmtar` and `make tar` functionality

set(CPACK_PACKAGE_NAME "sems")
set(CPACK_PACKAGE_VERSION "${FILE_VERSION}")
set(CPACK_PACKAGE_VENDOR "SEMS Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SIP Express Media Server")

# Source package settings
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")

# Match exclusions from Makefile rpmtar target
set(CPACK_SOURCE_IGNORE_FILES
    "/tmp/"
    "/build/"
    "\\\\.svn"
    "\\\\.git"
    "\\\\.#"
    "\\\\.[do]$"
    "\\\\.la$"
    "\\\\.lo$"
    "\\\\.so$"
    "\\\\.il$"
    "\\\\.gz$"
    "\\\\.bz2$"
    "\\\\.tar$"
    "~$"
    "/CMakeFiles/"
    "CMakeCache\\\\.txt$"
    "cmake_install\\\\.cmake$"
    "CPackConfig\\\\.cmake$"
    "CPackSourceConfig\\\\.cmake$"
    "_CPack_Packages"
)

include(CPack)

# Custom target: rpmtar - creates tarball in ~/rpmbuild/SOURCES/
add_custom_target(rpmtar
    COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/rpmbuild/SOURCES"
    COMMAND ${CMAKE_CPACK_COMMAND} --config ${CMAKE_BINARY_DIR}/CPackSourceConfig.cmake
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_BINARY_DIR}/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.tar.gz"
        "$ENV{HOME}/rpmbuild/SOURCES/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.tar.gz"
    COMMAND ${CMAKE_COMMAND} -E echo "Created: ~/rpmbuild/SOURCES/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.tar.gz"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Creating RPM source tarball in ~/rpmbuild/SOURCES/"
)
