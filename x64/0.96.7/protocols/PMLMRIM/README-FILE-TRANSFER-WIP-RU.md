# MRIM передача файлов — промежуточная реализация (НЕ ГОТОВА К ИСПОЛЬЗОВАНИЮ)

На основе https://github.com/mrimsu/mrim-docs/blob/main/files.md

Добавлены команды FILE_TRANSFER (0x1026), FILE_TRANSFER_ACK (0x1027) и безопасные сериализатор/парсер сообщений согласования передачи в `mrim_session.cpp` и `mrim_session.h`.

Проверены round-trip кодирование/декодирование запроса и ACK, а также отклонение усечённого ACK.

**Не реализовано:** обработка событий этих команд в `server.cpp`, интеграция `SendFile`/`FileAllow`/`FileDeny`/`FileCancel` с Miranda NG, TCP сокет приема/отправки и handshake `MRA_FT_HELLO`/`MRA_FT_GET_FILE`, прокси 0x1044..0x1047 и отчеты прогресса. Флаг PF1_FILE не выставлен, чтобы не показывать пользователю неработающую команду.

Тестирование кодека (если есть g++):

```
g++ -std=c++17 -Isrc tests/file_codec_test.cpp src/mrim_wire.cpp src/mrim_session.cpp -o file_codec_test
./file_codec_test
```

Для готового плагина необходимо собрать DLL против SDK Miranda NG и протестировать два клиента с прямым и проксированным соединением.
