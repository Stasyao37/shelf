pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Shelf

// Настройки Shelf: откуда цвета, стекло, оттенок иконок, терминал.
// Всё применяется сразу и хранится в ~/.config/shelf/shelf.conf
GlassDialog {
    id: dlg

    signal firstRunRequested()
    // Сообщение в тост главного окна
    signal message(string text)

    padding: 0
    width: 500
    height: Math.min(content.implicitHeight + 90, parent ? parent.height - 48 : 600)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // ── Мелкие контролы ─────────────────────────────────────────────
    component SectionTitle: Text {
        topPadding: 10
        color: Theme.subtext0
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize - 3
        font.weight: Font.DemiBold
    }

    // Ползунок; moved — пока тянут, released — отпустили (тогда и сохраняем)
    component Slide: Item {
        id: sl
        property real from: 0
        property real to: 1
        property real value: 0
        signal moved(real v)
        signal released(real v)
        height: 22
        readonly property real frac: (value - from) / (to - from)
        Rectangle {
            id: track
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width; height: 6; radius: 3
            color: Theme.ink(0.10)
            Rectangle {
                width: parent.width * sl.frac
                height: parent.height; radius: 3
                color: Theme.accent
            }
        }
        Rectangle {
            width: 16; height: 16; radius: 8
            anchors.verticalCenter: parent.verticalCenter
            x: sl.frac * (sl.width - width)
            color: Theme.material ? Theme.accent : Qt.lighter(Theme.accent, 1.1)
            border.width: 2
            border.color: Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, 0.9)
        }
        MouseArea {
            anchors.fill: parent
            anchors.margins: -6
            cursorShape: Qt.PointingHandCursor
            function at(mx) {
                // Шаг 5 %: ноль и края легко поймать мышью
                var f = Math.max(0, Math.min(1, (mx - 6) / (sl.width)))
                return Math.round((sl.from + f * (sl.to - sl.from)) * 20) / 20
            }
            onPressed: (m) => sl.moved(at(m.x))
            onPositionChanged: (m) => { if (pressed) sl.moved(at(m.x)) }
            onReleased: (m) => sl.released(at(m.x))
        }
    }

    // ── Содержимое ──────────────────────────────────────────────────
    contentItem: Item {
        // Кнопки закреплены внизу — видны, даже если список пришлось прокручивать
        Flickable {
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: footer.top; bottomMargin: 10 }
            clip: true
            contentHeight: content.implicitHeight + 24
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: content
                x: 20; y: 20
                width: dlg.width - 40
                spacing: 4

                // Заголовок и версия — одной строкой
                Item {
                    width: parent.width
                    height: titleText.implicitHeight
                    Text {
                        id: titleText
                        text: qsTr("Настройки")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize + 2
                        font.weight: Font.DemiBold
                    }
                    Text {
                        anchors { right: parent.right; baseline: titleText.baseline }
                        text: "Shelf " + Qt.application.version
                        color: Theme.overlay1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 4
                    }
                }

                SectionTitle { text: qsTr("ЦВЕТА") }
                Repeater {
                    model: ThemeSource.sources
                    ChoiceRow {
                        required property var modelData
                        label: modelData.name
                        available: modelData.available
                        checked: ThemeSource.sourceSetting === modelData.id
                        note: !modelData.available ? qsTr("не найден")
                            : modelData.id === "auto" ? qsTr("сейчас: %1").arg(dlg.sourceName(ThemeSource.source)) : ""
                        onPicked: ThemeSource.sourceSetting = modelData.id
                    }
                }

                SectionTitle { text: qsTr("СТЕКЛО") }
                Item {
                    width: parent.width
                    height: 34
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Стеклянный вид")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    ToggleSwitch {
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        checked: !Theme.material
                        onToggled: (on) => ThemeSource.setGlass(on)
                    }
                }
                Item {
                    width: parent.width
                    height: 34
                    visible: !Theme.material
                    Text {
                        id: alphaLabel
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Плотность")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    Slide {
                        id: alphaSlide
                        anchors { left: parent.left; leftMargin: 130; right: alphaValue.left; rightMargin: 14; verticalCenter: parent.verticalCenter }
                        from: 0.2; to: 1.0
                        value: ThemeSource.settings.glassAlpha ?? 0.62
                        onMoved: (v) => value = v
                        onReleased: (v) => ThemeSource.setGlassAlpha(v)
                    }
                    Text {
                        id: alphaValue
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        width: 40
                        horizontalAlignment: Text.AlignRight
                        text: Math.round(alphaSlide.value * 100) + " %"
                        color: Theme.subtext0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 3
                    }
                }
                Text {
                    visible: ThemeSource.glassOverridden
                    leftPadding: 12
                    text: qsTr("Вернуть как у шелла")
                    color: resetMa.containsMouse ? Theme.accent : Theme.subtext0
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 3
                    MouseArea {
                        id: resetMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: ThemeSource.resetGlass()
                    }
                }

                SectionTitle { text: qsTr("ИКОНКИ") }
                Item {
                    width: parent.width
                    height: 34
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Оттенок палитры")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    Slide {
                        id: tintSlide
                        anchors { left: parent.left; leftMargin: 160; right: tintValue.left; rightMargin: 14; verticalCenter: parent.verticalCenter }
                        from: 0; to: 1
                        value: ThemeSource.iconTint
                        // Иконки перекрашиваются прямо пока тянешь
                        onMoved: (v) => ThemeSource.iconTint = v
                        onReleased: (v) => ThemeSource.iconTint = v
                    }
                    Text {
                        id: tintValue
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        width: 40
                        horizontalAlignment: Text.AlignRight
                        text: tintSlide.value === 0 ? qsTr("нет") : Math.round(tintSlide.value * 100) + " %"
                        color: Theme.subtext0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 3
                    }
                }

                SectionTitle { text: qsTr("ИНТЕРФЕЙС") }
                Item {
                    width: parent.width
                    height: 34
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Анимации")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    Text {
                        anchors { right: animToggle.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        text: qsTr("переходы, окна, появление файлов")
                        color: Theme.overlay1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 4
                    }
                    ToggleSwitch {
                        id: animToggle
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        checked: Theme.animations
                        onToggled: (on) => Theme.setAnimations(on)
                    }
                }

                SectionTitle { text: qsTr("ПРОВОДНИК") }
                Item {
                    width: parent.width
                    height: 38
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - (Apps.defaultFileManager ? 40 : 200)
                        elide: Text.ElideRight
                        text: Apps.defaultFileManager ? qsTr("Shelf — проводник по умолчанию") : qsTr("Папки открывает другая программа")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    Text {
                        visible: Apps.defaultFileManager
                        anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                        text: "\u{f00c}"
                        color: Theme.accent
                        font.family: Theme.iconFont
                        font.pixelSize: Theme.fontSize - 2
                    }
                    TextButton {
                        visible: !Apps.defaultFileManager
                        anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
                        implicitHeight: 30
                        text: qsTr("Сделать Shelf")
                        primary: true
                        onClicked: Apps.makeDefaultFileManager()
                    }
                }

                // Окна выбора файлов из других программ (портал FileChooser)
                Item {
                    width: parent.width
                    height: 38
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 80
                        elide: Text.ElideRight
                        text: qsTr("Выбор файлов в окнах Shelf")
                        color: Environment.portalBackend ? Theme.text : Theme.subtext0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    ToggleSwitch {
                        visible: Environment.portalBackend
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        checked: Environment.portalPicker
                        onToggled: (on) => {
                            var err = Environment.setPortalPicker(on)
                            if (err) dlg.message(err)
                            else dlg.message(on ? qsTr("Готово — новые окна выбора откроются в Shelf; GTK-программы — после перезапуска")
                                                : qsTr("Выбор файлов вернулся к прежнему"))
                        }
                    }
                }
                Text {
                    width: parent.width
                    leftPadding: 12
                    rightPadding: 12
                    bottomPadding: 4
                    wrapMode: Text.Wrap
                    text: Environment.portalBackend
                        ? qsTr("Firefox, Chromium, Telegram, GTK- и Qt-программы будут открывать и сохранять файлы через Shelf. Меняет ~/.config/xdg-desktop-portal и подключает ~/.config/hypr/shelf.conf (с копиями .bak-shelf).")
                        : qsTr("Нужен бэкенд портала: поставьте Shelf пакетом или выполните %1").arg(Environment.portalInstallCommand())
                    color: Theme.overlay1
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 4
                    MouseArea {
                        anchors.fill: parent
                        enabled: !Environment.portalBackend
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { FileOps.copyText(Environment.portalInstallCommand()); dlg.message(qsTr("Команда скопирована")) }
                    }
                }

                SectionTitle { text: qsTr("ТЕРМИНАЛ") }
                ChoiceRow {
                    label: qsTr("Автоматически")
                    note: Terminal.chosen === "" ? (Terminal.current ? qsTr("сейчас: %1").arg(Terminal.current) : qsTr("терминал не найден")) : ""
                    checked: Terminal.chosen === ""
                    onPicked: Terminal.chosen = ""
                }
                Repeater {
                    model: Terminal.available
                    ChoiceRow {
                        required property string modelData
                        label: modelData
                        checked: Terminal.chosen === modelData
                        onPicked: Terminal.chosen = modelData
                    }
                }

                // Язык меняется сразу, без перезапуска
                SectionTitle { text: qsTr("ЯЗЫК") }
                Item {
                    width: parent.width
                    height: 42
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Язык интерфейса")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    TextButton {
                        id: langBtn
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        text: dlg.languageLabel(Language.choice) + "  ▾"
                        onClicked: {
                            var p = langBtn.mapToItem(langMenu.parent, langBtn.width - langMenu.implicitWidth, langBtn.height + 4)
                            var top = langBtn.mapToItem(langMenu.parent, 0, -4).y
                            langMenu.show(p.x, p.y, dlg.languageItems(), top)
                        }
                    }
                }
            }
        }

        Item {
            id: footer
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 20; rightMargin: 20; bottomMargin: 16 }
            height: 34
            TextButton {
                text: qsTr("Первая настройка…")
                onClicked: { dlg.close(); dlg.firstRunRequested() }
            }
            TextButton {
                anchors.right: parent.right
                text: qsTr("Готово")
                primary: true
                onClicked: dlg.close()
            }
        }
    }

    ContextMenu { id: langMenu; parent: dlg.contentItem }

    function languageName(code) {
        var list = Language.options
        for (var i = 0; i < list.length; ++i)
            if (list[i].code === code) return list[i].name
        return code
    }
    // "" — как в системе: в скобках — какой язык это сейчас
    function languageLabel(code) {
        return code === "" ? qsTr("Как в системе (%1)").arg(languageName(Language.system)) : languageName(code)
    }
    function languageItems() {
        var items = [{ text: languageLabel(""), checked: Language.choice === "", act: function () { Language.choice = "" } },
                     { separator: true }]
        Language.options.forEach(function (o) {
            items.push({ text: o.name, checked: Language.choice === o.code, act: function () { Language.choice = o.code } })
        })
        return items
    }

    function sourceName(id) {
        var list = ThemeSource.sources
        for (var i = 0; i < list.length; ++i)
            if (list[i].id === id) return list[i].name
        return id
    }
}
