#include <windows.h>
#include <wlanapi.h>
#include <memory>
#include <array>

#pragma comment(lib, "wlanapi.lib")

//������ ���� �
//https://msdn.microsoft.com/en-us/library/windows/desktop/ms706716(v=vs.85).asp

bool IsAdapterWifi(PCHAR name)
{
   //�������� ������� WLan API
   DWORD version = 0;
   HANDLE client_handle;
   DWORD result =::WlanOpenHandle(2, NULL, &version, &client_handle);
   if (result != ERROR_SUCCESS)
      throw std::exception("Can't open wlan handle");

   std::unique_ptr<std::remove_pointer<HANDLE>::type, BOOL (WINAPI *)(HANDLE)> handle(client_handle, ::CloseHandle);
   
   //��������� �����������
   PWLAN_INTERFACE_INFO_LIST ifaces_pointer;
   result = WlanEnumInterfaces(handle.get(), NULL, &ifaces_pointer);
   if (result != ERROR_SUCCESS)
      throw std::exception("Can' get wlan interfaces");

   std::unique_ptr<WLAN_INTERFACE_INFO_LIST, VOID (WINAPI *)(PVOID)> ifaces(ifaces_pointer, ::WlanFreeMemory);

   //������������ �� � ��������� GUID ����������
   for (size_t i = 0; i < static_cast<size_t>(ifaces->dwNumberOfItems); i++)
   {
      static const size_t length = 64; //������ ��������� �������

      //��������������� �� ���� � unicode-������
      std::array<wchar_t, length> guid;
      int result = ::StringFromGUID2(ifaces->InterfaceInfo[i].InterfaceGuid, guid.data(), guid.size());
      if (result == 0)
         throw std::exception("Can't get string GUID");

      //��������������� unicode-������ � ������� ������
      std::array<char, length> ansi_guid;
      result = ::WideCharToMultiByte(0, 0, guid.data(), -1, ansi_guid.data(), ansi_guid.size(), NULL, NULL);
      if (result == 0)
         throw std::exception("Can't get wide char GUID to char");

      //��������� GUID
      if (strcmp(ansi_guid.data(), name) == 0)
         return true;
   }

   return false;
}
