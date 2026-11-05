# MRIM / Miranda NG — входящая авторизация

Изменены только пути авторизации в `src/server.cpp`, `src/proto.cpp`, `src/proto.h`, `src/mrim_session.cpp/.h`, `src/mrim_wire.h`.

- Входящий `MRIM_CS_MESSAGE_ACK` с `MESSAGE_FLAG_AUTHORIZE = 0x08` вызывает штатный `Proto_AuthRecv` с `DB::AUTH_BLOB` и не попадает в историю обычного чата.
- Ответ сервера `MRIM_CS_MESSAGE_RECV` сохраняется для входящего запроса.
- `Authorize` отправляет `MRIM_CS_AUTHORIZE (0x1020)` с адресом инициатора.
- `AuthDeny` снимает локальное ожидание: отдельной команды отказа в MRIM нет.
- `AuthRequest` отправляет MRIM message с флагом `0x08`.
- Возможность авторизации объявляется через `PF1_AUTHREQ`.

**Ограничение:** нативная сборка/тест в Windows Miranda NG здесь не выполнялись. Проверить получение с другого аккаунта, принятие, отказ и исходящую заявку.
