**Я не знаю какой стиль вы предпочитаете поэтому, две реализации одного задания: C99 (ветка <code>Dev-C</code> и main) и C++17 (ветка <code>Dev-Cpp</code>).**


> [!CAUTION]
> **C99** и **C++17** — два отдельных сервера с одной логикой. Словарь `data/responses.csv` общий.

AT-сервер слушает TTY (или PTY), читает команды до `\r`/`\n` и отвечает по словарю и состоянию модема (`ATE`, `AT+CPIN`).

## CSV — `data/responses.csv`

Две колонки: `expect,answer`. Первая совпавшая строка побеждает, поэтому частные шаблоны выше, `*` в конце.

Ограниченный синтаксис шаблона (не POSIX regex):

| Символ | Смысл |
|---|---|
| `.` | ровно один любой символ |
| `*` | любая последовательность, в том числе пустая |

В ответах `\n` — перевод строки внутри поля. Кавычки по CSV: поле с запятыми в `"..."`, литеральная кавычка — `""`.

`ATE0`/`ATE1` и `AT+CPIN` есть и в файле, и в коде: эхо и PIN зависят от состояния, словарь их не подменяет.

## Заголовки C (`include/*.h`)

| Файл | Роль |
|---|---|
| `match.h` | `match_pattern`, `pattern_is_glob` — матчер `.` / `*` |
| `dict.h` | `Dict` / `DictEntry`: `dict_load`, `dict_lookup`, `dict_free` |
| `tty.h` | POSIX fd: открыть устройство или PTY, raw-режим, чтение строки, запись ответа |
| `modem.h` | `Modem`: эхо, PIN, `modem_handle` / `modem_serve` |
| `selftest.h` | `run_self_test` — матчер, словарь, extra, PTY |

## Заголовки C++ (`include/*.hpp`)

| Файл | Роль |
|---|---|
| `match.hpp` | те же `match_pattern` / `pattern_is_glob`, аргументы `std::string_view` |
| `dict.hpp` | класс `Dictionary` (`load`, `lookup`) |
| `tty.hpp` | класс `Tty`, RAII на fd, move-only |
| `modem.hpp` | класс `Modem`: `handle`, `serve(Tty&)` |
| `selftest.hpp` | `run_self_test(const std::string&)` |

## Linux


**C99**

```bash
git checkout Dev-C
make
make test
./at-server                 # PTY, путь slave в stderr
./at-server -d /dev/ttyUSB0 # реальный TTY
./at-server -f data/responses.csv
```

**C++17**

```bash
git checkout Dev-Cpp
make
make test
./at-server
./at-server -d /dev/ttyUSB0
```

Клиент к PTY:

```bash
minicom -D /dev/pts/N
```

## WSL

```bash
wsl
cd /mnt/h/NDM_test_C-_Rozov
sudo apt update
sudo apt install build-essential g++ make
```

Дальше как на Linux: `git checkout Dev-C` или `Dev-Cpp`, затем `make` / `make test` / `./at-server`.

**USB-UART в WSL виден только если порт проброшен в подсистему. **
