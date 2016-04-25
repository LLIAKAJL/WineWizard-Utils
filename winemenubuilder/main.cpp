#include <gdiplus.h>
#include <shlobj.h>
#include <Shlwapi.h>
#include <stdio.h>
#include <string>
#include <wincrypt.h>
#include <windows.h>

std::wstring toANSI(const std::string &str)
{
    if(str.empty())
        return std::wstring();
    auto size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], static_cast<int>(str.size()), nullptr, 0);
    std::wstring strTo(size_needed, 0);
    MultiByteToWideChar(CP_ACP, 0, &str[0], static_cast<int>(str.size()), &strTo[0], size_needed);
    return strTo;
}

std::string toUTF8(const std::wstring &wstr)
{
    if(wstr.empty())
        return std::string();
    auto size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

std::wstring toUnix(const std::wstring &str)
{
    auto res = str;
    size_t posn = 0;
    while (std::wstring::npos != (posn = res.find(L"\\", posn)))
    {
        res.replace(posn, 1, L"/");
        posn += 1;
    }
    if (res.length() >= 2 && res.substr(0, 2) == L"C:")
        res.replace(0, 2, L"c:");
    return res;
}

std::string baseName(const std::string &path)
{
    auto res = path.substr(path.find_last_of("/\\") + 1);
    return res.substr(0, res.find_last_of('.'));
}

void getEncoderClsid(LPCWSTR format, CLSID *pClsid)
{
    auto num = 0U;
    auto size = 0U;
    Gdiplus::GetImageEncodersSize(&num, &size);
    auto pImageCodecInfo = new Gdiplus::ImageCodecInfo[size];
    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    for (auto j = 0U; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
	    *pClsid = pImageCodecInfo[j].Clsid;
	    break;
        }
    }
    delete [] pImageCodecInfo;
}

const DWORD SHA1_HASH_LEN = 20;

bool sha1(LPCTSTR lpszPassword, LPTSTR lpszHash)
{
    HCRYPTPROV hCryptProv;
    HCRYPTHASH hCryptHash;
    BYTE       bHashValue[SHA1_HASH_LEN];
    auto dwSize = SHA1_HASH_LEN;
    auto bSuccess = false;
    if(CryptAcquireContext(&hCryptProv, nullptr, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if(CryptCreateHash(hCryptProv, CALG_SHA1, 0, 0, &hCryptHash))
        {
            if(CryptHashData(hCryptHash, (PBYTE)lpszPassword, lstrlen(lpszPassword) * sizeof(TCHAR), 0))
            {
                DWORD dwCount;
                if(CryptGetHashParam(hCryptHash, HP_HASHVAL, bHashValue, &dwSize, 0))
                {
                    for(dwCount = 0, *lpszHash = 0; dwCount < SHA1_HASH_LEN; ++dwCount)
                        wsprintf(lpszHash + (dwCount * 2), TEXT("%02x"), bHashValue[dwCount]);
                    bSuccess = TRUE;
                }
            }
            CryptDestroyHash(hCryptHash);
        }
        CryptReleaseContext(hCryptProv, 0);
    }
    return bSuccess;
}

bool fileExists(const std::string &path)
{
    auto dwAttrib = GetFileAttributes(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void createLink(const std::string &name, const std::string &appPath, const std::string &workDir, const std::string &args = std::string())
{
    CoInitialize(nullptr);
    IShellLink *sLink;
    CoCreateInstance(CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER, IID_IShellLink, reinterpret_cast<LPVOID *>(&sLink));
    sLink->SetPath(appPath.c_str());
    sLink->SetWorkingDirectory(workDir.c_str());
    sLink->SetArguments(args.c_str());
    sLink->SetIconLocation(appPath.c_str(), 0);
    IPersistFile *pf;
    sLink->QueryInterface(IID_IPersistFile, reinterpret_cast<LPVOID *>(&pf));
    WCHAR path[MAX_PATH] = {0};
    CreateDirectory("y:\\.links", nullptr);
    std::string buf = std::string("y:\\.links\\") + name + ".lnk";
    MultiByteToWideChar(CP_ACP, 0, buf.c_str(), -1, path, MAX_PATH);
    pf->Save(path, TRUE);
    sLink->Release();
    CoUninitialize();
}

void createShortcut(const std::string &linkPath)
{
    CoInitialize(nullptr);
    IShellLink *psl;
    CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, reinterpret_cast<LPVOID *>(&psl));
    IPersistFile *ppf; 
    psl->QueryInterface(IID_IPersistFile, reinterpret_cast<LPVOID *>(&ppf));
    ppf->Load(toANSI(linkPath).c_str(), STGM_READ);
    int index;
    CHAR path[MAX_PATH] = { 0 };
    psl->GetPath(path, MAX_PATH, nullptr, SLGP_RAWPATH);
    if (std::string(CharUpper(PathFindExtension(path))) == ".EXE")
    {
        auto name = baseName(linkPath);
        auto fileName = name;
        if (linkPath.substr(0, 2) != "y:")
        {
            CHAR hash[MAX_PATH] = { 0 };
            sha1(name.c_str(), hash);
            fileName = hash;
        }
        CreateDirectory("y:\\.icons", nullptr);
        CreateDirectory("y:\\.shortcuts", nullptr);
        CHAR iconLoc[MAX_PATH] = { 0 };
        psl->GetIconLocation(iconLoc, MAX_PATH, &index);
        if (fileExists(path) || fileExists(iconLoc))
        {
            Gdiplus::GdiplusStartupInput gdiplusStartupInput;
            ULONG_PTR gdiplusToken;
            Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
            CLSID encoderClsid;
            getEncoderClsid(L"image/png", &encoderClsid);
            auto icon = ExtractIcon(GetModuleHandle(nullptr), (fileExists(iconLoc)) ? iconLoc : path, static_cast<UINT>(index));
            auto bitmap = Gdiplus::Bitmap::FromHICON(icon);
            bitmap->Save(toANSI(std::string("y:\\.icons\\") + fileName).c_str(), &encoderClsid, nullptr);
            DestroyIcon(icon);
            delete bitmap;
            Gdiplus::GdiplusShutdown(gdiplusToken);
        }
        auto *f = fopen((std::string("y:\\.shortcuts\\") + fileName).c_str(), "w");
        fprintf(f, "[General]\nExe=%s\nName=%s\n", toUTF8(toUnix(toANSI(linkPath))).c_str(), toUTF8(toANSI(name)).c_str());
        fclose(f);
    }
    ppf->Release();
    psl->Release();
    CoUninitialize();    
}

int main(int argc, char **argv)
{
    switch (argc)
    {
    case 5:
        createLink(argv[1], argv[2], argv[3], argv[4]);
        break;
    case 4:
        createLink(argv[1], argv[2], argv[3]);
        break;
    case 3:
        if (std::string(argv[1]) == "-w")
            createShortcut(argv[2]);
    }
    return 0;
}
