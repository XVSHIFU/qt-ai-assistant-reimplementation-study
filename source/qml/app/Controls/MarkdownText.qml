import QtQuick 2.15
import QtQuick.Controls 2.15

// A deliberately small, safe Markdown renderer for chat content.
//
// Qt 5.15's native MarkdownText supports the right syntax, but code blocks and
// quotes are intentionally unstyled.  Rendering an escaped, known subset to
// Qt RichText gives those structures a useful visual hierarchy without ever
// passing model-provided HTML or <img> tags to QTextDocument.
Item {
    id: control

    property real scaleFactor: window.scale
    property bool markdown: true
    property bool streaming: false
    property int streamRenderInterval: 80
    property string sourceText: ""
    property string uiFontFamily: fontManager.item.uiFontFamily
    property string codeFontFamily: fontManager.item.codeFontFamily
    property font font: Qt.font({ family: uiFontFamily, pixelSize: 14 * scaleFactor })
    property color color: theme.item.colorB1
    property color codeBackgroundColor: "#F5F6F8"
    property color codeBorderColor: "#E1E4E8"
    property color quoteBackgroundColor: "#F7F7FA"
    property color quoteAccentColor: theme.item.themeOneColor3
    property color mutedTextColor: theme.item.colorW7
    property color codeKeywordColor: "#7C3AED"
    property color codeTypeColor: "#2563EB"
    property color codeStringColor: "#087F5B"
    property color codeNumberColor: "#B45309"
    property color codeCommentColor: "#6B7280"
    property color codeFunctionColor: "#0369A1"
    readonly property string renderedText: _renderedText
    readonly property int renderPassCount: _renderPassCount
    readonly property int markdownParseCount: _markdownParseCount
    readonly property int streamFlushCount: _streamFlushCount
    readonly property int codeBlockCount: _codeBlocks.length
    readonly property int tableBlockCount: _tableBlockCount
    readonly property int headingBlockCount: _headingBlockCount
    readonly property bool codeHorizontalOverflow: hasOverflowBlock("code")
    readonly property bool tableHorizontalOverflow: hasOverflowBlock("table")
    readonly property string lastCopiedCode: _lastCopiedCode
    readonly property int copiedCodeIndex: _copiedCodeIndex
    readonly property bool structuredBlockRendering: true
    readonly property real contentHeight: markdown && !streaming
                                          ? markdownColumn.implicitHeight
                                          : plainText.contentHeight
    readonly property real contentWidth: markdown && !streaming
                                         ? width : plainText.contentWidth
    implicitHeight: Math.ceil(contentHeight)

    property string _renderedText: ""
    property var _blocks: []
    property var _codeBlocks: []
    property int _tableBlockCount: 0
    property int _headingBlockCount: 0
    property int _renderPassCount: 0
    property int _markdownParseCount: 0
    property int _streamFlushCount: 0
    property bool _streamDirty: false
    property string _lastCopiedCode: ""
    property int _copiedCodeIndex: -1

    signal codeCopied(int index, string language)

    function requestRender() {
        if (streaming) {
            _streamDirty = true
            if (!streamRenderTimer.running)
                streamRenderTimer.start()
        } else {
            flushNow()
        }
    }

    function flushNow() {
        streamRenderTimer.stop()
        _streamDirty = false
        ++_renderPassCount
        if (markdown) {
            ++_markdownParseCount
            _renderedText = renderMarkdown(sourceText)
        } else {
            _blocks = []
            _codeBlocks = []
            _tableBlockCount = 0
            _headingBlockCount = 0
            _renderedText = sourceText
        }
    }

    function flushStreamingText() {
        if (!_streamDirty)
            return
        _streamDirty = false
        ++_renderPassCount
        ++_streamFlushCount
        // Streaming intentionally stays plain text. This avoids reparsing the
        // complete growing answer on every delta; the terminal transition below
        // performs one immediate, complete Markdown render.
        _renderedText = sourceText
    }

    function copyCodeBlockAt(index) {
        if (index < 0 || index >= _codeBlocks.length)
            return false
        var block = _codeBlocks[index]
        _lastCopiedCode = block.code
        _copiedCodeIndex = index
        clipboardText.text = block.code
        clipboardText.selectAll()
        clipboardText.copy()
        codeCopied(index, block.language)
        copiedFeedbackTimer.restart()
        return true
    }

    function activateLink(link) {
        var normalized = safeHref(link)
        if (normalized !== "")
            Qt.openUrlExternally(normalized)
    }

    function hasOverflowBlock(kind) {
        for (var index = 0; index < _blocks.length; ++index) {
            var block = _blocks[index]
            if (block.kind === kind && block.preferredWidth > width)
                return true
        }
        return false
    }

    function headingPixelSize(level) {
        var normalizedLevel = Math.max(1, Math.min(6, Number(level) || 6))
        var factors = [1.55, 1.36, 1.20, 1.08, 1.0, 0.94]
        return Math.round(font.pixelSize * factors[normalizedLevel - 1])
    }

    onSourceTextChanged: requestRender()
    onMarkdownChanged: flushNow()
    onStreamingChanged: {
        if (streaming)
            requestRender()
        else
            flushNow()
    }
    onColorChanged: if (!streaming && markdown) requestRender()
    onUiFontFamilyChanged: if (!streaming && markdown) requestRender()
    onCodeFontFamilyChanged: if (!streaming && markdown) requestRender()
    onCodeBackgroundColorChanged: if (!streaming && markdown) requestRender()
    onCodeBorderColorChanged: if (!streaming && markdown) requestRender()
    onQuoteBackgroundColorChanged: if (!streaming && markdown) requestRender()
    onQuoteAccentColorChanged: if (!streaming && markdown) requestRender()
    onMutedTextColorChanged: if (!streaming && markdown) requestRender()
    Component.onCompleted: flushNow()

    Timer {
        id: streamRenderTimer
        interval: Math.max(60, Math.min(100, control.streamRenderInterval))
        repeat: false
        onTriggered: control.flushStreamingText()
    }

    Timer {
        id: copiedFeedbackTimer
        interval: 1500
        repeat: false
        onTriggered: control._copiedCodeIndex = -1
    }

    TextEdit {
        id: clipboardText
        visible: false
        readOnly: false
    }

    TextEdit {
        id: plainText
        visible: !control.markdown || control.streaming
        width: control.width
        height: Math.ceil(contentHeight)
        readOnly: true
        selectByMouse: true
        persistentSelection: true
        wrapMode: TextEdit.Wrap
        textFormat: TextEdit.PlainText
        text: control.renderedText
        color: control.color
        font: control.font
        selectionColor: theme.item.themeZeroColor
        selectedTextColor: "white"
        renderType: Text.QtRendering
    }

    Column {
        id: markdownColumn
        visible: control.markdown && !control.streaming
        width: control.width
        spacing: 7 * control.scaleFactor

        Repeater {
            model: control._blocks
            delegate: Loader {
                width: markdownColumn.width
                height: item ? item.height : 0
                property var blockData: modelData
                property int blockIndex: index
                sourceComponent: blockData.kind === "code" ? codeBlockComponent
                                 : blockData.kind === "table" ? tableBlockComponent
                                 : blockData.kind === "heading" ? headingBlockComponent
                                 : richBlockComponent
            }
        }
    }

    Component {
        id: headingBlockComponent
        Item {
            width: parent.width
            readonly property var blockSpec: parent.blockData
            readonly property int headingLevel: Math.max(1, Math.min(6, blockSpec.level))
            readonly property real topInset: (headingLevel <= 2 ? 8 : 5) * control.scaleFactor
            readonly property real bottomInset: (headingLevel <= 3 ? 4 : 2) * control.scaleFactor
            height: Math.ceil(topInset + headingText.contentHeight + bottomInset)

            TextEdit {
                id: headingText
                objectName: "markdownHeadingLevel" + parent.headingLevel
                y: parent.topInset
                width: parent.width
                height: Math.ceil(contentHeight)
                readOnly: true
                selectByMouse: true
                persistentSelection: true
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.RichText
                text: parent.blockSpec.html
                color: control.color
                font.family: control.uiFontFamily
                font.pixelSize: control.headingPixelSize(parent.headingLevel)
                font.weight: parent.headingLevel <= 2 ? Font.Bold : Font.DemiBold
                selectionColor: theme.item.themeZeroColor
                selectedTextColor: "white"
                renderType: Text.QtRendering
                onLinkActivated: control.activateLink(link)
            }
        }
    }

    Component {
        id: richBlockComponent
        TextEdit {
            width: parent.width
            height: Math.ceil(contentHeight)
            readOnly: true
            selectByMouse: true
            persistentSelection: true
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.RichText
            text: "<html><body style=\"font-family:'" + control.escapeHtml(control.uiFontFamily)
                  + "'; color:" + String(control.color) + ";\">"
                  + parent.blockData.html + "</body></html>"
            color: control.color
            font: control.font
            selectionColor: theme.item.themeZeroColor
            selectedTextColor: "white"
            renderType: Text.QtRendering
            onLinkActivated: control.activateLink(link)
        }
    }

    Component {
        id: codeBlockComponent
        Item {
            id: codeBlock
            readonly property var blockSpec: parent.blockData
            readonly property int dataIndex: blockSpec.copyIndex
            width: parent.width
            height: codeHeader.height + codeViewport.height

            Rectangle {
                id: codeHeader
                width: parent.width
                height: 30 * control.scaleFactor
                color: "#ECEFF3"
                radius: 5 * control.scaleFactor

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10 * control.scaleFactor
                    anchors.verticalCenter: parent.verticalCenter
                    text: codeBlock.blockSpec.language === ""
                          ? qsTr("代码") : codeBlock.blockSpec.language
                    color: control.mutedTextColor
                    font.pixelSize: 11 * control.scaleFactor
                    font.bold: true
                    font.family: control.uiFontFamily
                }

                Button {
                    id: copyCodeButton
                    objectName: "markdownCodeCopyButton"
                    property bool copied: control.copiedCodeIndex === codeBlock.dataIndex
                    anchors.right: parent.right
                    anchors.rightMargin: 6 * control.scaleFactor
                    anchors.verticalCenter: parent.verticalCenter
                    height: 24 * control.scaleFactor
                    width: copyLabel.implicitWidth + 16 * control.scaleFactor
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6 * control.scaleFactor
                        color: copyCodeButton.copied ? "#DDF4E6"
                              : copyCodeButton.hovered ? "#E2E5EA" : "transparent"
                    }
                    contentItem: Text {
                        id: copyLabel
                        text: copyCodeButton.copied ? qsTr("✓ 已复制") : qsTr("复制")
                        color: copyCodeButton.copied ? "#18794E" : control.mutedTextColor
                        font.pixelSize: 11 * control.scaleFactor
                        font.family: control.uiFontFamily
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: control.copyCodeBlockAt(codeBlock.dataIndex)
                    Accessible.name: copied ? qsTr("代码已复制") : qsTr("复制代码")
                }
            }

            Flickable {
                id: codeViewport
                objectName: "markdownCodeHorizontalScroll"
                anchors.top: codeHeader.bottom
                width: parent.width
                height: Math.ceil(codeText.contentHeight) + 20 * control.scaleFactor
                contentWidth: Math.max(width, codeText.contentWidth + 24 * control.scaleFactor)
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentWidth > width

                Rectangle {
                    width: Math.max(codeViewport.width, codeViewport.contentWidth)
                    height: codeViewport.height
                    color: control.codeBackgroundColor
                }
                TextEdit {
                    id: codeText
                    x: 12 * control.scaleFactor
                    y: 10 * control.scaleFactor
                    width: Math.max(codeViewport.width - 24 * control.scaleFactor,
                                    codeBlock.blockSpec.preferredWidth)
                    readOnly: true
                    selectByMouse: true
                    persistentSelection: true
                    wrapMode: TextEdit.NoWrap
                    textFormat: TextEdit.RichText
                    text: "<pre style=\"margin:0; white-space:pre; font-family:'"
                          + control.escapeHtml(control.codeFontFamily) + "'; color:"
                          + String(control.color) + ";\">" + codeBlock.blockSpec.html
                          + "</pre>"
                    color: control.color
                    font.pixelSize: control.font.pixelSize
                    font.family: control.codeFontFamily
                    selectionColor: theme.item.themeZeroColor
                    selectedTextColor: "white"
                    renderType: Text.QtRendering
                }
                ScrollBar.horizontal: ScrollBar {
                    policy: codeViewport.contentWidth > codeViewport.width
                            ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }
            }
        }
    }

    Component {
        id: tableBlockComponent
        Flickable {
            id: tableViewport
            readonly property var blockSpec: parent.blockData
            objectName: "markdownTableHorizontalScroll"
            width: parent.width
            height: Math.ceil(tableText.contentHeight) + 4 * control.scaleFactor
            contentWidth: Math.max(width, tableText.contentWidth)
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentWidth > width

            TextEdit {
                id: tableText
                width: Math.max(tableViewport.width, tableViewport.blockSpec.preferredWidth)
                height: Math.ceil(contentHeight)
                readOnly: true
                selectByMouse: true
                persistentSelection: true
                wrapMode: TextEdit.NoWrap
                textFormat: TextEdit.RichText
                text: "<html><body style=\"font-family:'"
                      + control.escapeHtml(control.uiFontFamily) + "'; color:"
                      + String(control.color) + "; white-space:nowrap;\">"
                      + tableViewport.blockSpec.html + "</body></html>"
                color: control.color
                font: control.font
                selectionColor: theme.item.themeZeroColor
                selectedTextColor: "white"
                renderType: Text.QtRendering
                onLinkActivated: control.activateLink(link)
            }
            ScrollBar.horizontal: ScrollBar {
                policy: parent.contentWidth > parent.width
                        ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }
        }
    }

    function escapeHtml(value) {
        return String(value === undefined || value === null ? "" : value)
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
                .replace(/\"/g, "&quot;")
                .replace(/'/g, "&#39;")
    }

    function safeHref(value) {
        var normalized = String(value || "").trim()
        var lower = normalized.toLowerCase()
        if (lower.indexOf("https://") === 0 || lower.indexOf("http://") === 0
                || lower.indexOf("mailto:") === 0)
            return normalized
        return ""
    }

    function inlineMarkdown(value) {
        var work = String(value || "").replace(/\uE000/g, "&#57344;")
        var held = []

        function hold(html) {
            var token = "\uE000" + held.length + "\uE001"
            held.push(html)
            return token
        }

        // Protect code and links before applying emphasis.  All captured text
        // is escaped here; placeholders are the only markup that is restored.
        work = work.replace(/(`+)([\s\S]*?)\1/g, function(all, ticks, code) {
            var family = control.escapeHtml(control.codeFontFamily)
            return hold("<span style=\"font-family:'" + family
                        + "'; background-color:" + String(control.codeBackgroundColor)
                        + "; color:" + String(control.color)
                        + ";\">&#160;" + control.escapeHtml(code) + "&#160;</span>")
        })
        work = work.replace(/!\[([^\]\n]*)\]\(([^)\n]+)\)/g,
                            function(all, label, destination) {
            var href = control.safeHref(destination.replace(/\s+[\"'][^\"']*[\"']\s*$/, ""))
            var caption = "图片：" + (label || "未命名")
            if (href === "")
                return hold("<span style=\"color:" + String(control.mutedTextColor)
                            + ";\">[" + control.escapeHtml(caption) + "]</span>")
            return hold("<a href=\"" + control.escapeHtml(href) + "\">["
                        + control.escapeHtml(caption) + "]</a>")
        })
        work = work.replace(/\[([^\]\n]+)\]\(([^)\n]+)\)/g,
                            function(all, label, destination) {
            var href = control.safeHref(destination.replace(/\s+[\"'][^\"']*[\"']\s*$/, ""))
            if (href === "")
                return hold(control.escapeHtml(label) + " <span style=\"color:"
                            + String(control.mutedTextColor) + ";\">(链接已拦截)</span>")
            return hold("<a href=\"" + control.escapeHtml(href) + "\">"
                        + control.escapeHtml(label) + "</a>")
        })

        work = control.escapeHtml(work)
        work = work.replace(/\*\*([^*\n]+)\*\*/g, "<b>$1</b>")
        work = work.replace(/__([^_\n]+)__/g, "<b>$1</b>")
        work = work.replace(/~~([^~\n]+)~~/g, "<s>$1</s>")
        work = work.replace(/(^|[^*])\*([^*\n]+)\*/g, "$1<i>$2</i>")
        work = work.replace(/(^|[^_])_([^_\n]+)_/g, "$1<i>$2</i>")

        work = work.replace(/\uE000(\d+)\uE001/g, function(all, index) {
            return held[Number(index)] || ""
        })
        return work
    }

    function splitTableRow(line) {
        var text = String(line || "").trim()
        if (text.charAt(0) === "|")
            text = text.substring(1)
        if (text.charAt(text.length - 1) === "|")
            text = text.substring(0, text.length - 1)
        var cells = []
        var cell = ""
        var escaped = false
        var code = false
        for (var i = 0; i < text.length; ++i) {
            var ch = text.charAt(i)
            if (escaped) {
                cell += ch
                escaped = false
            } else if (ch === "\\") {
                escaped = true
                cell += ch
            } else if (ch === "`") {
                code = !code
                cell += ch
            } else if (ch === "|" && !code) {
                cells.push(cell.trim())
                cell = ""
            } else {
                cell += ch
            }
        }
        cells.push(cell.trim())
        return cells
    }

    function isTableDivider(line) {
        var cells = splitTableRow(line)
        if (cells.length === 0)
            return false
        for (var i = 0; i < cells.length; ++i) {
            if (!/^:?-{3,}:?$/.test(cells[i].replace(/\s/g, "")))
                return false
        }
        return true
    }

    function fenceMatch(line) {
        return /^\s{0,3}(`{3,}|~{3,})\s*([^`]*)$/.exec(line)
    }

    function listMatch(line) {
        return /^(\s{0,3})([-+*]|\d+[.)])\s+(.+)$/.exec(line)
    }

    function headingMatch(line) {
        // Accept CommonMark ATX headings and the frequently used compact CJK
        // form ("##标题"). Closing marker hashes are removed only when they
        // are separated from the title by whitespace.
        var match = /^\s{0,3}(#{1,6})(.*)$/.exec(String(line || ""))
        if (!match || /^#/.test(match[2]))
            return null
        var remainder = match[2]
        if (remainder !== "" && !/^[ \t]/.test(remainder)) {
            // Lenient compact form: the first title character follows '#'.
        } else {
            remainder = remainder.replace(/^[ \t]+/, "")
        }
        remainder = remainder.replace(/[ \t]+#+[ \t]*$/, "").replace(/[ \t]+$/, "")
        return { level: match[1].length, text: remainder }
    }

    function isHorizontalRule(line) {
        var compact = String(line || "").trim().replace(/\s/g, "")
        return /^(\*{3,}|-{3,}|_{3,})$/.test(compact)
    }

    function isBlockStart(lines, index) {
        var line = lines[index]
        if (line === undefined || /^\s*$/.test(line))
            return true
        if (fenceMatch(line) || /^\s{0,3}>/.test(line) || /^\s{4}|^\t/.test(line)
                || headingMatch(line) || listMatch(line)
                || isHorizontalRule(line))
            return true
        return index + 1 < lines.length && line.indexOf("|") >= 0
                && isTableDivider(lines[index + 1])
    }

    function renderCodeBlock(code, language) {
        var label = String(language || "").trim().split(/\s+/)[0]
        var family = escapeHtml(codeFontFamily)
        var highlighted = highlightCode(code, label)
        var header = label === "" ? qsTr("代码") : label
        var html = "<table width=\"100%\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\""
                + " bgcolor=\"" + String(codeBackgroundColor) + "\">"
                + "<tr><td bgcolor=\"#ECEFF3\" style=\"padding:5px 10px; color:"
                + String(mutedTextColor) + "; font-size:11px;\"><b>"
                + escapeHtml(header) + "</b></td></tr><tr><td style=\"padding:10px 12px;\">"
                + "<pre style=\"margin:0; white-space:pre; font-family:'" + family
                + "'; color:" + String(color) + ";\">" + highlighted
                + "</pre></td></tr></table>"
        return html
    }

    function highlightCode(value, language) {
        var lines = String(value || "").split("\n")
        var result = []
        var inBlockComment = false
        var keywordSet = "|as|async|await|break|case|catch|class|const|continue|def|delete|do|else|enum|export|extends|finally|for|from|function|if|import|in|interface|lambda|let|namespace|new|of|package|private|protected|public|raise|return|static|struct|switch|template|throw|try|typedef|typename|using|var|while|with|yield|"
        var typeSet = "|auto|bool|boolean|byte|char|double|float|int|long|number|object|short|string|QString|QByteArray|QObject|QVariant|unsigned|void|"
        var constantSet = "|false|null|None|nullptr|true|undefined|False|True|"
        var lowerLanguage = String(language || "").toLowerCase()
        var hashComments = /^(py|python|rb|ruby|sh|shell|bash|zsh|yaml|yml|toml|powershell|ps1)$/.test(lowerLanguage)

        function span(text, spanColor) {
            return "<span style=\"color:" + spanColor + ";\">"
                    + control.escapeHtml(text) + "</span>"
        }

        for (var lineIndex = 0; lineIndex < lines.length; ++lineIndex) {
            var line = lines[lineIndex]
            var html = ""
            var index = 0
            while (index < line.length) {
                if (inBlockComment) {
                    var blockEnd = line.indexOf("*/", index)
                    if (blockEnd < 0) {
                        html += span(line.substring(index), String(codeCommentColor))
                        index = line.length
                        continue
                    }
                    html += span(line.substring(index, blockEnd + 2), String(codeCommentColor))
                    index = blockEnd + 2
                    inBlockComment = false
                    continue
                }

                if (line.substr(index, 2) === "/*") {
                    var sameLineEnd = line.indexOf("*/", index + 2)
                    if (sameLineEnd < 0) {
                        html += span(line.substring(index), String(codeCommentColor))
                        inBlockComment = true
                        index = line.length
                    } else {
                        html += span(line.substring(index, sameLineEnd + 2), String(codeCommentColor))
                        index = sameLineEnd + 2
                    }
                    continue
                }
                if (line.substr(index, 2) === "//" || (hashComments && line.charAt(index) === "#")) {
                    html += span(line.substring(index), String(codeCommentColor))
                    break
                }

                var character = line.charAt(index)
                if (character === "\"" || character === "'" || character === "`") {
                    var quote = character
                    var stringEnd = index + 1
                    while (stringEnd < line.length) {
                        if (line.charAt(stringEnd) === "\\") {
                            stringEnd += 2
                            continue
                        }
                        if (line.charAt(stringEnd) === quote) {
                            ++stringEnd
                            break
                        }
                        ++stringEnd
                    }
                    html += span(line.substring(index, stringEnd), String(codeStringColor))
                    index = stringEnd
                    continue
                }

                var number = /^(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)/.exec(line.substring(index))
                if (number && (index === 0 || !/[A-Za-z0-9_$]/.test(line.charAt(index - 1)))) {
                    html += span(number[0], String(codeNumberColor))
                    index += number[0].length
                    continue
                }

                var word = /^[A-Za-z_$][A-Za-z0-9_$]*/.exec(line.substring(index))
                if (word) {
                    var token = word[0]
                    var tokenColor = ""
                    if (keywordSet.indexOf("|" + token + "|") >= 0)
                        tokenColor = String(codeKeywordColor)
                    else if (typeSet.indexOf("|" + token + "|") >= 0)
                        tokenColor = String(codeTypeColor)
                    else if (constantSet.indexOf("|" + token + "|") >= 0)
                        tokenColor = String(codeNumberColor)
                    else {
                        var tail = line.substring(index + token.length)
                        if (/^\s*\(/.test(tail))
                            tokenColor = String(codeFunctionColor)
                    }
                    html += tokenColor === "" ? escapeHtml(token) : span(token, tokenColor)
                    index += token.length
                    continue
                }

                html += escapeHtml(character)
                ++index
            }
            result.push(html)
        }
        return result.join("\n")
    }

    function renderMarkdown(value) {
        var source = String(value === undefined || value === null ? "" : value)
                .replace(/\r\n?/g, "\n")
        if (source === "") {
            _blocks = []
            _codeBlocks = []
            _tableBlockCount = 0
            _headingBlockCount = 0
            return ""
        }

        var lines = source.split("\n")
        var output = []
        var blocks = []
        var codeBlocks = []
        var tableCount = 0
        var headingCount = 0

        function estimatedCodeWidth(code) {
            var codeLines = String(code || "").split("\n")
            var longest = 0
            for (var lineIndex = 0; lineIndex < codeLines.length; ++lineIndex)
                longest = Math.max(longest, codeLines[lineIndex].length)
            return Math.max(180 * control.scaleFactor,
                            longest * control.font.pixelSize * 0.68
                            + 28 * control.scaleFactor)
        }

        function estimatedTableWidth(headers, rows) {
            var totalCharacters = 0
            for (var column = 0; column < headers.length; ++column) {
                var longest = String(headers[column] || "").length
                for (var row = 0; row < rows.length; ++row)
                    longest = Math.max(longest, String(rows[row][column] || "").length)
                totalCharacters += Math.max(5, longest)
            }
            return Math.max(220 * control.scaleFactor,
                            totalCharacters * control.font.pixelSize * 0.62
                            + headers.length * 30 * control.scaleFactor)
        }

        function appendRich(html) {
            output.push(html)
            if (blocks.length > 0 && blocks[blocks.length - 1].kind === "rich")
                blocks[blocks.length - 1].html += html
            else
                blocks.push({ kind: "rich", html: html })
        }

        function appendCode(code, language) {
            var normalizedLanguage = String(language || "").trim().split(/\s+/)[0]
            var copyIndex = codeBlocks.length
            output.push(renderCodeBlock(code, normalizedLanguage))
            blocks.push({
                kind: "code",
                html: highlightCode(code, normalizedLanguage),
                code: code,
                language: normalizedLanguage,
                copyIndex: copyIndex,
                preferredWidth: estimatedCodeWidth(code)
            })
            codeBlocks.push({ code: code, language: normalizedLanguage })
        }

        function appendTable(html, headers, rows) {
            output.push(html)
            blocks.push({
                kind: "table",
                html: html,
                preferredWidth: estimatedTableWidth(headers, rows)
            })
            ++tableCount
        }

        function appendHeading(text, level) {
            var html = inlineMarkdown(text)
            output.push("<h" + level + ">" + html + "</h" + level + ">")
            blocks.push({ kind: "heading", level: level, html: html })
            ++headingCount
        }

        var i = 0
        while (i < lines.length) {
            var line = lines[i]
            if (/^\s*$/.test(line)) {
                ++i
                continue
            }

            var fence = fenceMatch(line)
            if (fence) {
                var marker = fence[1]
                var markerChar = marker.charAt(0)
                var minimumLength = marker.length
                var codeLines = []
                ++i
                while (i < lines.length) {
                    var closing = new RegExp("^\\s{0,3}" + markerChar + "{" + minimumLength
                                             + ",}\\s*$")
                    if (closing.test(lines[i])) {
                        ++i
                        break
                    }
                    codeLines.push(lines[i])
                    ++i
                }
                appendCode(codeLines.join("\n"), fence[2])
                continue
            }

            if (/^\s{4}|^\t/.test(line)) {
                var indented = []
                while (i < lines.length
                       && (/^\s{4}|^\t/.test(lines[i]) || /^\s*$/.test(lines[i]))) {
                    indented.push(lines[i].replace(/^(?: {4}|\t)/, ""))
                    ++i
                }
                while (indented.length > 0 && indented[indented.length - 1] === "")
                    indented.pop()
                appendCode(indented.join("\n"), "")
                continue
            }

            if (i + 1 < lines.length && line.indexOf("|") >= 0
                    && isTableDivider(lines[i + 1])) {
                var headers = splitTableRow(line)
                i += 2
                var rows = []
                while (i < lines.length && lines[i].indexOf("|") >= 0
                       && !/^\s*$/.test(lines[i])) {
                    rows.push(splitTableRow(lines[i]))
                    ++i
                }
                var table = "<table border=\"1\" cellspacing=\"0\""
                        + " cellpadding=\"7\" bordercolor=\"" + String(codeBorderColor) + "\">"
                        + "<tr bgcolor=\"#F3F4F6\">"
                var column
                for (column = 0; column < headers.length; ++column)
                    table += "<td><b>" + inlineMarkdown(headers[column]) + "</b></td>"
                table += "</tr>"
                for (var row = 0; row < rows.length; ++row) {
                    table += "<tr bgcolor=\"" + (row % 2 === 0 ? "#FFFFFF" : "#FAFAFB") + "\">"
                    for (column = 0; column < headers.length; ++column)
                        table += "<td>" + inlineMarkdown(rows[row][column] || "") + "</td>"
                    table += "</tr>"
                }
                table += "</table>"
                appendTable(table, headers, rows)
                continue
            }

            if (/^\s{0,3}>/.test(line)) {
                var quote = []
                while (i < lines.length && /^\s{0,3}>/.test(lines[i])) {
                    quote.push(lines[i].replace(/^\s{0,3}>\s?/, ""))
                    ++i
                }
                appendRich("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\""
                            + " bgcolor=\"" + String(quoteBackgroundColor) + "\"><tr>"
                            + "<td width=\"3\" bgcolor=\"" + String(quoteAccentColor)
                            + "\"></td><td style=\"padding:8px 10px; color:"
                            + String(mutedTextColor) + ";\">"
                            + quote.map(function(part) { return inlineMarkdown(part) }).join("<br/>")
                            + "</td></tr></table>")
                continue
            }

            var heading = headingMatch(line)
            if (heading) {
                appendHeading(heading.text, heading.level)
                ++i
                continue
            }

            if (isHorizontalRule(line)) {
                appendRich("<hr style=\"color:" + String(codeBorderColor) + ";\"/>")
                ++i
                continue
            }

            var list = listMatch(line)
            if (list) {
                var ordered = /^\d/.test(list[2])
                var tag = ordered ? "ol" : "ul"
                var listHtml = "<" + tag + " style=\"margin:4px 0 6px 0;\">"
                while (i < lines.length) {
                    var item = listMatch(lines[i])
                    if (!item || /^\d/.test(item[2]) !== ordered)
                        break
                    var body = item[3]
                    var task = /^\[([ xX])\]\s+(.*)$/.exec(body)
                    if (task)
                        body = (task[1].toLowerCase() === "x" ? "☑ " : "☐ ") + task[2]
                    listHtml += "<li>" + inlineMarkdown(body) + "</li>"
                    ++i
                }
                listHtml += "</" + tag + ">"
                appendRich(listHtml)
                continue
            }

            var paragraph = [line]
            ++i
            while (i < lines.length && !isBlockStart(lines, i)) {
                paragraph.push(lines[i])
                ++i
            }
            appendRich("<p style=\"margin:3px 0 7px 0; line-height:145%;\">"
                        + paragraph.map(function(part) { return inlineMarkdown(part) }).join("<br/>")
                        + "</p>")
        }
        _blocks = blocks
        _codeBlocks = codeBlocks
        _tableBlockCount = tableCount
        _headingBlockCount = headingCount
        return "<html><body style=\"font-family:'" + escapeHtml(uiFontFamily)
                + "'; color:" + String(color) + ";\">" + output.join("") + "</body></html>"
    }

}
