import QtQuick
import Shelf

// Зеркало массива для Repeater: строки сверяются по ключу и добавляются и убираются
// по одной, а не пересоздаются все, — делегат успевает сыграть появление и уход.
// Роли: key, entering (новая — проявиться), leaving (ушла — исчезнуть; убирается
// из модели через leaveMs). Сам объект — item(key); rev меняется при каждой сверке
QtObject {
    id: keyed

    property var source: []
    property string keyRole: "path"
    property int leaveMs: Theme.dur(200)
    // Первое наполнение — без анимации: при запуске всё уже на месте
    property bool ready: false
    property int rev: 0
    readonly property ListModel model: ListModel {}

    property var _items: ({})

    function item(key) { return _items[key] || ({}) }

    function _indexOf(key) {
        for (var i = 0; i < model.count; ++i) {
            var r = model.get(i)
            if (r.key === key && !r.leaving) return i
        }
        return -1
    }

    function sync() {
        var src = source || []
        var keep = {}, items = Object.assign({}, _items)
        for (var s = 0; s < src.length; ++s) {
            var k = String(src[s][keyRole])
            keep[k] = true
            items[k] = src[s]
        }
        // До вставки строк: новый делегат сразу берёт свой объект через item(key)
        _items = items
        // Ушедшие — помечаем, удалит таймер
        var now = Date.now()
        for (var i = 0; i < model.count; ++i) {
            var r = model.get(i)
            if (!keep[r.key] && !r.leaving) {
                model.setProperty(i, "leaving", true)
                model.setProperty(i, "leaveAt", now + leaveMs)
            }
        }
        // Новые — на свои места; переставленные — сдвигаем
        var p = 0
        for (var j = 0; j < src.length; ++j) {
            var key = String(src[j][keyRole])
            var at = _indexOf(key)
            if (at < 0) {
                model.insert(p, { key: key, entering: ready, leaving: false, leaveAt: 0 })
                ++p
            } else if (at >= p) {
                p = at + 1
            } else {
                model.move(at, p - 1, 1)
            }
        }
        ++rev
        if (leaveMs <= 0) _drop()
        else if (!_timer.running) _timer.start()
    }

    function _drop() {
        var now = Date.now(), left = false
        for (var i = model.count - 1; i >= 0; --i) {
            var r = model.get(i)
            if (!r.leaving) continue
            if (r.leaveAt <= now) {
                // Тот же ключ успели вернуть — объект нужен новой строке
                if (_indexOf(r.key) < 0) delete _items[r.key]
                model.remove(i)
            } else {
                left = true
            }
        }
        if (left) _timer.restart()
    }

    readonly property Timer _timer: Timer {
        interval: Math.max(1, keyed.leaveMs)
        onTriggered: keyed._drop()
    }

    onSourceChanged: sync()
    Component.onCompleted: { sync(); Qt.callLater(function () { keyed.ready = true }) }
}
