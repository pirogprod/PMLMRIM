# Состояние передачи файлов MRIM

Это исходники в разработке, **не рабочий модуль передачи файлов Miranda NG**.

Добавлено:
- `mrim_file_transport.h/.cpp`: отправка и приём точно заданного числа байт через настраиваемые read/write callbacks, 1460-байтные буферы, обработка частичных записей, отмены и прогресса;
- `mrim_file_stream.cpp` и новый transport добавлены в `PMLMRIM.vcxproj` (раньше file_stream не компилировался);
- `tests/file_transport_test.cpp` — тесты для передачи 100 КБ, коротких чтений/записей, обрыва и ограничений команд.

НЕ реализовано:
- `CMrimProto::SendFile`, `FileAllow`, `FileDeny`, `FileCancel` и нативные file ACK в Miranda;
- обработка входящих MRIM FILE_TRANSFER / FILE_TRANSFER_ACK в HandlePacket;
- создание TCP listener, соединение с peer, HELLO/GET_FILE на реальном сокете;
- связь transfer ID ↔ файловая сессия и ресурсы/жизненный цикл;
- MRIM proxy (0x1044-0x1047), NAT traversal;
- проверка сборки под MSVC и реальный тест на двух Windows-клиентах.

**Не добавлять `PF1_FILE` до завершения этих частей.**

Локальная проверка: `g++ -std=c++17 -Wall -Wextra -Werror tests/file_transport_test.cpp src/mrim_file_transport.cpp -o test && ./test`.
