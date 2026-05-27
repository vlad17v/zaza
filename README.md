# Консольный чат с голосовыми звонками через WebRTC

Консольное приложение на C++17 с голосовыми звонками через WebRTC и собственным TURN сервером.

## Стек

- **Язык:** C++17
- **Платформа:** FreeBSD (основная), Ubuntu (разработка)
- **Библиотеки:** Boost.Asio/Beast, OpenSSL, nlohmann/json, libdatachannel 0.24, SQLite3, jwt-cpp
- **TURN:** Собственная реализация (RFC 8656) — без coturn

## Структура проекта

```
project/
├── backend/               # Аутентификация, signaling, HTTP/WebSocket сервер
│   ├── auth/              # JWT, auth сервис, хранилище пользователей (SQLite)
│   ├── api/               # HTTP сервер, маршруты
│   ├── signaling/         # WebSocket сервер, роутер
│   ├── calls/             # Менеджер звонков
│   └── certs/             # TLS сертификаты
├── turn_server/           # Собственный TURN сервер (RFC 8656)
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

### Ubuntu / Debian

```sh
sudo apt install \
    build-essential cmake \
    libboost-system-dev \
    libssl-dev \
    libsqlite3-dev \
    nlohmann-json3-dev \
    libdatachannel-dev
```

### FreeBSD

```sh
pkg install cmake boost-libs openssl sqlite3 \
    nlohmann-json libdatachannel
```

---

## Сборка

```sh
git clone <repo>
cd project

# Скопировать и заполнить конфиги
cp backend.env.example backend.env
cp turn.env.example turn.env

# Сгенерировать TLS сертификат (только для разработки)
mkdir -p backend/certs
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout backend/certs/key.pem \
    -out    backend/certs/cert.pem \
    -days 365 \
    -subj "/CN=localhost"

# Собрать
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

# Терминал 1 — TURN сервер
./turn_server/turn_server

# Терминал 2 — Backend
./backend/backend_server

# Терминал 3 — Alice
./client_cli/client_cli

# Терминал 4 — Bob
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

## Юнит тесты

Запуск всех юнит тестов:

```sh
cd build
../scripts/run_tests.sh
```

### test_crypto

Тестирует криптографические примитивы в `shared/crypto/`.

| Тест | Что проверяется |
|------|----------------|
| `hmac_sha1` | Результат HMAC-SHA1, пустые входные данные, размер=20 |
| `hmac_sha256` | Результат HMAC-SHA256, известный вектор |
| `sha256 / long_term_key` | SHA-256 хеш, генерация MD5 ключа |
| `constant_time_compare` | Сравнение без timing-атаки |
| `base64` | Кодирование и декодирование |

### test_backend

Тестирует компоненты backend без сети.

| Тест | Что проверяется |
|------|----------------|
| `JWT` | Подпись, проверка, истечение, неверный секрет, невалидный токен |
| `UserStore` | Добавление, поиск, проверка пароля, дубликат, персистентность |
| `AuthService` | Успешный/неуспешный логин, проверка токена |
| `WsServer JWT` | Извлечение токена из URL, верификация |
| `WsServer send` | Отправка подключённому/отключённому пользователю |
| `HttpServer routing` | Маршрутизация, 404, неверный метод, обработка исключений |
| `CallManager` | Создание, принятие, отклонение, завершение, занятость, таймаут |
| `Routes / TURN credentials` | Генерация HMAC-SHA1 credentials, разные пользователи |

### test_turn

Тестирует компоненты TURN сервера без сети.

| Тест | Что проверяется |
|------|----------------|
| `XOR codec` | Кодирование/декодирование порта и адреса |
| `Parser` | Allocate запрос, неверный magic cookie, атрибуты |
| `decodeMethod / decodeClass` | Все STUN методы и классы |
| `Builder` | Ошибки 400/401/438, успешный ответ, transaction ID |
| `Builder/Parser round-trip` | Кодирование затем декодирование |
| `Data indication` | Сборка и разбор |
| `ChannelData` | Сборка/разбор, выравнивание, невалидные номера каналов |
| `NonceManager` | Генерация, валидация, истечение, очистка |
| `HmacValidator` | Генерация/валидация, истечение, неверный секрет |
| `LongTermCred` | Challenge, процесс аутентификации, MESSAGE-INTEGRITY, stale nonce |
| `isDeniedAddress` | Блокировка RFC 1918 диапазонов |
| `AllocationManager` | Аллокация, дубликат 5-tuple (437), CreatePermission, ChannelBind, relay данных |
| `SSL context factory` | Создание TLS и DTLS контекстов |

### test_client

Тестирует компоненты CLI клиента без сети.

| Тест | Что проверяется |
|------|----------------|
| `Parser` | Разбор команд, обработка пробелов |
| `Session` | Начальное состояние, логин, состояния звонка, toString |
| `Commands` | Все команды: login, call, accept, reject, hangup, mute, unmute, status, record, stop, sendfile, quit |
| `MessageHandler` | Входящий звонок, создан, завершён, ошибка, rtc.config, offer/answer/ICE, обработка ошибок |
| `WebRTC JSON форматы` | Структура JSON для offer/answer/ICE/rtc.config |
| `Файлы протокола` | Все файлы protocol/*.json являются валидным JSON |
| `WAV` | Запись и чтение, пустой файл, неверный формат |
| `JitterBuffer` | Запись/чтение, тишина при underrun, защита от переполнения |

---

## Интеграционные тесты

Для этих тестов необходимы все три сервера и два экземпляра клиента.

### Подготовка

```sh
# Зарегистрировать пользователей (один раз, после запуска backend)
curl -k -X POST https://localhost:8080/api/auth/register \
     -H "Content-Type: application/json" \
     -d '{"userId":"alice","password":"pass123"}'

curl -k -X POST https://localhost:8080/api/auth/register \
     -H "Content-Type: application/json" \
     -d '{"userId":"bob","password":"pass123"}'
```

### Тест 1 — Регистрация

```sh
curl -k -X POST https://localhost:8080/api/auth/register \
     -H "Content-Type: application/json" \
     -d '{"userId":"alice","password":"pass123"}'
```

**Ожидается:** `{"ok":true}`

### Тест 2 — Вход и JWT

```sh
curl -k -X POST https://localhost:8080/api/auth/login \
     -H "Content-Type: application/json" \
     -d '{"userId":"alice","password":"pass123"}'
```

**Ожидается:** `{"token":"eyJ...","userId":"alice"}`

### Тест 3 — TURN Credentials

```sh
TOKEN=$(curl -k -s -X POST https://localhost:8080/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{"userId":"alice","password":"pass123"}' | \
    python3 -c "import sys,json; print(json.load(sys.stdin)['token'])")

curl -k https://localhost:8080/api/turn/credentials \
     -H "Authorization: Bearer $TOKEN"
```

**Ожидается:**
```json
{
  "credential": "...",
  "ttl": 3600,
  "urls": ["turn:localhost:3478", "turns:localhost:5349"],
  "username": "...:alice"
}
```

### Тест 4 — Неверный токен

```sh
curl -k https://localhost:8080/api/turn/credentials \
     -H "Authorization: Bearer bad_token"
```

**Ожидается:** `{"error":"invalid token"}`

### Тест 5 — WebSocket и Signaling

```sh
npm install -g wscat

ALICE_TOKEN=...  # из теста 2
BOB_TOKEN=...

# Терминал A — Alice
wscat -c "wss://localhost:8080/ws?token=$ALICE_TOKEN" --no-check

# Терминал B — Bob
wscat -c "wss://localhost:8080/ws?token=$BOB_TOKEN" --no-check

# Alice отправляет:
{"type":"call.create","to":"bob"}
```

**Ожидается у Alice:** `{"callId":"...","type":"call.created"}`
**Ожидается у Bob:** `{"callId":"...","from":"alice","type":"call.incoming"}`

### Тест 6 — Принятие звонка и RTC Config

Bob отправляет:
```json
{"type":"call.accept","callId":"<callId>"}
```

**Ожидается:** Alice и Bob получают `rtc.config` с TURN credentials.
Alice получает `username: ...:alice`, Bob получает `username: ...:bob`.

### Тест 7 — Таймаут звонка

Alice создаёт звонок, Bob не отвечает в течение 30 секунд.

**Ожидается:** Оба получают `{"type":"call.failed","reason":"timeout"}` через ~30 секунд.

### Тест 8 — Полный звонок через CLI (WebRTC Connected)

```sh
# Alice
./client_cli/client_cli
> login alice pass123
> call bob

# Bob
./client_cli/client_cli
> login bob pass123
> accept
```

**Ожидаемая последовательность:**
```
[call] created, callId: ...
[rtc] config received, turn: turn:localhost:3478
[rtc] offer sent          # Alice
[rtc] answer sent         # Bob
[rtc] connecting...       # Оба
[rtc] answer applied      # Alice
[rtc] connected           # Оба
```

Логи TURN сервера подтверждают relay:
```
[turn] allocate success user=...:alice
[turn] allocate success user=...:bob
```

### Тест 9 — Mute / Unmute

Во время активного звонка:

```sh
> mute
muted
> status
# muted: yes
> unmute
unmuted
> status
# muted: no
```

**Ожидается:** Состояние меняется без ошибок, `status` корректно отражает флаг muted.

### Тест 10 — Завершение звонка

```sh
> hangup
call ended
```

**Ожидается:**
- Alice: `call ended`
- Bob: `[call] ended, reason: hangup`
- У обоих `status` показывает `state: Idle`
- `exit` завершается без краша

### Тест 11 — Запись и отправка WAV (только FreeBSD)

Требует `/dev/dsp` — запускать на FreeBSD:

```sh
> record test.wav
> # говорить несколько секунд
> stop
saved to test.wav

> # во время активного звонка
> sendfile test.wav
sending test.wav (N samples)
```

**Ожидается:** Bob слышит записанный аудиофайл.

### Тест 12 — Повторный звонок после завершения

После завершения звонка сразу позвонить снова:

```sh
> call bob
```

**Ожидается:** Новый `callId`, полный цикл завершается включая `[rtc] connected`.

### Тест 13 — TURN Relay в логах

Во время активного звонка логи TURN сервера показывают:

```
[turn] allocate success user=...:alice client=127.0.0.1:XXXXX
[turn] allocate success user=...:bob   client=127.0.0.1:XXXXX
```

ChannelData пакеты передаются между обоими клиентами в обоих направлениях.

---

## Тесты отказоустойчивости

### Перезапуск backend во время звонка

1. Установить звонок (`[rtc] connected` у обоих)
2. Убить backend (`Ctrl+C`)
3. Перезапустить backend (`./backend/backend_server`)

**Ожидается:**
```
[ws] disconnected
[call] ended, reason: server disconnected
[ws] reconnect attempt 1
...
[ws] reconnected
```

После переподключения: `status` показывает `state: Idle`, новые звонки работают нормально.

### Перезапуск TURN сервера во время звонка

1. Установить звонок
2. Убить TURN сервер (`Ctrl+C`)

**Ожидается:**
```
[rtc] failed — use 'hangup' and call again
```

Состояние сбрасывается в `Idle`. Новые звонки работают после перезапуска TURN.

### Звонок несуществующему пользователю

```sh
> call charlie
```

**Ожидается:** `[call.failed]` через 30 секунд таймаута.

### Звонок самому себе

```sh
> call alice
```

**Ожидается:** `cannot call yourself`

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
