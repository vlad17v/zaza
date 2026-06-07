## Стек

- **Язык:** C++17
- **Платформа:** FreeBSD
- **Библиотеки:** Boost.Asio/Beast, OpenSSL, nlohmann/json, libdatachannel 0.24, SQLite3, jwt-cpp

## Структура проекта

```
project/
├── backend/               # Аутентификация, signaling, HTTP/WebSocket сервер
│   ├── auth/              # JWT, auth сервис, хранилище пользователей (SQLite)
│   ├── api/               # HTTP сервер, маршруты
│   ├── signaling/         # WebSocket сервер, роутер
│   ├── calls/             # Менеджер звонков
│   └── certs/             # TLS сертификаты
├── turn_server/           # TURN сервер
│   └── src/
│       ├── core/          # Диспетчер
│       ├── transport/     # UDP/TCP/TLS/DTLS транспорт
│       ├── message/       # STUN парсер/билдер
│       ├── auth/          # Long-term credentials, HMAC
│       └── allocation/    # Менеджер аллокаций
├── client_cli/            # CLI клиент
│   └── src/
│       ├── cli/           # REPL, команды, парсер
│       ├── http/          # HTTP клиент
│       ├── signaling/     # WebSocket клиент, обработчик сообщений
│       ├── rtc/           # PeerConnection, SDP, ICE
│       └── audio/         # Захват, воспроизведение, WAV, jitter buffer
├── shared/                # Общие утилиты
│   ├── crypto/            # HMAC, SHA-256, base64, constant-time сравнение
│   ├── config/            # Загрузчик .env конфига
│   └── log/               # Логгер (stdout + файл)
├── tests/                 # Юнит тесты
├── protocol/              # JSON схемы API
├── scripts/               # Скрипты сборки и запуска
├── backend.env.example    # Шаблон конфига backend
└── turn.env.example       # Шаблон конфига TURN сервера
```

---

## Зависимости

```sh
pkg install cmake boost-libs openssl sqlite3 \
    nlohmann-json libdatachannel
```

---

## Сборка

```sh
git clone <repo>
cd project

cp backend.env.example backend.env
cp turn.env.example turn.env

mkdir -p backend/certs
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout backend/certs/key.pem \
    -out    backend/certs/cert.pem \
    -days 365 \
    -subj "/CN=localhost"

./scripts/build.sh
```

---

## Конфигурация

Перед запуском заполнить `backend.env` и `turn.env` в корне проекта.
Файлы читаются при старте из директории `build/` как `../backend.env` и `../turn.env`.

### backend.env

| Ключ | По умолчанию | Описание |
|------|-------------|----------|
| `HOST` | `0.0.0.0` | Адрес для прослушивания |
| `PORT` | `8080` | Порт HTTPS/WSS |
| `CERT_FILE` | `../backend/certs/cert.pem` | TLS сертификат |
| `KEY_FILE` | `../backend/certs/key.pem` | TLS приватный ключ |
| `DB_PATH` | `../data/chat.db` | Путь к базе данных SQLite |
| `JWT_SECRET` | — | **Обязательно изменить в продакшн** |
| `JWT_TTL` | `86400` | Время жизни токена в секундах |
| `EXPIRE_INTERVAL` | `5` | Интервал проверки таймаута звонков (сек) |
| `TURN_HOST` | `localhost` | Хост TURN сервера |
| `TURN_PORT_PLAIN` | `3478` | Порт TURN UDP/TCP |
| `TURN_PORT_TLS` | `5349` | Порт TURN TLS/DTLS |
| `TURN_SECRET` | — | Общий секрет (должен совпадать с turn.env) |
| `TURN_TTL` | `3600` | Время жизни TURN credentials |
| `LOG_FILE` | _(пусто)_ | Путь к файлу логов (пусто = только stdout) |

### turn.env

| Ключ | По умолчанию | Описание |
|------|-------------|----------|
| `CERT_FILE` | `../backend/certs/cert.pem` | TLS сертификат |
| `KEY_FILE` | `../backend/certs/key.pem` | TLS приватный ключ |
| `SHARED_SECRET` | — | Должен совпадать с `TURN_SECRET` в backend.env |
| `REALM` | `chat.example.com` | TURN realm |
| `RELAY_ADDR` | `127.0.0.1` | IP для relay (публичный IP в продакшн) |
| `PORT_MIN` | `49152` | Начало диапазона relay портов |
| `PORT_MAX` | `65535` | Конец диапазона relay портов |
| `LOG_FILE` | _(пусто)_ | Путь к файлу логов |

---

## Запуск

Все команды выполняются из директории `build/`.

```sh
cd build

./turn_server/turn_server

./backend/backend_server

./client_cli/client_cli

./client_cli/client_cli
```

---

## Команды CLI

| Команда | Описание |
|---------|----------|
| `login <userId> <password>` | Регистрация (первый раз) и вход |
| `call <userId>` | Позвонить пользователю |
| `accept` | Принять входящий звонок |
| `reject` | Отклонить входящий звонок |
| `hangup` | Завершить текущий звонок |
| `mute` | Выключить микрофон |
| `unmute` | Включить микрофон |
| `record <file.wav>` | Начать запись в WAV файл |
| `stop` | Остановить запись |
| `sendfile <file.wav>` | Отправить WAV файл во время звонка |
| `status` | Показать текущее состояние |
| `exit` / `quit` | Выйти |

---

## Архитектура

```
Client A                Backend              Client B
   |                       |                    |
   |-- HTTPS регистрация-> |                    |
   |-- WSS логин --------> |                    |
   |                       | <-- WSS логин ---- |
   |-- call.create ------> |                    |
   |                       |-- call.incoming -> |
   |                       | <-- call.accept -- |
   |<-- rtc.config --------|-- rtc.config ----> |
   |-- webrtc.offer -----> |-- webrtc.offer --> |
   |                       | <-- webrtc.answer- |
   |<-- webrtc.answer -----|                    |
   |                       |                    |
   |<====== DTLS через TURN relay ============> |
```

Путь TURN relay (весь медиа трафик):
```
Client A  -->  TURN сервер  -->  Client B
         <--                <--
```

---

## Безопасность

- TLS везде: HTTPS/WSS на порту 8080, TURN TLS/DTLS на порту 5349
- Временные TURN credentials на основе HMAC-SHA1 (по умолчанию TTL: 1 час)
- JWT аутентификация для всех API и WebSocket подключений
- Обязательно изменить `JWT_SECRET` и `SHARED_SECRET` в продакшн
- Использовать настоящий TLS сертификат в продакшн (не самоподписанный)
- Установить `RELAY_ADDR` равным публичному IP TURN сервера в продакшн
- RFC 1918 адреса заблокированы в TURN permissions по умолчанию
