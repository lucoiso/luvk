// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#ifndef LUVKMODULE_H
#define LUVKMODULE_H

#ifdef __GNUC__
#define LUVKMODULE_API __attribute__((visibility("default")))
#else
#ifdef BUILD_DLL
#define LUVKMODULE_API _declspec(dllexport)
#else
#define LUVKMODULE_API _declspec(dllimport)
#endif // BUILD_DLL
#endif // __GNUC__
#endif // LUVKMODULE_H
