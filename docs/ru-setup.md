# Русский гайд

## Что это

Это Linux-управление фронтальной RGB light bar панели на ноутбуках платформы Intel NUC M15 / совместимых Uniwill-ревизиях.

В репозитории лежит:
- минимальный kernel helper для вызова OEM WMI-методов
- userland CLI для переключения режимов панели
- краткие reverse notes и карта подтверждённых режимов

Решение не требует Windows, Wine или запуска вендорных сервисов в рантайме.

## На чём тестировалось

Тестовая машина:
- ноутбук на платформе Intel NUC M15 / Uniwill
- плата определялась как `Intel Corporation CM11EBI38W`
- Linux: Arch Linux
- ядро с поддержкой WMI/debugfs

Текущий working path подтверждён на реальном железе визуально.

## Что уже работает

Подтверждены:
- включение/выключение панели
- режимы Alexa/lightbar через raw path `F351 -> 0x0603`
- набор визуально проверенных состояний, включая:
  - `talking`
  - `thinking`
  - `notification_incoming`
  - `notification_unheard`
  - `bluetooth_connect`
  - `error`

Подробная карта состояний:
- [state-map.md](state-map.md)

## Что не нужно

Для уже работающего управления не нужны:
- Windows
- Wine
- `UniwillService.exe`
- `UWACPIDriver.sys`
- `NucSoftwareStudioService.exe`

Они использовались только как источник для reverse engineering.

## Как развернуть

### 1. Собрать kernel helper

Нужны:
- заголовки ядра
- `make`

Сборка:

```bash
cd kernel
make
```

### 2. Загрузить модуль

```bash
sudo insmod intel_lightbar_wmi.ko
```

После загрузки появится debugfs-интерфейс:

```bash
/sys/kernel/debug/intel_lightbar_wmi/
```

### 3. Проверить базовое управление

Включить рабочий режим:

```bash
sudo python tools/lightbar_ctl.py talking
```

Выключить:

```bash
sudo python tools/lightbar_ctl.py off
```

Посмотреть список состояний:

```bash
sudo python tools/lightbar_ctl.py --list-states
```

## Что делает CLI

Основной инструмент:
- `tools/lightbar_ctl.py`

Примеры:

```bash
sudo python tools/lightbar_ctl.py talking
sudo python tools/lightbar_ctl.py thinking
sudo python tools/lightbar_ctl.py notification_incoming
sudo python tools/lightbar_ctl.py error
sudo python tools/lightbar_ctl.py off
```

Сырые вызовы тоже доступны:
- `tools/raw_f351_call.py`

## Техническая суть

Рабочий путь сейчас такой:
- OEM WMI GUID: `F3517D45-0E66-41EF-8472-FCB7C98AE932`
- семейство методов: `0x0603`
- семантическое имя из новых Intel NSS пакетов: `AlexaLedControl`

Payload после MID:
- `function`
- `state`
- `speed`
- `brightness`
- `direction`

## Ограничения

- решение аппаратно-зависимое
- ранние HID-ревизии light bar могут вести себя иначе
- часть режимов может требовать контекст, который в Windows давал вендорный софт
- `direction` пока сохранён в API, но для подтверждённых режимов не был критичен

## Что дальше можно сделать

- упаковать модуль в DKMS
- повесить режимы на системные события Linux
- сделать daemon для уведомлений, Bluetooth, mute mic и ошибок
