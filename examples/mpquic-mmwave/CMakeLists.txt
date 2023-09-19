build_example(
  NAME mpquic-two-plmn
  SOURCE_FILES mpquic-two-plmn.cc
  LIBRARIES_TO_LINK
    ${libquic}
    ${libmmwave}
)