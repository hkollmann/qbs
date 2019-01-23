QtApplication {
    name: "qmlapp"
    Depends { name: "Qt.quick" }
    Depends { name: "Qt.android_support" }
    Properties {
        condition: qbs.targetOS.contains("android")
        Qt.android_support.extraPrefixDirs: path
    }
    property stringList qmlImportPaths: path
    files: [
        "main.cpp",
        "qml.qrc",
    ]
}