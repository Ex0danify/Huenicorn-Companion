set(CPACK_PACKAGE_NAME huenicorn-companion)
set(CPACK_PACKAGE_VENDOR Ex0danify)
set(CPACK_PACKAGE_CONTACT "Ex0danify <ex0danify@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "System tray controller and configurator for the Huenicorn daemon")
set(CPACK_PACKAGE_HOMEPAGE_URL https://github.com/Ex0danify/Huenicorn-Companion)
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE.txt)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_STRIP_FILES ON)

# Formats are picked with `cpack -G`
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-linux-${CMAKE_SYSTEM_PROCESSOR}")

set(CPACK_RPM_PACKAGE_LICENSE GPLv3)
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
# Quick is loaded from QML at runtime, so rpm cannot detect it
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtdeclarative")
set(CPACK_RPM_PACKAGE_SUGGESTS "huenicorn")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtquick, qml6-module-qtquick-window, qml6-module-qtqml-workerscript")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS huenicorn)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)
