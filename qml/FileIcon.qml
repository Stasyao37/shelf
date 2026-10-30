import QtQuick
import Shelf

// Стеклянная иконка файла, в том же стиле, что FolderIcon. У каждого
// типа своя картинка из icons/ (большую часть рисует tools/gen_icons.py),
// архивы — папка со стеклянной молнией; неизвестным типам — пустой лист.
Item {
    id: root

    property string mimeType: ""
    // Нужно для конфигов: у .conf, .ini, .cfg, .env нет своего MIME-типа, они text/plain
    property string fileName: ""
    property int size: 48
    // Под какой размер рисовать картинки. Во время зума size меняется каждый кадр,
    // а renderSize — сразу целевой: картинка лишь масштабируется, а не рисуется заново
    property int renderSize: size

    width: size
    height: size

    // [картинка из icons/, эмблема из icons/emblems (во весь размер) или lang/<язык>]
    readonly property var kind: classify(mimeType, fileName)
    // Группа для оттенка палитры (Theme.iconTint)
    readonly property string group: ["document", "pdf", "book", "spreadsheet", "presentation", "html"].includes(kind[0]) ? "doc"
        : ["image", "picture", "video", "audio", "font"].includes(kind[0]) ? "media" : "other"

    // Код на известном языке — пустой лист с логотипом языка (icons/lang, Simple Icons, CC0).
    // По имени, а не по MIME: у многих языков своего типа нет, а .ts Qt считает видео
    readonly property var langByExt: ({
        py: "python", pyw: "python", pyi: "python",
        js: "javascript", mjs: "javascript", cjs: "javascript",
        ts: "typescript", mts: "typescript", cts: "typescript", jsx: "react", tsx: "react",
        c: "c", h: "c", cpp: "cplusplus", cxx: "cplusplus", cc: "cplusplus",
        hpp: "cplusplus", hxx: "cplusplus", hh: "cplusplus", cs: "dotnet",
        rs: "rust", go: "go", java: "java", kt: "kotlin", kts: "kotlin",
        php: "php", rb: "ruby", lua: "lua", qml: "qt", dart: "dart", swift: "swift",
        hs: "haskell", zig: "zig", nim: "nim", r: "r", jl: "julia", ex: "elixir", exs: "elixir",
        scala: "scala", pl: "perl", pm: "perl", clj: "clojure", ml: "ocaml",
        f90: "fortran", f95: "fortran", sol: "solidity", graphql: "graphql", gql: "graphql",
        css: "css", scss: "sass", sass: "sass", less: "less", json: "json",
        vue: "vuedotjs", svelte: "svelte", nix: "nixos", cmake: "cmake"
    })
    readonly property var langByName: ({
        "Dockerfile": "docker", "Containerfile": "docker", "CMakeLists.txt": "cmake"
    })

    function classify(m, name) {
        function has(re) { return re.test(m) }
        const dot = name.lastIndexOf(".")
        const lang = langByName[name] || (dot > 0 ? langByExt[name.slice(dot + 1).toLowerCase()] : undefined)
        if (lang)                                                    return ["file", "lang/" + lang]
        // Книги — раньше картинок: djvu это image/vnd.djvu
        if (has(/epub|fictionbook|djvu|mobipocket/))                 return ["book"]
        if (m === "image/png" || m === "image/svg+xml")              return ["picture"]
        if (m.startsWith("image/"))                                  return ["image"]
        if (m.startsWith("audio/"))                                  return ["audio"]
        if (m.startsWith("video/"))                                  return ["video"]
        // .otf ещё и шаблон формулы OpenDocument с тем же весом — Qt выбирает его
        if (m.startsWith("font/") || has(/font-|x-font/) || /\.(otf|ttf|ttc|woff2?)$/i.test(name))
                                                                     return ["font"]
        if (m === "application/pdf")                                 return ["pdf"]
        if (has(/spreadsheet|excel|text\/csv/))                      return ["spreadsheet"]
        if (has(/presentation|powerpoint|keynote/))                  return ["presentation"]
        if (has(/opendocument\.text|msword|wordprocessing|rtf/))     return ["document"]
        if (has(/x-iso9660|cd-image|efi\.i(so|mg)|disk-image|raw-disk|x-qemu/)) return ["disc"]
        // Пакеты — раньше архивов: у apk тип package-archive
        if (has(/x-rpm|x-deb|debian|flatpak|android\.package|x-msi/)) return ["package"]
        if (has(/zip|tar|gzip|bzip|x-xz|zstd|7z|rar|compress|archive|lzma|lz4|cpio/))
                                                                     return ["folder", "archive"]
        if (has(/x-ms-dos-executable|x-msdownload|x-ms-ne/))        return ["exe"]
        if (has(/x-executable|x-sharedlib|x-pie-executable|appimage|x-object/))
                                                                     return ["executable"]
        if (has(/x-shellscript/))                                    return ["script"]
        if (has(/x-desktop|toml|yaml|systemd-unit|dbus-service/) || /\.(conf|ini|cfg|env|properties)$|^\.\w+rc$/.test(name))
                                                                     return ["config"]
        if (has(/pkix-cert|x509|pkcs|x-pem/))                        return ["certificate"]
        if (has(/pgp|ssh-key/))                                      return ["key"]
        if (has(/sqlite|database|x-dbf|msaccess/))                   return ["database"]
        if (has(/x-readme|markdown|x-log|^text\/plain/))             return ["document"]
        if (m === "text/html" || has(/xhtml/))                       return ["html"]
        if (has(/^text\/x-|javascript|typescript|json|xml|yaml|toml|x-qml|text\/css|sql|x-python|x-perl|x-ruby|x-php/))
                                                                     return ["code"]
        if (m.startsWith("text/"))                                   return ["document"]
        return ["file"]
    }

    GlassImage {
        anchors.fill: parent
        source: Theme.glassIcon(root.kind[0], Theme.groupTint(root.group))
        sourceSize: Qt.size(root.renderSize, root.renderSize)
    }

    readonly property bool isLang: kind.length > 1 && kind[1].startsWith("lang/")

    GlassEmblem {
        // Логотип языка меньше 32 px не разобрать — как и эмблемы папок
        visible: root.kind.length > 1 && (!root.isLang || root.size >= 32)
        // Логотип — на листе ниже загнутого угла; эмблема архива — во весь размер
        width: root.isLang ? Math.round(root.size * 0.4) : root.size
        height: width
        renderWidth: root.isLang ? Math.round(root.renderSize * 0.4) : root.renderSize
        x: (root.size - width) / 2
        y: root.isLang ? root.size * 0.6 - height / 2 : 0
        name: !visible ? "" : root.isLang ? root.kind[1] : "emblems/" + root.kind[1]
        tint: Theme.inkTint
        // У эмблемы во весь размер линии мельче — тень ниже
        depthRatio: root.isLang ? 1 / 22 : 1 / 48
    }
}
