// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#ifndef LUVKMODULE_H
#define LUVKMODULE_H

#ifdef BUILD_DLL
#define LUVKMODULE_API _declspec(dllexport)
#else
#define LUVKMODULE_API _declspec(dllimport)
#endif
#endif