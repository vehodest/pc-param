#pragma once
#include <string>
#include <vector>
#include <regex>

namespace PcParam
{
   static const std::wstring ErrorMsg = L"Не удалось получить";
   static const std::wstring Default  = L"?";
   static const std::wstring WiFi     = L"Wi-Fi";
   static const std::wstring Ethernet = L"Ethernet";

   struct Mac
   {
      std::wstring Address; //mac-адрес
      bool IsWifi; //тип адаптера wifi, иначе ethernet
      std::wstring Name; //имя адаптера
      std::wstring Desc; //описание адаптера
   };

   typedef std::vector<Mac> Macs;

   struct Info
   {
      std::wstring ComputerName;
      std::wstring UserName;
      std::wstring UserDisplayName;
      std::wstring MachineGuid;
      std::wstring OsVersion;
      std::wstring HardwareString;
      Macs MacAddresses;
   };

   struct FullInfo: Info
   {
      std::wstring Login;
      std::wstring Password;
      std::wstring Regstring;
      std::wstring Person;
      std::wstring Phone;

      FullInfo() {}

      FullInfo(Info const& base):
         Info(base)
      {
      }
   };

   template <typename T>
   class Base
   {
   public:
      typedef T ValueType;

      explicit Base(T const& error_value = ErrorMsg):
         result(error_value)
      {}

      T Get() const
      {
         return result;
      }

   protected:
      T result;
   };

   class Get
   {
   public:
      static std::wstring ComputerName();
      static std::wstring UserName();
      static std::wstring UserDisplayName();
      static std::wstring MachineGuid();
      static std::wstring OsVersion();
      static std::wstring HardwareString();
      static Macs MacAddresses();

   private:
      template <typename T>
      static typename T::ValueType GetParam()
      {
         T pcparam;
         return pcparam.Get();
      }
   };

   class Stuff
   {
   public:
      static std::wstring MakePassword(size_t length);
      static bool CopyToClipboard(std::wstring const& str);
      static std::wstring MakeFreeradiusString(std::wstring const& login,
                                               std::wstring const& password,
                                               std::wstring const& mac);
      static Macs::const_iterator GetFirstWiFi(Macs const& addresses);
      static std::wstring GenerateUsername(Info const& info);
      static std::wstring GenerateFilename(FullInfo const& info);
      static Info GetInfo();
      static FullInfo MakeFullInfo(Info const& info);
      static bool SaveToFile(FullInfo const& info, std::wstring const& filename);
      static bool LoadFromFile(FullInfo& info, std::wstring const& filename);
      static Macs::const_iterator FindMacByString(FullInfo const& info);
   };

   class ComputerName: public Base<std::wstring>
   {
   public:
      ComputerName();
   };

   class UserName: public Base<std::wstring>
   {
   public:
      UserName();
   };

   class UserDisplayName: public Base<std::wstring>
   {
   public:
      UserDisplayName();
   };

   class OsVersion: public Base<std::wstring>
   {
   public:
      OsVersion();
   };

   class MachineGuid: public Base<std::wstring>
   {
   public:
      MachineGuid();
   };

   class HardwareString: public Base<std::wstring>
   {
   public:
      HardwareString();
   };

   class MacAddresses: public Base<Macs>
   {
   public:
      MacAddresses();
   };
}