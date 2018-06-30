// ConvertToRedumpOrgFormat.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

/**
 * Copyright 2011-2018 sarami
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "stdafx.h"
// http://www.accuraterip.com/driveoffsets.htm
#define MAX_DRIVE_PLUS_OFFSET 1776
#define MAX_DRIVE_MINUS_OFFSET 1164
// no Evidence
#define MAX_WRITE_OFFSET 4000

#define MAXIMUM_NUMBER_TRACKS 100
typedef struct _GAMEDATA {
	_TCHAR Track[MAXIMUM_NUMBER_TRACKS][_MAX_FNAME];
	DWORD Size[MAXIMUM_NUMBER_TRACKS];
	DWORD Crc[MAXIMUM_NUMBER_TRACKS];
	DWORD TrackNum;
} GAMEDATA, *PGAMEDATA;

typedef enum _EXEC_TYPE {
	split
} EXEC_TYPE, *PEXEC_TYPE;

#define OutputString(str, ...) \
{ \
	_tprintf(str, __VA_ARGS__); \
}
#ifdef _DEBUG
_TCHAR logBuffer[2048];
#define OutputErrorString(str, ...) \
{ \
	_sntprintf(logBuffer, 2048, str, __VA_ARGS__); \
	logBuffer[2047] = 0; \
	OutputDebugString(logBuffer); \
}
#else
#define OutputErrorString(str, ...) \
{ \
	_ftprintf(stderr, str, __VA_ARGS__); \
}
#endif

#define FcloseAndNull(fp) \
{ \
	if (fp) { \
		fclose(fp); \
		fp = NULL; \
	} \
}

#define FreeAndNull(buf) \
{ \
	if (buf) { \
		free(buf); \
		buf = NULL; \
	} \
}

static LONG s_writeOfs = 0;
static LONG s_driveOfs = 0;

VOID OutputLastErrorNumAndString(
	LPCTSTR pszFuncName,
	LONG lLineNum
	)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

	OutputErrorString(_T("[F:%s][L:%ld] GetLastError: %lu, %s\n"),
		pszFuncName, lLineNum, GetLastError(), (LPCTSTR)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

static unsigned int table[256] = {0};
void InitCRC32()
{
	unsigned int poly = 0xEDB88320, u, i, j;
	for (i = 0; i < 256; i++) {
		u = i;
		for (j = 0; j < 8; j++) {
			if (u & 0x1) {
				u = (u >> 1) ^ poly;
			}
			else {
				u >>= 1;
			}
		}
		table[i] = u;
	}
}

HRESULT ReadDatfile(
	_TCHAR* datfilename,
	_TCHAR* pszGamename, 
	PGAMEDATA gamedata
	)
{
	HRESULT hr = S_OK;
	CComPtr<IXmlReader> pReader;
	if (FAILED(hr = CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(&pReader), 0))) {
		OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
		return hr;
	}
	CComPtr<IStream> pStream;
	if (FAILED(hr = SHCreateStreamOnFile(datfilename, STGM_READ, &pStream))) {
		OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
		return hr;
	}
	if (FAILED(hr = pReader->SetInput(pStream))) {
		OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
		return hr;
	}
	if (FAILED(hr = pReader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Parse))) {
		OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
		return hr;
	}
	XmlNodeType nodeType;
	bool bEnd = false;
	bool bGameName = false;
	bool bRomName = false;
	while (S_OK == pReader->Read(&nodeType) && !bEnd) {
		switch (nodeType) {
		case XmlNodeType_Element:
			LPCTSTR pwszLocalName;
			if (FAILED(hr = pReader->GetLocalName(&pwszLocalName, NULL))) {
				OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
				return hr;
			}
			else if (!pwszLocalName) {
				OutputErrorString(_T("Error GetLocalName, NULL, L:%d\n"), __LINE__);
				return S_FALSE;
			}
			if (!_tcscmp(pwszLocalName, _T("game"))) {
				if (S_FALSE == pReader->MoveToFirstAttribute()) {
					break;
				}
				do {
					LPCTSTR pwszAttributeName;
					LPCTSTR pwszAttributeValue;
					if (FAILED(hr = pReader->GetLocalName(&pwszAttributeName, NULL))) {
						OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
						return hr;
					}
					else if (!pwszAttributeName) {
						OutputErrorString(_T("Error GetLocalName, NULL, L:%d\n"), __LINE__);
						return S_FALSE;
					}
					if (!_tcscmp(pwszAttributeName, _T("name"))) {
						if (FAILED(hr = pReader->GetValue(&pwszAttributeValue, NULL))) {
							OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
							return hr;
						}
						else if (!pwszAttributeValue) {
							OutputErrorString(_T("Error GetValue, NULL, L:%d\n"), __LINE__);
							return S_FALSE;
						}
						if (!_tcscmp(pwszAttributeValue, pszGamename)) {
							bGameName = true;
							break;
						}
					}
				} while (S_OK == pReader->MoveToNextAttribute());
			}
			else if (!_tcscmp(pwszLocalName, _T("rom")) && bGameName) {
				hr = pReader->MoveToFirstAttribute();
				if (S_FALSE == pReader->MoveToFirstAttribute()) {
					break;
				}
				do {
					LPCTSTR pwszAttributeName;
					LPCTSTR pwszAttributeValue;
					if (FAILED(hr = pReader->GetLocalName(&pwszAttributeName, NULL))) {
						OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
						return hr;
					}
					else if (!pwszAttributeName) {
						OutputErrorString(_T("Error GetLocalName, NULL, L:%d\n"), __LINE__);
						return S_FALSE;
					}
					if (!_tcscmp(pwszAttributeName, _T("name"))) {
						if (FAILED(hr = pReader->GetValue(&pwszAttributeValue, NULL))) {
							OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
							return hr;
						}
						else if (!pwszAttributeValue) {
							OutputErrorString(_T("Error GetValue, NULL, L:%d\n"), __LINE__);
							return S_FALSE;
						}
						_tcscpy(gamedata->Track[gamedata->TrackNum], pwszAttributeValue);
					}
					else if (!_tcscmp(pwszAttributeName, _T("size"))) {
						if (FAILED(hr = pReader->GetValue(&pwszAttributeValue, NULL))) {
							OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
							return hr;
						}
						else if (!pwszAttributeValue) {
							OutputErrorString(_T("Error GetValue, NULL, L:%d\n"), __LINE__);
							return S_FALSE;
						}
						gamedata->Size[gamedata->TrackNum] = _tcstoul(pwszAttributeValue, NULL, 10);
					}
					else if (!_tcscmp(pwszAttributeName, _T("crc"))) {
						if (FAILED(hr = pReader->GetValue(&pwszAttributeValue, NULL))) {
							OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
							return hr;
						}
						else if (!pwszAttributeValue) {
							OutputErrorString(_T("Error GetValue, NULL, L:%d\n"), __LINE__);
							return S_FALSE;
						}
						gamedata->Crc[gamedata->TrackNum] = _tcstoul(pwszAttributeValue, NULL, 16);
					}
				} while (S_OK == pReader->MoveToNextAttribute());
				gamedata->TrackNum++;
				bRomName = true;
			}
			break;
		case XmlNodeType_EndElement:
			if (bGameName && bRomName) {
				bEnd = true;
			}
			break;
		case XmlNodeType_None:
		case XmlNodeType_Attribute:
		case XmlNodeType_Text:
		case XmlNodeType_CDATA:
		case XmlNodeType_ProcessingInstruction:
		case XmlNodeType_Comment:
		case XmlNodeType_DocumentType:
		case XmlNodeType_Whitespace:
		case XmlNodeType_XmlDeclaration:
		default:
			break;
		}
	}
	if (bGameName && bRomName) {
		OutputString(_T("Match: [%s]\n"), pszGamename);
	}
	else {
		OutputErrorString(_T("Unmatch: [%s]\n")
			_T("Please input game name correctly\n")
			_T("Example\n")
			_T("\tLunar 2 - Eternal Blue (Japan) (Disc 1)\n")
			_T("\tJ. League Pro Soccer Club wo Tsukurou! 2 (Japan) (3M)\n"),
			pszGamename);
		hr = S_FALSE;
	}
	return hr;
}

inline DWORD GetFilesize(
	FILE *fp
	)
{
	DWORD filesize = 0;
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		filesize = (DWORD)ftell(fp);
		rewind(fp);
	}

	return filesize;
}

inline DWORD CalcCrc32(
	const PGAMEDATA gamedata,
	LPBYTE data,
	LONG idx,
	DWORD i
	)
{
	DWORD crc32 = 0xFFFFFFFF;
	DWORD tmp = (DWORD)idx;
	for (DWORD cnt = 0; cnt < gamedata->Size[i]; tmp++, cnt++) {
		crc32 = (crc32 >> 8) ^ table[data[tmp] ^ (crc32 & 0xFF)];
	}
	crc32 ^= 0xFFFFFFFF;
	return crc32;
}

inline bool IsValidDataHeader(
	const LPBYTE src
	)
{
	BYTE aHeader[] = { 
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
	};
	bool bRet = true;
	for (int c = 0; c < sizeof(aHeader); c++) {
		if (src[c] != aHeader[c]) {
			bRet = false;
			break;
		}
	}
	return bRet;
}

int GetEccEdcWriteCmd(
	LPTSTR pszCmd,
	size_t cmdSize,
	LPCTSTR pszOutPath,
	DWORD minute,
	DWORD second,
	DWORD frame,
	DWORD mode,
	DWORD count
	)
{
	_TCHAR path[_MAX_PATH] = { 0 };
	if (!::GetModuleFileName(NULL, path, _MAX_PATH)) {
		OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
		return FALSE;
	}
	BOOL bRet = FALSE;
	_TCHAR drive[_MAX_DRIVE] = { 0 };
	_TCHAR dir[_MAX_DIR] = { 0 };
	_tsplitpath(path, drive, dir, NULL, NULL);
	_tmakepath(path, drive, dir, _T("EccEdc"), _T("exe"));
	if (PathFileExists(path)) {
		_sntprintf(pszCmd, cmdSize, _T("\"\"%s\" write \"%s\" %lu %lu %lu %lu %lu\""),
			path, pszOutPath, minute, second, frame, mode, count);
		bRet = TRUE;
	}
	return bRet;
}

DWORD HexToDec(
	BYTE src
	)
{
	return (DWORD)((src >> 4) * 10 + (src & 0x0f));
}

VOID OutputHeader(
	LPBYTE headerData
	)
{
	OutputErrorString(
		_T("\tHeader: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n"),
		headerData[0], headerData[1], headerData[2], headerData[3],
		headerData[4], headerData[5], headerData[6], headerData[7],
		headerData[8], headerData[9], headerData[10], headerData[11],
		headerData[12], headerData[13], headerData[14], headerData[15]);
}

int Parsefile(
	_TCHAR* pszInfile,
	PGAMEDATA gamedata,
	DWORD mode
)
{
	FILE* fp = _tfopen(pszInfile, _T("rb"));
	if (!fp) {
		OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
		return FALSE;
	}
	_TCHAR fileNew[_MAX_PATH] = { 0 };
	_tcsncpy(fileNew, pszInfile, _MAX_PATH);
	fileNew[_MAX_PATH - 1] = 0;
	_tcsncat(fileNew, _T("_new"), 4);
	DWORD filesizeOnArg = GetFilesize(fp);
	DWORD filesizeOnDat = 0;
	for (DWORD i = 1; i < gamedata->TrackNum; i++) {
		// i == 0 is .cue file
		OutputString(_T("rom name: %s, size: %ld\n"), gamedata->Track[i], gamedata->Size[i]);
		filesizeOnDat += gamedata->Size[i];
	}
	BOOL bRet = TRUE;
	BYTE headerData[16] = { 0 };
	LPBYTE TrackData = NULL;
	try {
		if (filesizeOnDat != filesizeOnArg) {
			OutputString(
				_T("Different file size. %ld, on datfile: %ld => %ld\n"),
				filesizeOnArg, filesizeOnDat, filesizeOnArg - filesizeOnDat);
			DWORD pregapSize = filesizeOnDat - filesizeOnArg;

			// 00:02:00 - 00:05:00
			if (pregapSize == 176400 || pregapSize == 352800 || pregapSize == 529200 ||
				pregapSize == 882000) {
				OutputString(_T("Insert pregap...\n"));
				TrackData = (LPBYTE)calloc(filesizeOnArg, sizeof(BYTE));
				if (!TrackData) {
					OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
					throw FALSE;
				}
				fread(TrackData, sizeof(BYTE), filesizeOnArg, fp);

				FILE* fpNew = _tfopen(fileNew, _T("wb"));
				if (!fpNew) {
					OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
					throw FALSE;
				}
				size_t writeSize = fwrite(TrackData, sizeof(BYTE), gamedata->Size[1], fpNew);

				if (pregapSize == 176400 || pregapSize == 352800) {
					for (DWORD i = 0; i < pregapSize; i++) {
						fputc(0, fpNew);
					}
					writeSize += fwrite(TrackData + gamedata->Size[1],
						sizeof(BYTE), filesizeOnArg - gamedata->Size[1], fpNew);
				}
				else if (pregapSize == 529200 || pregapSize == 882000) {
					fseek(fp, (LONG)gamedata->Size[1] - 176400, SEEK_SET);
					BYTE headerDataPrev[16] = { 0 };
					fread(headerDataPrev, sizeof(BYTE), sizeof(headerDataPrev), fp);

					fseek(fp, (LONG)gamedata->Size[1], SEEK_SET);
					fread(headerData, sizeof(BYTE), sizeof(headerData), fp);

					fseek(fp, (LONG)gamedata->Size[1] + 176400, SEEK_SET);
					BYTE headerDataNext[16] = { 0 };
					fread(headerDataNext, sizeof(BYTE), sizeof(headerDataNext), fp);

					if (IsValidDataHeader(headerDataPrev) && 
						IsValidDataHeader(headerData) &&
						IsValidDataHeader(headerDataNext)) {
						OutputHeader(headerDataPrev);
						OutputHeader(headerData);
						OutputHeader(headerDataNext);
						_TCHAR cmdBuf[_MAX_PATH] = { 0 };
						DWORD minute = HexToDec(headerData[0xc]);
						DWORD second = HexToDec(headerData[0xd]);
						DWORD frame = HexToDec(headerData[0xe]);
						DWORD second2 = HexToDec(headerDataNext[0xd]);

						DWORD missedSector = pregapSize / 2352 / 75;
						DWORD secondAdd = missedSector - (second2 - second);

						DWORD additionalSector = 0;
						if (missedSector) {
							additionalSector = pregapSize / missedSector;
							writeSize += fwrite(TrackData + gamedata->Size[1],
								sizeof(BYTE), additionalSector, fpNew);
						}
						second += secondAdd;

						if (!GetEccEdcWriteCmd(cmdBuf, _MAX_PATH,
							fileNew, minute, second, frame, mode, 75)) {
							OutputErrorString(_T("cmd err: %s\n"), cmdBuf);
							throw FALSE;
						}
						FcloseAndNull(fpNew);
						OutputString(_T("exec: %s\n"), cmdBuf);
						_tsystem(cmdBuf);
						second++;
						if (!GetEccEdcWriteCmd(cmdBuf, _MAX_PATH,
							fileNew, minute, second, frame, 2, 150)) {
							OutputErrorString(_T("cmd err: %s\n"), cmdBuf);
							throw FALSE;
						}
						OutputString(_T("exec: %s\n"), cmdBuf);
						_tsystem(cmdBuf);
						if (pregapSize == 529200) {
							writeSize += fwrite(TrackData + gamedata->Size[1],
								sizeof(BYTE), filesizeOnArg - gamedata->Size[1], fpNew);
						}
						else if (pregapSize == 882000) {
							fpNew = _tfopen(fileNew, _T("ab"));
							if (!fpNew) {
								OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
								throw FALSE;
							}
							writeSize += fwrite(TrackData + gamedata->Size[1] + additionalSector,
								sizeof(BYTE), gamedata->Size[2] - additionalSector, fpNew);
							for (DWORD i = 0; i < 352800; i++) {
								fputc(0, fpNew);
							}
							fwrite(TrackData + writeSize,
								sizeof(BYTE), filesizeOnArg - writeSize, fpNew);
						}
					}
					else {
						for (DWORD i = 0; i < pregapSize; i++) {
							fputc(0, fpNew);
						}
					}
				}
				FcloseAndNull(fp);
				FcloseAndNull(fpNew);
				FreeAndNull(TrackData);
			}
			else {
				throw FALSE;
			}
		}
		if (!fp) {
			fp = _tfopen(fileNew, _T("rb"));
			if (!fp) {
				OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
				throw FALSE;
			}
		}
		DWORD crc32 = 0;
		_TCHAR drive[_MAX_DRIVE] = { 0 };
		_TCHAR dir[_MAX_DIR] = { 0 };
		_TCHAR binfile[_MAX_PATH] = { 0 };
		_tsplitpath(pszInfile, drive, dir, NULL, NULL);

		DWORD nOffset = 0;
		DWORD nCombinedOffset =
			(MAX_DRIVE_PLUS_OFFSET + MAX_DRIVE_MINUS_OFFSET + MAX_WRITE_OFFSET) * 4;
		DWORD nZerobyte = MAX_DRIVE_MINUS_OFFSET * 4;

		LONG idx = (s_driveOfs + MAX_DRIVE_MINUS_OFFSET + s_writeOfs) * 4;
		LONG roopCnt = (s_driveOfs + s_writeOfs) * 4;
		LONG maxCnt = (MAX_DRIVE_PLUS_OFFSET + s_writeOfs) * 4;
		bool bSucceeded = false;
		bool bFailed = false;
		bool bEnd = false;
		InitCRC32();

		for (DWORD i = 1; i < gamedata->TrackNum; i++) {
			fread(headerData, sizeof(BYTE), sizeof(headerData), fp);
			fseek(fp, -16, SEEK_CUR);
			OutputString(_T("Splitting Track %ld data\n"), i);

			if (IsValidDataHeader(headerData)) {
				// Data Track
				TrackData = (LPBYTE)malloc(gamedata->Size[i]);
				if (!TrackData) {
					OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
					throw FALSE;
				}
				fread(TrackData, sizeof(BYTE), gamedata->Size[i], fp);
				crc32 = CalcCrc32(gamedata, TrackData, 0, i);
				if (gamedata->Crc[i] != crc32) {
					OutputErrorString(
						_T("Crc32 is unmatched\n")
						_T("\tSplit data: 0x%lx, dat: 0x%lx\n")
						_T("\tFile size: %ld(0x%lx)\n"),
						crc32, gamedata->Crc[i], gamedata->Size[i], gamedata->Size[i]);
					if (i > 1) {
						OutputHeader(headerData);
					}
				}
				else {
					_tmakepath(binfile, drive, dir, gamedata->Track[i], NULL);
					FILE* fp2 = _tfopen(binfile, _T("wb"));
					if (!fp2) {
						OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
						throw FALSE;
					}
					fwrite(TrackData, sizeof(BYTE), gamedata->Size[i], fp2);
					FcloseAndNull(fp2);
				}
				FreeAndNull(TrackData);
			}
			else {
				// Audio Track
				if (roopCnt == maxCnt) {
					break;
				}
				DWORD nDatasize = gamedata->Size[i] + nCombinedOffset;
				TrackData = (LPBYTE)calloc(nDatasize, sizeof(BYTE));
				if (!TrackData) {
					OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
					throw FALSE;
				}
				fseek(fp, (LONG)nOffset, SEEK_SET);
				fread(TrackData + nZerobyte,
					sizeof(BYTE), nDatasize - nZerobyte, fp);
				for (;;) {
					crc32 = CalcCrc32(gamedata, TrackData, idx, i);
					if (gamedata->Crc[i] == crc32) {
						if (!bSucceeded) {
							OutputString(
								_T("\nThis image was ripped by a drive of %ld offsets\n"),
								roopCnt / 4 - s_writeOfs);
						}
						_tmakepath(binfile, drive, dir, gamedata->Track[i], NULL);
						FILE* fp3 = _tfopen(binfile, _T("wb"));
						if (!fp3) {
							OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
							throw FALSE;
						}
						fwrite(TrackData + idx, sizeof(BYTE), gamedata->Size[i], fp3);
						FcloseAndNull(fp3);
						FreeAndNull(TrackData);
						bSucceeded = true;
						break;
					}
					else if (bSucceeded && i > 2) {
						OutputErrorString(
							_T("\tCrc32 is unmatched. 0x%lx, on datfile: 0x%lx\n"),
							crc32, gamedata->Crc[i]);
						if (i == gamedata->TrackNum - 1) {
							OutputErrorString(
								_T("\tPlease re-rip by a drive that can read a lead-out\n"));
						}
						FreeAndNull(TrackData);
						bFailed = true;
						break;
					}
					OutputString(
						_T("\rAnalizing file: ofs %5ld/%5ld"), roopCnt - 1, maxCnt - 1);
					if (roopCnt == maxCnt) {
						bEnd = true;
						break;
					}
					else {
						roopCnt++;
					}
					idx++;
				}

				if (bEnd) {
					OutputString(_T("\n"));
					break;
				}
			}
			nOffset += gamedata->Size[i];
		}
		if (bSucceeded && !bFailed) {
			OutputString(_T("Succeeded\n"));
		}
		else if (bSucceeded && bFailed) {
			OutputErrorString(_T("Some of tracks are corrupt or alt version\n"));
			bRet = FALSE;
		}
		else {
			OutputErrorString(_T("Failed. Image is corrupt or alt version\n"));
			bRet = FALSE;
		}
	}
	catch (BOOL bErr) {
		bRet = bErr;
		FreeAndNull(TrackData);
	}
	FcloseAndNull(fp);
	return bRet;
}

DWORD GetModeFromDat(
	LPCTSTR pszDat
	)
{
	DWORD mode = 0;
	_TCHAR fname[_MAX_FNAME] = { 0 };
	_tsplitpath(pszDat, NULL, NULL, fname, NULL);
	if (!_tcsncmp(fname, _T("Sega - Saturn"), 13)) {
		mode = 1;
	}
	else if (!_tcsncmp(fname, _T("Sony - PlayStation"), 18)) {
		mode = 3;
	}
	else {
		mode = 1;
	}
	return mode;
}

int execSplit(_TCHAR* argv[])
{
	int nRet = TRUE;
	PGAMEDATA pGamedata = (PGAMEDATA)calloc(sizeof(GAMEDATA), sizeof(UCHAR));
	if (!pGamedata) {
		OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
		nRet = FALSE;
	}
	else {
		nRet = ReadDatfile(argv[1], argv[2], pGamedata);
	}
	if (nRet == 0) {
		nRet = Parsefile(argv[3], pGamedata, GetModeFromDat(argv[1]));
	}
	FreeAndNull(pGamedata);
	return nRet;
}

void printUsage(void)
{
	OutputString(
		_T("Usage\n")
		_T("\t<datfile> <filename> <parsefilename> <write offset(sample)> <drive offset(sample)>\n")
		_T("\t\tfilename: \"game name\" tagname written in datfile\n")
		_T("\t\tparsefilename: img(clonecd) or bin(isobuster)\n")
		_T("\t\twrite offset(sample): see redump.org\n")
		_T("\t\tdrive offset(sample): offset of drive which created img or bin. If don't know it, input -1164\n"));
}

int checkArg(int argc, _TCHAR* argv[], PEXEC_TYPE pExecType)
{
	int ret = TRUE;
	_TCHAR* endptr = NULL;
	if (argc == 6) {
		s_writeOfs = _tcstol(argv[4], &endptr, 10);
		if (*endptr) {
			OutputErrorString(_T("Invalid argument: %s Please input integer\n"), endptr);
			return FALSE;
		}
		s_driveOfs = _tcstol(argv[5], &endptr, 10);
		if (*endptr) {
			OutputErrorString(_T("Invalid argument: %s Please input integer\n"), endptr);
			return FALSE;
		}
		if (s_writeOfs + s_driveOfs + MAX_DRIVE_MINUS_OFFSET < 0) {
			OutputErrorString(
				_T("Offsets are too small (%ld). Write offsets + drive offsets > -1165\n"), s_writeOfs + s_driveOfs)
				return FALSE;
		}
		*pExecType = split;
	}
	else {
		OutputErrorString(_T("argc: %d\n"), argc);
		ret = FALSE;
	}
	return ret;
}

int printSeveralInfo()
{
	OutputString(_T("AppVersion\n"));
#ifdef _WIN64
	OutputString(_T("\tx64, "));
#else
	OutputString(_T("\tx86, "));
#endif
#ifdef UNICODE
	OutputString(_T("UnicodeBuild, "));
#else
	OutputString(_T("AnsiBuild, "));
#endif
	OutputString(
		_T("%s %s\n"), _T(__DATE__), _T(__TIME__));
	return TRUE;
}

int _tmain(int argc, _TCHAR* argv[])
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
#ifdef UNICODE
	if (_setmode(_fileno(stdin), _O_U8TEXT) == -1) {
		OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
		return EXIT_FAILURE;
	}
	if (_setmode(_fileno(stdout), _O_U8TEXT) == -1) {
		OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
		return EXIT_FAILURE;
	}
	if (_setmode(_fileno(stderr), _O_U8TEXT) == -1) {
		OutputLastErrorNumAndString(_T(__FUNCTION__), __LINE__);
		return EXIT_FAILURE;
	}
#endif
	int nRet = EXIT_SUCCESS;
	printSeveralInfo();
	EXEC_TYPE execType;
	if (!checkArg(argc, argv, &execType)) {
		printUsage();
		return EXIT_FAILURE;
	}
	if (execType == split) {
		nRet = execSplit(argv);
	}
	return nRet == TRUE ? EXIT_SUCCESS : EXIT_FAILURE;
}

