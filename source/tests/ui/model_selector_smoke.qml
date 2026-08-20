import QtQuick 2.15
import QtQuick.Window 2.15
import "qrc:/Component"

Window {
    id: host
    width: 420
    height: 320
    visible: true
    property bool passed: false
    property string failure: "not completed"
    property int stage: 0

    DeepSeekModelSelector {
        id: selector
        objectName: "selectorUnderTest"
        x: 24
        y: 250
        width: 260
        fontFamily: "Segoe UI"
    }

    Timer {
        interval: 80
        repeat: true
        running: true
        onTriggered: {
            if (host.stage === 0) {
                selector.openPickerForSmoke()
                host.stage = 1
                return
            }
            if (host.stage === 1) {
                if (!selector.pickerVisible || selector.filteredModelCount !== 50
                        || !selector.modelListScrollable
                        || !selector.reasoningOptionsVisible
                        || !selector.popupFitsAvailableGeometry) {
                    host.failure = "50-model layout or placement contract failed"
                    running = false
                    return
                }
                selector.setSearchForSmoke("does-not-exist")
                host.stage = 2
                return
            }
            if (host.stage === 2) {
                if (selector.filteredModelCount !== 0 || !selector.emptyStateVisible) {
                    host.failure = "search empty state failed"
                    running = false
                    return
                }
                selector.closePickerForSmoke()
                host.stage = 3
                return
            }
            if (host.stage === 3) {
                if (selector.pickerVisible) {
                    host.failure = "first close failed"
                    running = false
                    return
                }
                selector.openPickerForSmoke()
                host.stage = 4
                return
            }
            if (host.stage === 4) {
                host.passed = selector.pickerVisible
                        && selector.filteredModelCount === 50
                        && selector.reasoningOptionsVisible
                        && selector.popupFitsAvailableGeometry
                host.failure = host.passed ? "" : "second open failed"
                selector.closePickerForSmoke()
                running = false
            }
        }
    }
}
