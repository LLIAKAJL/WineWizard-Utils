#include <windows.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <stdio.h>
#include <wincrypt.h>

const DWORD SHA1_HASH_LEN = 20;

void sha1(LPCSTR data, LPSTR hash)
{
    HCRYPTPROV hCryptProv;
    HCRYPTHASH hCryptHash;
    BYTE       bHashValue[SHA1_HASH_LEN];
    DWORD dwSize = SHA1_HASH_LEN;
    if(CryptAcquireContext(&hCryptProv, nullptr, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if(CryptCreateHash(hCryptProv, CALG_SHA1, 0, 0, &hCryptHash))
        {
            if(CryptHashData(hCryptHash, (PBYTE)data, strlen(data) * sizeof(CHAR), 0))
            {
                *hash = 0;
                if(CryptGetHashParam(hCryptHash, HP_HASHVAL, bHashValue, &dwSize, 0))
                    for(DWORD dwCount = 0; dwCount < SHA1_HASH_LEN; ++dwCount)
                        wsprintf(hash + (dwCount * 2), "%02x", bHashValue[dwCount]);
            }
            CryptDestroyHash(hCryptHash);
        }
        CryptReleaseContext(hCryptProv, 0);
    }
}

void getEncoderClsid(LPCWSTR format, CLSID *pClsid)
{
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    Gdiplus::ImageCodecInfo *pImageCodecInfo = new Gdiplus::ImageCodecInfo[size];
    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            break;
        }
    }
    delete [] pImageCodecInfo;
}

void replaceSpec(LPSTR str)
{
    CHAR tmp[MAX_PATH] = { 0 };
    for (int i = 0, count = strlen(str), j = 0; i < count; ++i)
        if (str[i] == '\\')
        {
            tmp[j++] = '\\';
            tmp[j++] = '\\';
        }
        else if (str[i] == '"')
        {
            tmp[j++] = '\\';
            tmp[j++] = '"';
        }
        else
            tmp[j++] = str[i];
    strcpy(str, tmp);
}

int main(int argc, char **argv)
{
    if (argc <= 2 || strcmp(argv[1], "-w") != 0)
        return 0;
    CoInitialize(nullptr);
    IShellLink *psl;
    CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, reinterpret_cast<LPVOID *>(&psl));
    IPersistFile *ppf; 
    psl->QueryInterface(IID_IPersistFile, reinterpret_cast<LPVOID *>(&ppf));
    LPWSTR *lpArgv = CommandLineToArgvW(GetCommandLineW(), &argc);
    ppf->Load(lpArgv[2], STGM_READ);
    CHAR path[MAX_PATH] = { 0 };
    psl->GetPath(path, MAX_PATH, nullptr, SLGP_RAWPATH);
    int index;
    CHAR iconLocation[MAX_PATH] = { 0 };
    psl->GetIconLocation(iconLocation, MAX_PATH, &index);
    CHAR arguments[MAX_PATH] = { 0 };
    psl->GetArguments(arguments, MAX_PATH);
    replaceSpec(arguments);
    CHAR workDir[MAX_PATH] = { 0 };
    psl->GetWorkingDirectory(workDir, MAX_PATH);
    replaceSpec(workDir);
    ppf->Release();
    psl->Release();
    CoUninitialize();
    CreateDirectory("y:\\.shortcuts", nullptr);
    CHAR outPath[MAX_PATH] = { 0 };
    strcpy(outPath, "y:\\.shortcuts\\");
//    CHAR mbFileName[MAX_PATH] = { 0 };
//    WideCharToMultiByte(CP_UTF8, 0, lpArgv[2], -1, mbFileName, MAX_PATH, nullptr, nullptr);
    HICON icon = ExtractIcon(GetModuleHandle(nullptr), (iconLocation[0] == '\0') ? path : iconLocation, index);
    replaceSpec(path);
    CHAR fileName[_MAX_FNAME] = { 0 };
    _splitpath(argv[2], nullptr, nullptr, fileName, nullptr);
    CHAR hash[MAX_PATH] = { 0 };
    sha1(fileName, hash);
    FILE *f = fopen(strcat(outPath, hash), "wb");
    WCHAR temp[MAX_PATH] = { 0 };
    CHAR tempC[MAX_PATH] = { 0 };
    MultiByteToWideChar(CP_ACP, 0, arguments, MAX_PATH, temp, MAX_PATH);
    WideCharToMultiByte(CP_UTF8, 0, temp, -1, tempC, MAX_PATH, nullptr, nullptr);
    fprintf(f, "[General]\nArgs=\"%s\"\n", tempC);
    MultiByteToWideChar(CP_ACP, 0, path, MAX_PATH, temp, MAX_PATH);
    WideCharToMultiByte(CP_UTF8, 0, temp, -1, tempC, MAX_PATH, nullptr, nullptr);
    fprintf(f, "Exe=\"%s\"\n", tempC);
    MultiByteToWideChar(CP_ACP, 0, fileName, MAX_PATH, temp, MAX_PATH);
    WideCharToMultiByte(CP_UTF8, 0, temp, -1, tempC, MAX_PATH, nullptr, nullptr);
    fprintf(f, "Name=\"%s\"\n", tempC);
    MultiByteToWideChar(CP_ACP, 0, workDir, MAX_PATH, temp, MAX_PATH);
    WideCharToMultiByte(CP_UTF8, 0, temp, -1, tempC, MAX_PATH, nullptr, nullptr);
    fprintf(f, "WorkDir=\"%s\"", tempC);
    fclose(f);
    if (icon != 0)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
        CLSID encoderClsid;
        getEncoderClsid(L"image/png", &encoderClsid);
        Gdiplus::Bitmap *bitmap = Gdiplus::Bitmap::FromHICON(icon);
        CreateDirectory("y:\\.icons", nullptr);
        WCHAR outIconPath[MAX_PATH] = { 0 };
        wcscpy(outIconPath, L"y:\\.icons\\");
        WCHAR wFileName[MAX_PATH] = { 0 };
        MultiByteToWideChar(CP_ACP, 0, fileName, -1, wFileName, MAX_PATH);
        WCHAR wHash[MAX_PATH] = { 0 };
        MultiByteToWideChar(CP_ACP, 0, hash, -1, wHash, MAX_PATH);
        bitmap->Save(wcscat(outIconPath, wHash), &encoderClsid, nullptr);
        DestroyIcon(icon);
        delete bitmap;
        Gdiplus::GdiplusShutdown(gdiplusToken);        
    }
    LocalFree(lpArgv);
    return 0;
}
