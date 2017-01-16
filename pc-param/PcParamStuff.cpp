#include <Windows.h>
#include <memory>
#include <type_traits>
#include <sstream>
#include <algorithm>
#include <time.h>
#include <fstream>
#include <locale>
#include <array>
#include <list>
#include <tuple>

#include "PcParam.h"

namespace PcParam
{
   std::wstring Stuff::MakePassword(size_t length)
   {
      struct Alphabet
      {
         std::vector<wchar_t> values;
         Alphabet()
         {
            srand(static_cast<int>(time(NULL)));

            for (wchar_t w = L'a'; w <= L'z'; ++w)
               values.push_back(w);
            for (wchar_t w = L'A'; w <= L'Z'; ++w)
               values.push_back(w);
            for (wchar_t w = L'0'; w <= L'9'; ++w)
               values.push_back(w);
         }
      } static alphabet;

      std::wstring result;
      for (size_t i = 0; i < length; ++i)
      {
         result.push_back(alphabet.values[rand() % alphabet.values.size()]);
      }

      return result;
   }

   bool Stuff::CopyToClipboard(std::wstring const& str)
   {
      //Открытие буфера обмена
      if (FALSE == ::OpenClipboard(NULL))
         return false;

      std::unique_ptr<void, void (*)(void *)> auto_close(reinterpret_cast<void *>(0x1), [](void *){::CloseClipboard();});

      //Очитска содержимого буфера обмена
      if (FALSE == ::EmptyClipboard())
         return false;

      const size_t size = (str.size() + 1) * sizeof(wchar_t);

      //Получение указателя
      HGLOBAL g = ::GlobalAlloc(GMEM_MOVEABLE, size);
      if (NULL == g)
         return false;
      std::unique_ptr<std::remove_pointer<HGLOBAL>::type, BOOL (WINAPI *)(HGLOBAL)> memory(g, ::GlobalUnlock);

      //Копирование памяти
      memcpy(::GlobalLock(memory.get()), str.c_str(), size);
   
      //Заполнение буфера обмена
      ::SetClipboardData(CF_UNICODETEXT, g);

      //Закрытие буфера обмена
      ::CloseClipboard();

      return true;
   }

   std::wstring Stuff::MakeFreeradiusString(std::wstring const& login,
                                            std::wstring const& password,
                                            std::wstring const& mac)
   {
      std::wstringstream str;
      str << login << L" Cleartext-Password := \"" << password << "\"";
      if (!mac.empty())
         str << ", Calling-Station-Id == \"" << mac << "\"";
      return str.str();
   }

   Macs::const_iterator Stuff::GetFirstWiFi(Macs const& addresses)
   {
      return std::find_if(addresses.begin(), addresses.end(), [](Mac const& value) -> bool
         {
            return value.IsWifi;
         });
   }

   std::wstring Stuff::GenerateUsername(Info const& info)
   {
      return info.UserName + L"_" + info.ComputerName;
   }

   std::wstring Stuff::GenerateFilename(FullInfo const& info)
   {
      std::wstringstream str;
      str << info.MachineGuid << L"_" << info.ComputerName << L".txt";
      return str.str();
   }

   Info Stuff::GetInfo()
   {
      Info result;

      result.ComputerName = Get::ComputerName();
      result.UserName = Get::UserName();
      result.UserDisplayName = Get::UserDisplayName();
      result.MachineGuid = Get::MachineGuid();
      result.OsVersion = Get::OsVersion();
      result.HardwareString = Get::HardwareString();
      result.MacAddresses = Get::MacAddresses();

      return result;
   }

   FullInfo Stuff::MakeFullInfo(Info const& info)
   {
      FullInfo result(info);

      result.Login = GenerateUsername(info);
      result.Password = MakePassword(12);

      auto mac = GetFirstWiFi(info.MacAddresses);
      result.Regstring = MakeFreeradiusString(result.Login, result.Password,
                                              mac != info.MacAddresses.end() ? mac->Address : std::wstring());

      result.Person = result.Phone = Default;

      return result;
   }

   bool Stuff::SaveToFile(FullInfo const& info, std::wstring const& filename)
   {
      FILE *f;
      if (_wfopen_s(&f, filename.c_str(), L"wt, ccs=UTF-16LE") != 0)
         return false;

      struct
      {
         wchar_t const * const header;
         wchar_t const * const value;
      } name[] = {{L"Имя компьютера         : ", info.ComputerName.c_str()},
                  {L"Имя пользователя       : ", info.UserName.c_str()},
                  {L"Отображаемое имя       : ", info.UserDisplayName.c_str()},
                  {L"Идентификатор установки: ", info.MachineGuid.c_str()},
                  {L"Строка версии          : ", info.OsVersion.c_str()},
                  {L"Строка оборудования    : ", info.HardwareString.c_str()},
                  {L"", L""},
                  {L"Логин                  : ", info.Login.c_str()},
                  {L"Пароль                 : ", info.Password.c_str()},
                  {L"", L""},
                  {L"Строка freeradius:\n", info.Regstring.c_str()},
                  {L"", L""},
                  {L"Владелец               : ", info.Person.c_str()},
                  {L"Телефон                : ", info.Phone.c_str()}};

      for (auto const& data: name)
         fwprintf(f, L"%s%s\n", data.header, data.value);

      fwprintf(f, L"\nАппаратные адреса:\n"
                  L"|  № |     mac-адрес     |    Тип   |                      Название                      |"
                  L"                      Описание                      |\n");

      for (size_t i = 0; i < info.MacAddresses.size(); ++i)
         fwprintf(f, L"| %2u | %17s | %8s | %50s | %50s |\n",
                  i + 1, info.MacAddresses[i].Address.c_str(),
                  (info.MacAddresses[i].IsWifi ? WiFi.c_str() : Ethernet.c_str()),
                  info.MacAddresses[i].Name.c_str(), info.MacAddresses[i].Desc.c_str());

      fclose(f);
      return true;
   }

   bool Stuff::LoadFromFile(FullInfo& info, std::wstring const& filename)
   {
      FILE *f;
      if (_wfopen_s(&f, filename.c_str(), L"rt, ccs=UTF-16LE") != 0)
         return false;

      enum ParseType
      {
         Basic, Mac, Regstring
      } type = ParseType::Basic;

      struct ParseUnit
      {
         std::wregex Regex; //регулярное выражения для поиска
         std::wstring* Result; //запись результата, если есть
         ParseType NextType; //тип следующей строки

         ParseUnit(wchar_t const* regex, std::wstring *result, ParseType next):
            Regex(regex), Result(result), NextType(next)
         {}
      };

      std::list<ParseUnit> units;
      units.emplace_back(L"^Имя компьютера\\s*\\:\\s+(.+)\\n?$", &info.ComputerName, ParseType::Basic);
      units.emplace_back(L"^Имя пользователя\\s*\\:\\s+(.+)\\n?$", &info.UserName, ParseType::Basic);
      units.emplace_back(L"^Отображаемое имя\\s*\\:\\s+(.+)\\n?$", &info.UserDisplayName, ParseType::Basic);
      units.emplace_back(L"^Идентификатор установки\\s*\\:\\s+(.+)\\n?$", &info.MachineGuid, ParseType::Basic);
      units.emplace_back(L"^Строка версии\\s*\\:\\s+(.+)\\n?$", &info.OsVersion, ParseType::Basic);
      units.emplace_back(L"^Строка оборудования\\s*\\:\\s+(.+)\\n?$", &info.HardwareString, ParseType::Basic);
      units.emplace_back(L"^Логин\\s*\\:\\s+(.+)\\n?$", &info.Login, ParseType::Basic);
      units.emplace_back(L"^Пароль\\s*\\:\\s+(.+)\\n?$", &info.Password, ParseType::Basic);
      units.emplace_back(L"^Строка freeradius\\:\\n?$", nullptr, ParseType::Regstring);
      units.emplace_back(L"^\\|\\s*№\\s*\\|\\s*mac-адрес\\s*\\|\\s*Тип\\s*\\|\\s*Название\\s*\\|\\s*Описание\\s*\\|\\n?$", nullptr, ParseType::Mac);
      units.emplace_back(L"^Владелец\\s*\\:\\s+(.+)\\n?$", &info.Person, ParseType::Basic);
      units.emplace_back(L"^Телефон\\s*\\:\\s+(.+)\\n?$", &info.Phone, ParseType::Basic);
      
      std::array<wchar_t, 1024> buffer;
      std::wcmatch result;
      while(fgetws(buffer.data(), buffer.size(), f) != nullptr)
      {
         switch (type) //в зависимости от ожидаемого типа строки выбираем как анализировать ее
         {
         case ParseType::Mac:
            {
               static std::wregex mac(
                  L"^\\| +\\d+ "
                  L"\\| ([0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}) "
                  L"\\| +(Wi-Fi|Ethernet) "
                  L"\\| +(\\w.+) "
                  L"\\| +(\\w.+) "
                  L"\\|\\n?$");
               if (std::regex_match(buffer.data(), result, mac) && result.size() == 5)
               {
                  PcParam::Mac mac;
                  mac.Address = result[1];
                  mac.IsWifi = result[2] == WiFi;
                  mac.Name = result[3];
                  mac.Desc = result[4];
                  info.MacAddresses.push_back(mac);
                  type = ParseType::Mac;
                  break; //переход к следующей строке
               }
               else //в этом случае анализ будет продолжен в следующем блоке case
                  type = ParseType::Basic;
            }

         case ParseType::Basic: //алгоритм для базового типа
            for (auto unit = units.begin(); unit != units.end(); ++unit) //поиск среди доступных типов строк
            {
               if (std::regex_match(buffer.data(), result, unit->Regex)) 
               {
                  //Найдена подгруппа и есть указатель на строку, то выполняется запись
                  type = unit->NextType; //установка слеующего типа
                  if (result.size() == 2 && unit->Result != nullptr)
                     unit->Result->assign(result[1]); //запись результата
                  units.erase(unit); //удаление типа строки, так как она уже обнаружена
                  break;
               }
            }
            break;

         case ParseType::Regstring:
            {
               type = ParseType::Basic;
               
               static std::wregex regstr(L"^(.+)\\n?$");
               if (std::regex_match(buffer.data(), result, regstr) && result.size() == 2)
                  info.Regstring.assign(result[1]);
            }
            break;
         }
      }

      fclose(f);
      return true;
   }

   Macs::const_iterator Stuff::FindMacByString(FullInfo const& info)
   {
      static std::wregex regex(L"^(.+) Cleartext-Password\\s+:=\\s+\"(\\w+)\","
                               L"\\s+Calling-Station-Id\\s+==\\s+\""
                               L"([0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2})\"$");
      std::wcmatch result;
      if (std::regex_match(info.Regstring.c_str(), result, regex) && result.size() == 4 &&
          info.Login == result[1] && info.Password == result[2])
      {
         return std::find_if(info.MacAddresses.begin(), info.MacAddresses.end(), [result](Mac const& mac) -> bool
            {
               return mac.Address == result[3];
            });
      }

      return info.MacAddresses.end();
   }
}