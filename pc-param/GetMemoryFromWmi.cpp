#include <Windows.h>
#include <WbemCli.h> //для COM-интерфейсов WMI
#include <exception>
#include <atlbase.h>

#pragma comment (lib, "wbemuuid.lib") //для UUID'ов WMI

class ComInitializer
{
public:
   explicit ComInitializer(DWORD flag = COINIT_APARTMENTTHREADED)
   {
      if (FAILED(::CoInitializeEx(NULL, flag)))
         throw std::exception("Can't initialize COM!");
   }

   ~ComInitializer()
   {
      ::CoUninitialize();
   }

private:
   ComInitializer(ComInitializer const&);
   void operator=(ComInitializer const&);
};

size_t GetMemoryFromWmi()
{
   //Подключение COM
   ComInitializer com;

   //Инициализация защиты
   HRESULT r = ::CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
                              RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
   if (FAILED(r))
      throw std::exception("Error init security.");
   
   //Открытие локатора WMI
   CComPtr<IWbemLocator> pLocator;
   r = ::CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pLocator));
   if (FAILED(r))
      throw std::exception("Can't create locator.");

   //Подключение к серверу:
   CComPtr<IWbemServices> pService;
   r = pLocator->ConnectServer(L"root\\CIMV2", NULL, NULL, NULL, WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                                 NULL, NULL, &pService);
   if (FAILED(r))
      throw std::exception("Can't connect to service CIMV2.");

   //Получение инстансов объектов
   CComPtr<IEnumWbemClassObject> pInstances;
   r = pService->CreateInstanceEnum(L"Win32_PhysicalMemory", WBEM_FLAG_FORWARD_ONLY, NULL, &pInstances);
   if (FAILED(r))
   {
      throw std::exception("Can't get wmi instances!");
   }

   size_t megabytes(0); //результат
   //Перебор полученных инстансов и подсчет объема памяти
   while(true)
   {
      CComPtr<IWbemClassObject> pObj;
      ULONG uReturned(0);

      r = pInstances->Next(0, 1, &pObj, &uReturned);
      if (uReturned == 0)
         break;

      VARIANT value;
      r = pObj->Get(L"Capacity", 0, &value, NULL, NULL);
      if (FAILED(r))
         throw std::exception("Can't get capacity of module");

      unsigned __int64 result = _wcstoui64(value.bstrVal, NULL, 10);
      result /= 1024 * 1024;
      megabytes += static_cast<size_t>(result);
   }

   return megabytes;
}