#include <winsock2.h> //IP_ADAPTER_ADDRESSES for GetAdaptersAddresses
#include <Windows.h> //GetComputerName
#include <iphlpapi.h> //GetAdaptersAddresses
#define SECURITY_WIN32 //GetUserNameExW
#include <Security.h> //GetUserNameExW

#include <map>
#include <array>
#include <memory>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cwctype>
#include "PcParam.h"

#pragma comment(lib, "Secur32.lib") //GetUserNameExW
#pragma comment(lib, "IPHLPAPI.lib") //GetAdaptersAddresses

namespace
{
   inline std::wstring MakeErrorMsg(std::wstring const& name)
   {
      std::wstring result(PcParam::ErrorMsg);
      result += std::wstring(L" <");
      result += name;
      result += std::wstring(L">");
      return result;
   }

   template<typename T>
   inline size_t GetArrayLength(T const& arr)
   {
      return sizeof(arr[0]) * arr.size();
   };

   inline bool GetDataFromHklm(std::wstring const& key, std::wstring const& value, LPVOID data, DWORD size)
   {
      HKEY h;
      if (ERROR_SUCCESS != ::RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_WOW64_64KEY | KEY_READ, &h))
         return false;

      DWORD size_param = size;
      ::RegQueryValueExW(h, value.c_str(), NULL, NULL, reinterpret_cast<LPBYTE>(data), &size_param);
      ::RegCloseKey(h);

      return true;
   }

   std::wstring GetStringFromHklm(std::wstring const& key, std::wstring const& value)
   {
      std::array<wchar_t, 128> data;
      data.fill(wchar_t(0));
      GetDataFromHklm(key, value, data.data(), sizeof(data));
      return std::wstring(data.data());
   }

   DWORD GetDwordFromHklm(std::wstring const& key, std::wstring const& value)
   {
      DWORD result;
      GetDataFromHklm(key, value, &result, sizeof(result));
      return result;
   }

   inline std::wstring GetMacInString(const IP_ADAPTER_ADDRESSES *adapter)
   {
      std::wstringstream str;

      for (size_t i = 0; i < adapter->PhysicalAddressLength; ++i)
      {
         if (i > 0)
            str << L"-";
         str << std::hex << std::setfill(L'0') << std::setw(2) << adapter->PhysicalAddress[i];
      }

      return str.str();
   }
}

namespace PcParam
{
   ComputerName::ComputerName():
      Base(MakeErrorMsg(L"Имя компьютера"))
   {
      std::array<wchar_t, MAX_PATH> name;
      DWORD length = name.size();
      if (::GetComputerNameW(name.data(), &length) == FALSE)
         return;

      result = name.data();
   }

   UserName::UserName():
      Base(MakeErrorMsg(L"Имя пользователя"))
   {
      std::array<wchar_t, 64> name;
      DWORD length = name.size();
      if (::GetUserNameW(name.data(), &length) == FALSE)
         return;

      result = name.data();
   }

   UserDisplayName::UserDisplayName():
      Base(L"")
   {
      std::array<wchar_t, 128> name;
      DWORD length = name.size();
      if (::GetUserNameExW(EXTENDED_NAME_FORMAT::NameDisplay, name.data(), &length) == FALSE)
         return;

      result = name.data();
   }

   OsVersion::OsVersion():
      Base(MakeErrorMsg(L"версия ОС"))
   {
      OSVERSIONINFOEX ver = {0};
      ver.dwOSVersionInfoSize = sizeof(ver);
      //получение версии Windows
      //Для корректной работы необходимо наличия манифеста с указанием поддержки Windows 10.
      if (::GetVersionExW(reinterpret_cast<LPOSVERSIONINFO>(&ver)) == FALSE)
         return;

      auto product = GetStringFromHklm(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName");

      //ver.wServicePackMajor и ver.wServicePackMinor также могут дать информацию о SP
      //Начиная с Vista, можно получить информацию о версии GetProductInfo, а также из поля
      //ver.wSuiteMask
      //Подобная информация расположена в HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion"
      if (ver.dwMajorVersion == 5)//для XP смотрим значение полей структуры
         product += ver.wSuiteMask & VER_SUITE_PERSONAL ? L" Home" : L" Professional";

      BOOL iswow64;
      ::IsWow64Process(::GetCurrentProcess(), &iswow64); //определение битности

      std::wstringstream str;
      str << product << L" (" << ver.dwMajorVersion << L"." << ver.dwMinorVersion << L") "
          << (iswow64 ? L"x64" : L"x86");
      if (ver.wServicePackMajor > 0)
         str << L" SP " << ver.wServicePackMajor;
      str << L", build " << ver.dwBuildNumber;

      result = str.str();
   }

   MachineGuid::MachineGuid():
      Base(MakeErrorMsg(L"идентификатор установки"))
   {
      auto guid= GetStringFromHklm(L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid");
      if (guid.empty())
         return;
      result = guid;
   }

   HardwareString::HardwareString():
      Base(MakeErrorMsg(L"Характеристики системы"))
   {
      auto proc = GetStringFromHklm(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\", L"ProcessorNameString");
      if (proc.empty())
         return;

      auto freq = GetDwordFromHklm(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\", L"~MHz");
      if (freq == 0)
         return;

      //Определение версии ОС для того, чтобы выбрать способ получения объема оперативной памяти
      OSVERSIONINFOEX ver = {0};
      ver.dwOSVersionInfoSize = sizeof(ver);
      if (::GetVersionExW(reinterpret_cast<LPOSVERSIONINFO>(&ver)) == FALSE)
         return;

      double memory(0);
      //Для Vista SP1 и выше используем функцию GetPhysicallyInstalledSystemMemory
      if (ver.dwMajorVersion > 6 || (ver.dwMajorVersion == 6 && ver.wServicePackMajor >= 1))
      {
         HMODULE h = ::GetModuleHandleW(L"kernel32.dll");
         auto func = reinterpret_cast<BOOL (WINAPI *)(PULONGLONG)>(::GetProcAddress(h, "GetPhysicallyInstalledSystemMemory"));
         ULONGLONG total = 0;
         func(&total); //обойти ошибку
         total /= 1024;
         memory = static_cast<double>(total);
      }
      else //для остальных версий используем получение размера памяти средствами WMI
      {
         try
         {
            //Эту функцию будем использовать только тут, значит, и прототип можно объявить здесь:
            size_t GetMemoryFromWmi();
            memory = static_cast<double>(GetMemoryFromWmi());
         }
         catch(...)
         {
         }
      }
      
      //Если размер памяти получить не удалось, то используем менее точный способ:
      if (0 == memory)
      {
         MEMORYSTATUSEX statex;
         statex.dwLength = sizeof (statex);
         ::GlobalMemoryStatusEx (&statex);
         memory = static_cast<double>(statex.ullTotalPhys) / 1024.0 / 1024.0;
      }

      std::wstringstream str;
      str << proc << L" (" << freq << L" MHz" << L"); ";
      str << std::setprecision(2) << std::fixed << memory << L" Mb";
      result = str.str();

      //Удаление лишних пробелов из результирующей строки
      auto new_end = std::unique(result.begin(), result.end(), [](wchar_t lhs, wchar_t rhs) -> bool
                                 {
                                    return (lhs == rhs) && (lhs == L' ');
                                 });
      result.erase(new_end, result.end());   
   }

   MacAddresses::MacAddresses():
      Base(Macs())
   {
      std::array<IP_ADAPTER_ADDRESSES, 1024> data;
      ULONG size = data.size() * sizeof(data[0]);

      //Получение данных об адаптерах
      if (NO_ERROR != ::GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, data.data(), &size))
         return;

      //Определение версии ОС для того, чтобы выбрать способ определения wi-fi адаптеров
      OSVERSIONINFO ver = {0};
      ver.dwOSVersionInfoSize = sizeof(ver);
      if (::GetVersionExW(&ver) == FALSE)
         return;

      for (auto current = data.data(); current; current = current->Next)
      {
         bool is_wifi = IF_TYPE_IEEE80211 == current->IfType;
         //Для Windows XP is_wifi всегда будет false, используем другой способ:
         if (ver.dwMajorVersion < 6)
         {
            try
            {
               bool IsAdapterWifi(PCHAR name);
               is_wifi = IsAdapterWifi(current->AdapterName);
            }
            catch(...)
            {
            }
         }

         //Отсеивание не wi-fi и ethernet адаптреов
         if (IF_TYPE_ETHERNET_CSMACD != current->IfType && !is_wifi)
            continue;

         //Отсеивание виртуальных bluetooth адаптеров
         const std::wstring fields[] = {current->FriendlyName, current->Description};
         static const std::wstring keywords[] = {L"virtual", L"bluetooth", L"виртуальный"};
         //Поиск ключевых слов в массиве fields
         auto word = std::find_first_of(std::begin(fields), std::end(fields),
                                        std::begin(keywords), std::end(keywords),
                                        [] (std::wstring const& f, std::wstring const& k) -> bool
                        {
                           //Сравнение в нижнем регистре
                           return std::search(f.begin(), f.end(), k.begin(), k.end(),
                                              [] (wchar_t fc, wchar_t kc)
                              {
                                 return std::towlower(fc) == kc;
                              }) != f.end();
                        });

         //Если ключевые слова обнаружены, то пропускаем адаптер
         if (word != std::end(fields))
            continue;

         //Добавление описания mac-адреса
         Mac entry;
         entry.Address = GetMacInString(current);
         entry.Name = current->FriendlyName;
         entry.Desc = current->Description;
         entry.IsWifi = is_wifi;

         result.emplace_back(entry);
      }

      std::sort(result.begin(), result.end(), [](Macs::const_reference f, Macs::const_reference s)
         {
            return f.IsWifi && !s.IsWifi;
         });
   }

   std::wstring Get::ComputerName()
   {
      return GetParam<PcParam::ComputerName>();
   }

   std::wstring Get::UserName()
   {
      return GetParam<PcParam::UserName>();
   }

   std::wstring Get::UserDisplayName()
   {
      return GetParam<PcParam::UserDisplayName>();
   }

   std::wstring Get::OsVersion()
   {
      return GetParam<PcParam::OsVersion>();
   }

   std::wstring Get::MachineGuid()
   {
      return GetParam<PcParam::MachineGuid>();
   }

   std::wstring Get::HardwareString()
   {
      return GetParam<PcParam::HardwareString>();
   }

   Macs Get::MacAddresses()
   {
      return GetParam<PcParam::MacAddresses>();
   }
}