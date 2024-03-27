find_package(PkgConfig)

if (PKG_CONFIG_FOUND)
	pkg_check_modules(X264  x264)
	message(WARNING "find x264 --- debug --- ")
endif()

