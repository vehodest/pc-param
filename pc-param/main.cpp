#include <crtdbg.h>
#include <locale>
#include <iostream>
#include <conio.h>
#include <algorithm>
#include <chrono>
#include "PcParam.h"

void Print(PcParam::Info const& info)
{
   //Вывод параметров компьютера:
   std::wcout << L"PC parameters:" << std::endl;
   std::wcout << L"Computer name    : " << info.ComputerName << std::endl;
   std::wcout << L"User name        : " << info.UserName << std::endl;
   std::wcout << L"User display name: " << info.UserDisplayName << std::endl;
   std::wcout << L"Machine GUID     : " << info.MachineGuid << std::endl;
   std::wcout << L"OS               : " << info.OsVersion << std::endl;
   std::wcout << L"Hardware string  : " << info.HardwareString<< std::endl;

   std::wcout << L"Mac addresses    : " << std::endl;
   auto macs = info.MacAddresses;
   for (auto const &mac: macs)
      std::wcout << mac.Address << L", "
                 << mac.Name << L", "
                 << mac.Desc << L", "
                 << (mac.IsWifi ? L"wi-fi" : L"ethernet")
                 << std::endl;
}

void Print(PcParam::FullInfo const& info)
{
   Print(static_cast<PcParam::Info>(info));

   std::wcout << std::endl << L"Additional param from FullInfo:" << std::endl;
   std::wcout << L"Login    : " << info.Login << std::endl;
   std::wcout << L"Password : " << info.Password << std::endl;
   std::wcout << L"Regstring: " << info.Regstring << std::endl;

   std::wcout << std::endl << L"Other values:" << std::endl;
   std::wcout << L"Filename to save: " << PcParam::Stuff::GenerateFilename(info) << std::endl;
}

void BasicTest()
{
   auto info = PcParam::Stuff::GetInfo();
   Print(info);

   //Сгенерированные параметры и действия:
   std::wcout << std::endl << L"Generated parameters:" << std::endl;
   std::wstring pass = PcParam::Stuff::MakePassword(12);
   std::wcout << pass;
   if (PcParam::Stuff::CopyToClipboard(pass))
      std::wcout << L" (password copied to clipboard)" << std::endl;
   else
      std::wcout << L" (can't copy password to clipboard)" << std::endl;

   std::wcout << L"First wifi mac: ";
   auto wifi = PcParam::Stuff::GetFirstWiFi(info.MacAddresses);
   if (wifi != info.MacAddresses.end())
   {
      std::wcout << wifi->Address << std::endl;
      std::wcout << PcParam::Stuff::MakeFreeradiusString(L"user", pass, wifi->Address) << std::endl;
   }
   else
      std::wcout << L"Not found." << std::endl;
}

void GenerationTest()
{
   PcParam::Info info;
   info.ComputerName = L"COMPUTER";
   info.UserName = L"John";
   info.UserDisplayName = L"John Doe";
   info.MachineGuid = L"11111111-1111-1111-1111-111111111111";
   info.HardwareString = L"Any CPU N MHz, Many Mb RAM";
   info.OsVersion = L"Super Windows";

   for (size_t i = 0; i < 10; ++i)
   {
      const size_t length = 128;
      wchar_t buffer[length];

      PcParam::Mac mac;
      swprintf_s(buffer, length, L"Adapter #%02u", i+1);
      mac.Name = buffer;
      swprintf_s(buffer, length, L"Description #%02u", i+1);
      mac.Desc = buffer;
      swprintf_s(buffer, length, L"%02u-%02u-%02u-%02u-%02u-%02u", i, i, i, i, i, i);
      mac.Address = buffer;
      mac.IsWifi = i % 2 != 0;

      info.MacAddresses.emplace_back(mac);
   }

   PcParam::FullInfo fullinfo = PcParam::Stuff::MakeFullInfo(info);

   std::wcout << L"### Full info ###" << std::endl;
   Print(fullinfo);

   //Сохранение в файл
   std::wstring filename = PcParam::Stuff::GenerateFilename(fullinfo);
   PcParam::Stuff::SaveToFile(fullinfo, filename);

   //Загрузка из файла
   PcParam::FullInfo result;
   PcParam::Stuff::LoadFromFile(result, filename);

   std::wcout << std::endl << L"### Load result ###" << std::endl;
   Print(result);

   auto iter = PcParam::Stuff::FindMacByString(result);
   if (iter != result.MacAddresses.end())
   {
      std::wcout << L"Mac from register string found:" << std::endl;
      std::wcout << iter->Address << L" " << iter->Name << L" " << iter->Desc << std::endl;
   }
   else
      std::wcout << L"Mac from register string not found." << std::endl;
}

void RegexForMacString()
{
   std::vector<std::wstring> data;
   data.push_back(L"|  1 | 00-00-00-00-00-00 | Ethernet |                                        Adapter #01 |                                    Description #01 |");
   data.push_back(L"|  2 | 01-01-01-01-01-01 |    Wi-Fi |                                        Adapter #02 |                                    Description #02 |");
   data.push_back(L"|  3 | 02-02-02-02-02-02 | Ethernet |                                        Adapter #03 |                                    Description #03 |");
   data.push_back(L"|  4 | 03-03-03-03-03-03 |    Wi-Fi |                                        Adapter #04 |                                    Description #04 |");
   data.push_back(L"|  5 | 04-04-04-04-04-04 | Ethernet |                                        Adapter #05 |                                    Description #05 |");
   data.push_back(L"|  6 | 05-05-05-05-05-05 |    Wi-Fi |                                        Adapter #06 |                                    Description #06 |");
   data.push_back(L"|  7 | 06-06-06-06-06-06 | Ethernet |                                        Adapter #07 |                                    Description #07 |");
   data.push_back(L"|  8 | 07-07-07-07-07-07 |    Wi-Fi |                                        Adapter #08 |                                    Description #08 |");
   data.push_back(L"|  9 | 08-08-08-08-08-08 | Ethernet |                                        Adapter #09 |                                    Description #09 |");
   data.push_back(L"| 10 | 09-09-09-09-09-09 |    Wi-Fi |                                        Adapter #10 |                                    Description #10 |");
   data.push_back(L"| 11 | aa-bb-cc-dd-ee-ff |    Wi-Fi |                                        Adapter #11 |                                    Description #11 |");
   data.push_back(L"| 12 | a1-b2-c3-d4-e5-f6 |    Wi-Fi |                                        Adapter #12 |                                    Description #12 |");//*/

   std::wregex mac(L"^\\| +\\d+ "
                   L"\\| ([0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}) "
                   L"\\| +(Wi-Fi|Ethernet) "
                   L"\\| +(\\w.+) "
                   L"\\| +(\\w.+) "
                   L"\\|\\n?$");

   long long avg = 0;
   for (auto const& d: data)
   {
      std::wcout << std::endl << L"Parse...";

      auto start = std::chrono::system_clock::now();
      std::wcmatch match;
      bool result = std::regex_match(d.c_str(), match, mac);
      auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count();
      avg += msec;
      std::wcout << L"done. Time: " << msec
                 << L" msec" << std::endl << L"Result: ";
      if (result)
      {
         std::wcout << L"success! Matches:" << std::endl;
         for (size_t i = 0; i < match.size(); ++i)
            std::wcout << i << L". \"" << match[i] << L"\"" << std::endl;
      }
      else
         std::wcout << L"not found :-(" << std::endl;
   }

   std::wcout << std::endl << L"Average: " << static_cast<double>(avg) / static_cast<double>(data.size()) << L" msec" << std::endl;
}

void RegexForParseRegstring()
{
   std::wstring regstr = L"Vladimir_COMPUTER Cleartext-Password := \"kRpnTJuCzNAW\", Calling-Station-Id == \"01-01-01-01-01-01\"";

   std::wregex regex(L"^(.+) Cleartext-Password\\s+:=\\s+\"(\\w+)\","
                     L"\\s+Calling-Station-Id\\s+==\\s+\"([0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2}-[0-9a-f]{2})\"$");
   std::wcmatch match;
   if (std::regex_match(regstr.c_str(), match, regex))
   {
      std::wcout << L"Matched! Subgroups:" << std::endl;
      for (size_t i = 0; i < match.size(); ++i)
         std::wcout << i << L". \"" << match[i] << L"\"" << std::endl;
   }
   else
      std::wcout << L"Not matched!" << std::endl;
}

void main()
{
   _CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); //контроль утечек памяти
   _wsetlocale(LC_ALL, L"rus");

   //BasicTest();
   GenerationTest();
   //RegexForMacString();
   //RegexForParseRegstring();

   std::wcout << L"That's all!" << std::endl;
   _getch();
}