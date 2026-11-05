# PMLMRIM — исходники для Miranda NG 0.96.7

Основа: PMLMRIM-BETA3-direct-file-transfer-COMPILE-FIX1 (последняя версия от пользователя).
Этот архив рассчитан на размещение в исходном дереве **Miranda NG v0.96.7**.
Версия Miranda NG 0.96.7: https://github.com/miranda-ng/miranda-ng/releases/tag/v0.96.7

## Сборка

1. Получите исходники **Miranda NG v0.96.7** (тег `v0.96.7`, не `master`).
2. Распакуйте содержимое архива в корень дерева Miranda NG, чтобы файл был по пути `protocols/PMLMRIM/PMLMRIM.vcxproj`.
3. Откройте `protocols/PMLMRIM/PMLMRIM.vcxproj` в Visual Studio с установленными C++ toolset и Windows SDK, либо включите проект в решение Miranda NG из того же дерева исходников.
4. Выберите платформу `Win32` или `x64` **в соответствии с разрядностью Miranda NG** и соберите `Release`.
5. Скопируйте полученную `PMLMRIM.dll` в папку `Plugins` Miranda NG той же разрядности.

## Совместимость API

Сверены по официальным заголовкам `v0.96.7`:
- `PROTO_INTERFACE::SendFile(MCONTACT, const wchar_t*, wchar_t**)`;
- `FileAllow(MCONTACT, HANDLE, const wchar_t*)`;
- `FileDeny(MCONTACT, HANDLE, const wchar_t*)`;
- `FileCancel(MCONTACT, HANDLE)`;
- поле `DBEVENTINFO::iTimestamp`.

`PMLMRIM.vcxproj` использует относительный путь `../../build/vc.common/plugin.props` из дерева Miranda NG, поэтому отдельно от дерева он не соберётся.

## Важные ограничения

- Здесь сохранён прямой TCP-механизм передачи файлов из тестовой версии; **MRIM proxy/NAT fallback по-прежнему отсутствует**.
- Проверка реальной загрузки DLL и передачи файлов в Miranda NG 0.96.7 не проводилась; это исходники для проверки сборки, а не гарантия совместимости или работоспособности.
- Исправления авторизации, размера окон статусов и отображения Gender из предыдущих архивов сохранены.
- Это не полный исходный код самого Miranda NG, а дерево исходников плагина PMLMRIM для его сборки вместе с Miranda NG.
