import QtQuick
import Shelf

// Картинка значка (image://glass). При запуске — сразу, чтобы окно открывалось со значками.
// Потом — в фоне: при зуме новые размеры рисуются не на кадре интерфейса, а пока
// рисуются, видна прежняя картинка, растянутая, — без мигания (retainWhileLoading, Qt 6.8+)
Image {
    asynchronous: Theme.imagesInBackground
    smooth: true
    Component.onCompleted: if ("retainWhileLoading" in this) retainWhileLoading = true
}
