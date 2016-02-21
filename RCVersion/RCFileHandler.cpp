#include "stdafx.h"
#include "RCFileHandler.h"
#include "RCUpdater.h"
#include "AutoHClose.h"
#include "AutoFree.h"

RCFileHandler::RCFileHandler(ILogger &rlogger)
   : ilogger(rlogger)
   , logger(rlogger)
   , verbose(false)
   , linesLogged(0)
   , linesErrors(0)
{
}

RCFileHandler::~RCFileHandler()
{
}

bool RCFileHandler::UpdateFile(const wchar_t *inpath, const wchar_t *outpath, int major, int minor, int build, int revision)
{
   logger.Log(L"UpdateFile(%s,%s)", NN(inpath), NN(outpath));

   void* buffer = nullptr;
   size_t bytes = 0;

   if (!LoadFile(inpath, 1024, buffer, bytes))
      return false;

   AutoFree af(buffer);

   int flags = IS_TEXT_UNICODE_UNICODE_MASK;
   bool isUnicode = 0 != IsTextUnicode(buffer, int(min(bytes, 256)), &flags);

   unsigned changes;
   if (isUnicode)
      changes = UpdateBuffer(static_cast<wchar_t*>(buffer), bytes / sizeof(wchar_t), major, minor, build, revision);
   else
      changes = UpdateBuffer(static_cast<char*>(buffer), bytes / sizeof(char), major, minor, build, revision);

   if (0 == changes)
   {
      logger.Log(L"No changes made to [%s], file [%s] not modified.", NN(inpath), NN(outpath));
      return false;
   }

   logger.Log(L"%u changes made to [%s], writing file [%s].", changes, NN(inpath), NN(outpath));

   return true;
}

unsigned RCFileHandler::RCFileHandler::UpdateBuffer(char* buffer, size_t chars, int major, int minor, int build, int revision)
{
   logger.Log(L"UpdateBuffer<char>(...,%u)", unsigned(chars));
   RCUpdater<char> updater(ilogger);
   unsigned changes = updater.UpdateVersion(buffer, chars, major, minor, build, revision);
   return changes;
}

unsigned RCFileHandler::RCFileHandler::UpdateBuffer(wchar_t* buffer, size_t chars, int major, int minor, int build, int revision)
{
   logger.Log(L"UpdateBuffer<wchar>(...,%u)", unsigned(chars));
   RCUpdater<wchar_t> updater(ilogger);
   unsigned changes = updater.UpdateVersion(buffer, chars, major, minor, build, revision);
   return changes;
}

// ---------------------------------------------------------------------------
// 
// ---------------------------------------------------------------------------
bool RCFileHandler::LoadFile(const wchar_t* path, size_t padding, void* &buffer, size_t &bytes)
{
   if (!path || !*path)
      return logger.Error(ERROR_INVALID_PARAMETER, L"*** RCFileUpdater::Load: Input file path must not be empty");

   HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
   if (INVALID_HANDLE_VALUE == hFile)
      return logger.Error(GetLastError(), L"*** RCFileUpdater::Load: Cannot open input file", path);

   AutoHClose ahc(hFile);

   ULARGE_INTEGER li = { 0 };
   li.LowPart = GetFileSize(hFile, &li.HighPart);
   if (0 != li.HighPart || (0x7FFFFFFF - padding) <= li.LowPart)
      return logger.Error(ERROR_FILE_CORRUPT, L"*** RCFileUpdater::Load: File too large", path);

   if (padding < 2)
      padding = 2;

   bytes = li.LowPart + padding;
   buffer = malloc(bytes);
   if (!buffer)
      return logger.Error(ERROR_OUTOFMEMORY, L"*** RCFileUpdater::Load: File too large", path);

   DWORD readBytes = 0;
   BOOL ok = ReadFile(hFile, buffer, DWORD(bytes), &readBytes, nullptr);
   if (!ok || li.LowPart < readBytes)
   {
      free(buffer);
      buffer = nullptr;
      bytes = 0;
      return logger.Error(GetLastError(), L"*** RCFileUpdater::Load: Cannot read input file", path);
   }

   *(0 + readBytes + static_cast<char*>(buffer)) = 0;
   *(1 + readBytes + static_cast<char*>(buffer)) = 0;
   return true;
}
