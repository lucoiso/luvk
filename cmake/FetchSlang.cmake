# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

# Slang is consumed as prebuilt release binaries (see FetchSlangHelper.cmake), so none of the SLANG_*
# from-source build options apply here.
INCLUDE(FetchSlangHelper.cmake)
FETCH_SLANG(VERSION 2026.20 COMPONENTS slang)
