<div align="center">

# 📚 Shelf

**Простой файловый менеджер на Qt 6 / QML в стиле стеклянного шелла Quickshell**

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Qt 6](https://img.shields.io/badge/Qt-6.5%2B-41CD52?style=for-the-badge&logo=qt&logoColor=white)
![QML](https://img.shields.io/badge/QML-Qt%20Quick-41CD52?style=for-the-badge&logo=qt&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.21%2B-064F8C?style=for-the-badge&logo=cmake&logoColor=white)
![Ninja](https://img.shields.io/badge/Ninja-build-555555?style=for-the-badge)

![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![Wayland](https://img.shields.io/badge/Wayland-FFBC00?style=for-the-badge&logo=wayland&logoColor=black)
![Hyprland](https://img.shields.io/badge/Hyprland-58E1FF?style=for-the-badge&logo=hyprland&logoColor=black)
![Python](https://img.shields.io/badge/Python-shapely-3776AB?style=for-the-badge&logo=python&logoColor=white)
![SVG](https://img.shields.io/badge/SVG-icons-FFB13B?style=for-the-badge&logo=svg&logoColor=black)

<img src="docs/icons.png" alt="Стеклянный набор иконок Shelf" width="820">

</div>

---

## ✨ Зачем

Thunar и Nautilus остаются GNOME-приложениями даже после перекраски GTK-темы. Shelf — отдельное
приложение (свой процесс, `app_id` и `.desktop`), которое выглядит как остальной рабочий стол:
берёт палитру и настройки стекла у шелла [Quickshell](https://quickshell.org) и перекрашивается
вместе с ним при смене обоев, без перезапуска.

## 🧩 Возможности

- **Навигация** — сетка и список, история назад/вперёд, хлебные крошки с вводом пути, скрытые файлы
- **Боковая панель** — XDG-папки, закладки GTK (общие с Thunar и Nautilus), диски и флешки, корзина
- **Операции** — копирование, перемещение и вставка с прогрессом и отменой, переименование на месте,
  удаление в корзину, новая папка, окно свойств, «открыть в терминале»
- **Буфер обмена** совместим с GNOME-приложениями (`x-special/gnome-copied-files`)
- **Поиск** — рекурсивный, в фоновом потоке, результаты приходят по мере нахождения
- **Стеклянные иконки** — у каждого типа файла своя, папки приоткрываются при наведении,
  у стандартных папок и архивов — эмблемы из матового стекла
- **Особые папки** — эмблемы у git-репозиториев, `~/Projects`, `.config` и `.local`;
  `node_modules`, `build`, `.cache` и прочие служебные папки приглушены
- **Значки** — стрелка у ссылок, замок у файлов без доступа или только для чтения
- **Корзина** в боковой панели показывает, есть ли в ней что-то
- **Логотипы языков** на файлах кода: Python, C/C++, Rust, Go, JS/TS, Qt/QML и ещё три десятка

## ⌨️ Горячие клавиши

| Клавиши | Действие |
|---|---|
| <kbd>Alt</kbd>+<kbd>←</kbd> / <kbd>Alt</kbd>+<kbd>→</kbd> | Назад / вперёд |
| <kbd>Alt</kbd>+<kbd>↑</kbd>, <kbd>Backspace</kbd> | Папка выше |
| <kbd>Ctrl</kbd>+<kbd>L</kbd> | Ввести путь |
| <kbd>Ctrl</kbd>+<kbd>F</kbd>, <kbd>/</kbd> | Поиск |
| <kbd>Ctrl</kbd>+<kbd>H</kbd> | Скрытые файлы |
| <kbd>Ctrl</kbd>+<kbd>1</kbd> / <kbd>Ctrl</kbd>+<kbd>2</kbd> | Сетка / список |
| <kbd>F2</kbd> | Переименовать |
| <kbd>Delete</kbd> | В корзину |
| <kbd>Ctrl</kbd>+<kbd>C</kbd> / <kbd>X</kbd> / <kbd>V</kbd> | Копировать / вырезать / вставить |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>N</kbd> | Новая папка |
| <kbd>Ctrl</kbd>+<kbd>D</kbd> | В закладки |
| <kbd>Alt</kbd>+<kbd>Enter</kbd> | Свойства |
| <kbd>F4</kbd> | Терминал (kitty) в текущей папке |
| <kbd>F5</kbd>, <kbd>Ctrl</kbd>+<kbd>R</kbd> | Обновить |
| <kbd>F9</kbd> | Боковая панель |
| <kbd>Esc</kbd> | Отменить операцию / очистить поиск / снять выделение |

## 🔧 Сборка и установка

Нужны Qt ≥ 6.5 (Base, Declarative, Svg), CMake ≥ 3.21, Ninja и компилятор с C++20.

```sh
# Fedora
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg cmake ninja-build gcc-c++

cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build
cmake --install build
```

Сделать Shelf проводником по умолчанию:

```sh
xdg-mime default org.stasyao.shelf.desktop inode/directory
```

> [!NOTE]
> Сборка ограничена четырьмя параллельными компиляторами (`JOB_POOLS` в `CMakeLists.txt`):
> каждый `cc1plus` с Qt занимает около 350 МБ, и на 16 ГБ с открытым браузером
> сборка по умолчанию (nproc + 2 задач) доводила систему до OOM.

## 🎨 Тема

Shelf читает два файла шелла и следит за ними:

| Файл | Что берётся |
|---|---|
| `~/.local/state/quickshell/user/generated/qs_theme.json` | палитра, сгенерированная из обоев |
| `~/.config/quickshell/settings.json` | стекло вкл/выкл, прозрачность |

Фон окна полупрозрачный — размытие делает композитор (в Hyprland — обычный `blur`, отдельные
правила не нужны).

## 🗂️ Структура

```
src/            C++: модель папки, файловые операции, места, тема, провайдер иконок
qml/            интерфейс: окно, боковая панель, сетка/список, иконки, диалоги
icons/          стеклянные SVG-иконки типов файлов
icons/emblems/  эмблемы стандартных папок и архивов (белые силуэты)
icons/lang/     логотипы языков программирования
icons/badges/   значки ссылки и замка
tools/          gen_icons.py — генератор большей части иконок
data/           шаблон .desktop-файла
```

### Как рисуются иконки

Большую часть набора строит `tools/gen_icons.py`: фигуры описываются парой строк
(скруглённые прямоугольники, круги, линии), [shapely](https://shapely.readthedocs.io) объединяет
их и сам считает кромки и зазоры между предметами.

```sh
pip install shapely
python tools/gen_icons.py icons
```

Эмблемы и логотипы — плоские белые силуэты; матовое стекло на них накладывает
`qml/GlassEmblem.qml`. Новый логотип языка — это SVG в `icons/lang/` и одна строка
соответствия в `qml/FileIcon.qml`.

## 📜 Благодарности

- Стиль иконок и часть исходных SVG — [Icons8](https://icons8.com)
- Логотипы языков — [Simple Icons](https://simpleicons.org) (CC0); чашка Java нарисована отдельно
