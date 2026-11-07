# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

INCLUDE(FetchContent)

SET(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
SET(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FETCHCONTENT_DECLARE(glm
                     GIT_REPOSITORY https://github.com/g-truc/glm.git
                     GIT_TAG 1.0.3
                     SYSTEM
)
FETCHCONTENT_MAKEAVAILABLE(glm)
