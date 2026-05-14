# aria2-next helper functions

get_version() {
  VERSION=$(sed -n 's/^  VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | head -n 1)
  echo "Version: $VERSION"
}
