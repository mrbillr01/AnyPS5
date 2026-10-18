# libjpeg-turbo's spng object target needs the external zlib headers before
# FindZLIB creates its imported target later in the parent configuration.
include_directories(SYSTEM "${ZLIB_INCLUDE_DIR}")
