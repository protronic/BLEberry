# Build-time script: writes the current git hash of the app repository
# into OUT as BLEBERRY_GIT_HASH. Only touches the file when the value
# changes, so incremental builds stay incremental.
execute_process(
  COMMAND git -C ${SOURCE_DIR} rev-parse --short=8 HEAD
  OUTPUT_VARIABLE git_hash
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT git_hash)
  set(git_hash "unknown")
endif()

execute_process(
  COMMAND git -C ${SOURCE_DIR} status --porcelain --untracked-files=no
  OUTPUT_VARIABLE git_dirty
  ERROR_QUIET
)
if(git_dirty)
  string(APPEND git_hash "-dirty")
endif()

set(content "#define BLEBERRY_GIT_HASH \"${git_hash}\"\n")

if(EXISTS ${OUT})
  file(READ ${OUT} old_content)
else()
  set(old_content "")
endif()

if(NOT content STREQUAL old_content)
  file(WRITE ${OUT} ${content})
endif()
