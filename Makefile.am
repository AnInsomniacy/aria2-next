SUBDIRS = po lib third_party src doc test

ACLOCAL_AMFLAGS = -I m4 --install

EXTRA_DIST = config.rpath \
	     tools/script-helper \
	     tools/import-po \
	     tools/build_test.sh \
	     packaging/scripts/makerelease \
	     packaging/scripts/mingw-config packaging/scripts/mingw-release packaging/scripts/mingw-build-memo \
	     packaging/scripts/android-config packaging/scripts/android-release \
	     packaging/README.md \
	     packaging/docs/ANDROID.md packaging/docs/WINDOWS.md \
	     packaging/macos/makerelease-osx.mk \
	     packaging/macos/welcome.html \
	     packaging/macos/DS_Store packaging/macos/etc/paths.d/aria2c packaging/macos/etc/manpaths.d/aria2 \
	     packaging/docker/Dockerfile.mingw \
	     packaging/docker/Dockerfile.android \
	     packaging/legacy/Dockerfile.raspberrypi \
	     packaging/legacy/README.md \
	     packaging/legacy/travis.yml \
	     packaging/android/generic-config packaging/android/openssl-config packaging/android/zlib-config \
	     maintenance/README.md maintenance/open-issue-review.md maintenance/issue-review-matrix.csv \
	     maintenance/modernization-progress.md \
	     third_party/README.md \
	     tools/README.md \
	     doc/requirements.txt \
	     examples/libaria2ex.cc examples/libaria2wx.cc

dist_doc_DATA = README.md

dist_noinst_DATA = LICENSE.OpenSSL

.PHONY: clang-format

# Format source files using clang-format.  Don't format source files
# under third_party directory since we are not responsible for their coding
# style.
clang-format:
	CLANGFORMAT=`git config --get clangformat.binary`; \
	test -z $${CLANGFORMAT} && CLANGFORMAT="clang-format"; \
	$${CLANGFORMAT} -i $(top_srcdir)/src/*.{c,cc,h} $(top_srcdir)/src/includes/aria2/*.h \
	$(top_srcdir)/examples/*.cc $(top_srcdir)/test/*.{cc,h}
