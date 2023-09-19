build_example(
  NAME wns3-mpquic-two-path
  SOURCE_FILES wns3-mpquic-two-path.cc
  LIBRARIES_TO_LINK
    ${libquic}
)

build_example(
  NAME wns3-mpquic-one-path
  SOURCE_FILES wns3-mpquic-one-path.cc
  LIBRARIES_TO_LINK
    ${libquic}
)


