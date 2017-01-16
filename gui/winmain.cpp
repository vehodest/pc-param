#include <Windows.h>
#include <commctrl.h>
#include <array>
#include "..\pc-param\PcParam.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' "\
                        "name='Microsoft.Windows.Common-Controls' "\
                        "version='6.0.0.0' processorArchitecture='*' "\
                        "publicKeyToken='6595b64144ccf1df' language='*'\"")
bool loading = false;

inline HWND GetMacsHwnd(HWND dlg)
{
   return ::GetDlgItem(dlg, IDC_MACS);
}

void LoadInfo(HWND dlg, PcParam::FullInfo const& info)
{
   //Заполнение полей
   ::SetDlgItemTextW(dlg, IDC_COMPNAME, info.ComputerName.c_str());
   ::SetDlgItemTextW(dlg, IDC_USERNAME, info.UserName.c_str());
   ::SetDlgItemTextW(dlg, IDC_USERDISPLAYNAME, info.UserDisplayName.c_str());
   ::SetDlgItemTextW(dlg, IDC_GUID, info.MachineGuid.c_str());
   ::SetDlgItemTextW(dlg, IDC_OS, info.OsVersion.c_str());
   ::SetDlgItemTextW(dlg, IDC_HARDWARE, info.HardwareString.c_str());
   ::SetDlgItemTextW(dlg, IDC_LOGIN, info.Login.c_str());
   ::SetDlgItemTextW(dlg, IDC_PASSWORD, info.Password.c_str());
   ::SetDlgItemTextW(dlg, IDC_REGSTRING, info.Regstring.c_str());
   ::SetDlgItemTextW(dlg, IDC_PERSON, info.Person.c_str());
   ::SetDlgItemTextW(dlg, IDC_PHONE, info.Phone.c_str());

   //Получение дескриптора ListView
   HWND list = GetMacsHwnd(dlg);
   ListView_DeleteAllItems(list);

   //Структура, которая будет использоваться для добавления элементов
   LVITEM item;
   memset(&item, 0, sizeof(item));
   item.mask = LVIF_TEXT;

   auto mac_from_reg = PcParam::Stuff::FindMacByString(info);
   
   //Вставка строк
   int i = 0;
   std::array<wchar_t, 16> number;
   for (auto mac = info.MacAddresses.begin(); mac != info.MacAddresses.end(); ++mac, ++i)
   {
      item.iItem = i;
      _itow_s(i+1, number.data(), number.size(), 10);
      ListView_InsertItem(list, &item); //добавление строки и установка первой колонки
      ListView_SetItemText(list, i, 1, number.data()); //номер
      ListView_SetItemText(list, i, 2, const_cast<LPWSTR>(mac->Address.c_str())); //mac-адрес
      ListView_SetItemText(list, i, 3, const_cast<LPWSTR>((mac->IsWifi ? PcParam::WiFi : PcParam::Ethernet).c_str())); //тип
      ListView_SetItemText(list, i, 4, const_cast<LPWSTR>(mac->Name.c_str())); //имя
      ListView_SetItemText(list, i, 5, const_cast<LPWSTR>(mac->Desc.c_str())); //описание
      ListView_SetCheckState(list, i, mac_from_reg == mac); //выделение
   }
   ListView_SetItemState (list, 0, LVIS_SELECTED, LVIS_SELECTED);
}

PcParam::FullInfo GetInfo(HWND dlg)
{
   PcParam::FullInfo result;

   //Получение значений полей
   struct
   {
      int id;
      std::wstring &value;
   } fields[] = {{IDC_COMPNAME, result.ComputerName},
                 {IDC_USERNAME, result.UserName},
                 {IDC_USERDISPLAYNAME, result.UserDisplayName},
                 {IDC_GUID, result.MachineGuid},
                 {IDC_OS, result.OsVersion},
                 {IDC_HARDWARE, result.HardwareString},
                 {IDC_LOGIN, result.Login},
                 {IDC_PASSWORD, result.Password},
                 {IDC_REGSTRING, result.Regstring},
                 {IDC_PERSON, result.Person},
                 {IDC_PHONE, result.Phone}};
   std::array<wchar_t, 1024> buffer;
   for (auto &f: fields)
   {
      if (::GetDlgItemTextW(dlg, f.id, buffer.data(), buffer.size()) == 0)
         continue;
      f.value.assign(buffer.data());
   }

   //Получение дескриптора ListView
   HWND list = GetMacsHwnd(dlg);
   int count = ListView_GetItemCount(list);

   for (int i = 0; i < count; ++i)
   {
      PcParam::Mac mac;
      //mac-адрес
      ListView_GetItemText(list, i, 2, buffer.data(), buffer.size());
      mac.Address.assign(buffer.data());
      //тип
      ListView_GetItemText(list, i, 3, buffer.data(), buffer.size());
      mac.IsWifi = std::wstring(buffer.data()) == PcParam::WiFi;
      //имя
      ListView_GetItemText(list, i, 4, buffer.data(), buffer.size());
      mac.Name.assign(buffer.data());
      //описание
      ListView_GetItemText(list, i, 5, buffer.data(), buffer.size());
      mac.Desc.assign(buffer.data());

      result.MacAddresses.emplace_back(mac);
   }

   return result;
}

void CreateString(HWND dlg)
{
   std::array<wchar_t, 512> login, password;
   ::GetDlgItemTextW(dlg, IDC_LOGIN, login.data(), login.size());
   ::GetDlgItemTextW(dlg, IDC_PASSWORD, password.data(), password.size());

   static std::wregex empty(L"^\\s*$");

   std::wstring result;
   if (!std::regex_match(login.data(), empty) &&
       !std::regex_match(password.data(), empty))
   {
      HWND list = GetMacsHwnd(dlg);
      std::wstring mac;
      int count = ListView_GetItemCount(list);
      for (int i = 0; i < count; ++i)
      {
         if (ListView_GetCheckState(list, i))
         {
            std::array<wchar_t, 128> buffer;
            ListView_GetItemText(list, i, 2, buffer.data(), buffer.size());
            mac = buffer.data();
            break;
         }
      }
      result = PcParam::Stuff::MakeFreeradiusString(login.data(), password.data(), mac);
   }

   ::SetDlgItemTextW(dlg, IDC_REGSTRING, result.c_str());
}

void CopyString(HWND dlg)
{
   std::array<wchar_t, 512> buffer;
   if (::GetDlgItemTextW(dlg, IDC_REGSTRING, buffer.data(), buffer.size()) == 0)
      return;
   PcParam::Stuff::CopyToClipboard(buffer.data());
}

void OpenFile(HWND dlg)
{
   std::array<wchar_t, 512> buffer;

   //https://msdn.microsoft.com/en-us/library/windows/desktop/ms646829(v=vs.85).aspx#open_file
   OPENFILENAME data;
   ZeroMemory(&data, sizeof(data));

   data.lStructSize = sizeof(data);
   data.hwndOwner = dlg;
   data.lpstrFile = buffer.data();
   data.lpstrFile[0] = '\0';
   data.nMaxFile = buffer.size();
   data.lpstrFilter = L"Текстовые файлы\0*.TXT\0Все файлы\0*.*\0";
   data.nFilterIndex = 1;
   data.lpstrFileTitle = NULL;
   data.nMaxFileTitle = 0;
   data.lpstrInitialDir = NULL;
   data.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

   if (::GetOpenFileNameW(&data) == FALSE)
      return;

   PcParam::FullInfo info;
   if (!PcParam::Stuff::LoadFromFile(info, data.lpstrFile))
      return;

   LoadInfo(dlg, info);
}

void SaveFile(HWND dlg)
{
   PcParam::FullInfo info = GetInfo(dlg);

   std::array<wchar_t, 512> buffer;
   wcscpy_s(buffer.data(), buffer.size(), PcParam::Stuff::GenerateFilename(info).c_str());

   OPENFILENAME data;
   ZeroMemory(&data, sizeof(data));

   data.lStructSize = sizeof(data);
   data.hwndOwner = dlg;
   data.lpstrFile = buffer.data();
   data.lpstrDefExt = L"txt";
   data.nMaxFile = buffer.size();
   data.lpstrFilter = L"Текстовые файлы\0*.TXT\0Все файлы\0*.*\0";
   data.nFilterIndex = 1;
   data.lpstrFileTitle = NULL;
   data.nMaxFileTitle = 0;
   data.lpstrInitialDir = NULL;
   data.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

   if (!::GetSaveFileNameW(&data))
      return;

   PcParam::Stuff::SaveToFile(info, data.lpstrFile);
}

void Detect(HWND dlg)
{
   PcParam::FullInfo result = PcParam::Stuff::MakeFullInfo(PcParam::Stuff::GetInfo());
   std::array<wchar_t, 1024> buffer;

   //Защита от замены значений ФИО и телефона на значения по-умолчанию
   struct
   {
      int id;
      std::wstring &value;
   } fields[] = {{IDC_PERSON, result.Person},
                 {IDC_PHONE, result.Phone}};

   for (auto &f: fields)
   {
      ::GetDlgItemTextW(dlg, f.id, buffer.data(), buffer.size());
      std::wstring value(buffer.data());
      if (value.empty() || PcParam::Default == value)
         continue;
      f.value = value;
   }

   //Загрузка значений из структуры
   LoadInfo(dlg, result);
}

void Initialize(HWND dlg)
{
   //Настройка
   HWND list = GetMacsHwnd(dlg); //получение дескриптора ListView
   ListView_SetView(list, LV_VIEW_DETAILS);
   ListView_SetExtendedListViewStyle(list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP);

   LVCOLUMN column;
   memset(&column, 0, sizeof(column));
   column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
   column.fmt = LVCFMT_RIGHT;

   int index = 0;

   //Добавление колонки, которая позже будет удалена, чтобы обойти проблему установки выравнивания
   column.cx = 10;
   column.pszText = L"";
   ListView_InsertColumn(list, index++, &column);

   //Первая колонка -- чекбокс:
   column.cx = 30;
   column.pszText = const_cast<LPWSTR>(L"\u2713");
   ListView_InsertColumn(list, index++, &column);

   //Первая колонка -- нормер:
   column.cx = 30;
   column.pszText = const_cast<LPWSTR>(L"№");
   ListView_InsertColumn(list, index++, &column);
   
   //Вторая колонка - mac-адрес:
   column.cx = 105;
   column.pszText = const_cast<LPWSTR>(L"Mac-адрес");
   ListView_InsertColumn(list, index++, &column);

   //Третья колонка - тип:
   column.cx = 55;
   column.pszText = const_cast<LPWSTR>(L"Тип");
   ListView_InsertColumn(list, index++, &column);

   //Четвертая колонка - название:
   column.cx = 150;
   column.pszText = const_cast<LPWSTR>(L"Название");
   ListView_InsertColumn(list, index++, &column);

   //Пятая колонка - описание:
   column.cx = 250;
   column.pszText = const_cast<LPWSTR>(L"Описание");
   ListView_InsertColumn(list, index++, &column);

   //Удаление вспомогательной колонки
   ListView_DeleteColumn(list, 0);

   //Запрет редактирования всех полей ввода, кроме логина и пароля
   int edits[] = {IDC_COMPNAME, IDC_USERNAME, IDC_USERDISPLAYNAME, IDC_OS, IDC_HARDWARE, IDC_GUID, IDC_REGSTRING};
   for (auto edit: edits)
      ::SendMessageW(::GetDlgItem(dlg, edit), EM_SETREADONLY, 1, 0);
}

void UncheckElementsExсeptOne(HWND dlg, int number)
{
   HWND list = GetMacsHwnd(dlg);
   int count = ListView_GetItemCount(list);
   for (int i = 0; i < count; ++i)
   {
      if (ListView_GetCheckState(list, i) && i != number)
      {
         ListView_SetCheckState(list, i, FALSE);
      }
   }
}

INT_PTR CALLBACK DialogProc(HWND dlg, UINT msg, WPARAM w, LPARAM l)
{
   switch (msg)
   {
   case WM_INITDIALOG:
      Initialize(dlg);
      break;

   case WM_COMMAND:
      {
         //Кнопка Копировать
         if (LOWORD(w) == IDC_COPY)
         {
            CopyString(dlg);
         }
         //Кнопка Определить
         else if (LOWORD(w) == IDC_DETECT)
         {
            loading = true;
            Detect(dlg);
            loading = false;
         }
         //Кнопка Сохранить
         else if (LOWORD(w) == IDC_SAVE)
         {
            SaveFile(dlg);
         }
         //Кнопка Открыть
         else if (LOWORD(w) == IDC_OPEN)
         {
            loading = true;
            OpenFile(dlg);
            loading = false;
         }
         //Изменение полей Логин, Пароль
         else if (!loading && HIWORD(w) == EN_CHANGE && (LOWORD(w) == IDC_LOGIN || LOWORD(w) == IDC_PASSWORD))
         {
            CreateString(dlg);
         }
         else
            return FALSE;
      }
      break;

   case WM_NOTIFY:
      if (!loading && w == IDC_MACS)
      {
         auto notify = reinterpret_cast<LPNMLISTVIEW>(l); //первая часть структуры -- общая для всех NMHDR
         if (notify->hdr.idFrom == IDC_MACS && //подтверждает id контрола (значение в w может быть неуникальным согласно MSDN)
             notify->hdr.code == LVN_ITEMCHANGED && //тип сообщения -- изменение элемента
             ((notify->uOldState & LVIS_STATEIMAGEMASK) != (notify->uNewState & LVIS_STATEIMAGEMASK))) //изменилось состояние чекбокса
         {
            //Если элемент был отмечен, снимем пометку со всех остальных, проверив корректность
            if ((notify->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2))
            {
               {//проверка корректности и выдача предупреждения
                  HWND list = GetMacsHwnd(dlg);
                  std::array<wchar_t, 1024> buffer;
                  ListView_GetItemText(list, notify->iItem, 3, buffer.data(), buffer.size());
                  if (PcParam::WiFi != buffer.data()) //если тип выбранного адаптера не wifi
                  {
                     loading = true;
                     //Снимаем пометку (для того чтобы на время диалога не отображалось двух пометок):
                     ListView_SetCheckState(list, notify->iItem, FALSE);
                     //Подтверждение
                     if (IDYES == ::MessageBoxW(dlg, L"Выбран аппаратный адрес адаптера, не являющегося беспроводным. Продолжить?",
                                            L"PcParam", MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2))
                     {//пометка нужна
                        ListView_SetCheckState(list, notify->iItem, TRUE);
                     }
                     else
                     {
                      //пометка не нужна
                        loading = false;
                        return TRUE;
                     }
                  }
               }

               loading = true;
               UncheckElementsExсeptOne(dlg, notify->iItem);
               loading = false;
            }

            CreateString(dlg);
            return TRUE;
         }
      }
      return FALSE;

   case WM_CLOSE:
      ::PostQuitMessage(0);

   default:
      return FALSE;
   }

   return TRUE;
}

int WINAPI wWinMain(HINSTANCE h, HINSTANCE, LPWSTR, int)
{
   INITCOMMONCONTROLSEX control;
   control.dwSize = sizeof(control);
   control.dwICC = ICC_STANDARD_CLASSES;

   ::InitCommonControlsEx(&control);

   ::DialogBoxW(h, MAKEINTRESOURCE(IDD_MAINDIALOG), NULL, DialogProc);

   MSG msg;
   while(::GetMessageW(&msg, NULL, 0, 0))
   {
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
   }

   return 0;
}