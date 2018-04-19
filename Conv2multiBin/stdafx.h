// stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、または
// 参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
// を記述します。
//

#pragma once

#pragma warning(disable:4710 4711 4514)
#pragma warning(push)
#pragma warning(disable:4191 4365 4619 4820 4987 5039)
#define _CRT_NON_CONFORMING_SWPRINTFS
#include "targetver.h"

#include <stdio.h>
#include <tchar.h>


#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // 一部の CString コンストラクターは明示的です。

#include <atlbase.h>
#include <atlstr.h>

// TODO: プログラムに必要な追加ヘッダーをここで参照してください。
#include <xmllite.h>
#pragma comment(lib, "xmllite.lib")
#pragma warning(pop)
