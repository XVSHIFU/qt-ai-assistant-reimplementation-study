import QtQuick 2.15
import "Controls"

Item {
    id: window
    width: 520
    height: 720
    property real scale: 1.0
    property bool passed: false
    property int testStage: 0
    property int streamingBaselineParses: 0
    property double streamingUpdateElapsedMs: 0
    readonly property string renderedHtml: markdownView.renderedText

    QtObject {
        id: fontManager
        property QtObject item: QtObject {
            property string uiFontFamily: "Segoe UI"
            property string codeFontFamily: "Cascadia Mono"
        }
    }

    QtObject {
        id: theme
        property QtObject item: QtObject {
            property color colorB1: "#18181B"
            property color colorW7: "#52525B"
            property color themeZeroColor: "#4F46E5"
            property color themeOneColor3: "#818CF8"
        }
    }

    MarkdownText {
        id: markdownView
        width: parent.width
        height: contentHeight
        sourceText: "# Heading 1\n\n## Heading 2\n\n### Heading 3\n\n"
                    + "#### Heading 4\n\n##### Heading 5\n\n###### Heading 6\n\n"
                    + "##紧凑中文标题\n\n`inline` and **bold**\n\n"
                    + "> quote\n\n- one\n- [x] done\n\n"
                    + "| Very wide column A with a deliberately long heading | Very wide column B with another deliberately long heading |\n"
                    + "| --- | --- |\n"
                    + "| aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa | bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb |\n\n"
                    + "```cpp\nconst char *raw = \"<tag>\"; // xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n```\n\n"
                    + "    indented();\n\n---\n\n"
                    + "<script src=\"https://invalid.example/x.js\"></script>\n\n"
                    + "![remote](https://invalid.example/pixel.png)\n\n"
                    + "[bad](javascript:alert(1)) [good](https://example.com)"
    }

    MarkdownText {
        id: streamingView
        visible: false
        width: 320
        streaming: true
        markdown: true
    }

    function prepareStreamingBenchmark() {
        streamingBaselineParses = streamingView.markdownParseCount
        var chunk = "streaming delta with **markdown-looking text** and <unsafe-html> 0123456789\n"
        while (chunk.length < 1024)
            chunk += chunk
        chunk = chunk.substring(0, 1024)
        var value = ""
        var started = Date.now()
        for (var update = 0; update < 100; ++update) {
            value += chunk
            streamingView.sourceText = value
        }
        streamingUpdateElapsedMs = Date.now() - started
    }

    function findDescendantByObjectName(item, name) {
        if (!item)
            return null
        if (item.objectName === name)
            return item
        var descendants = item.children || []
        for (var index = 0; index < descendants.length; ++index) {
            var match = findDescendantByObjectName(descendants[index], name)
            if (match)
                return match
        }
        return null
    }

    Timer {
        id: verifyTimer
        interval: 120
        repeat: true
        running: true
        onTriggered: {
            if (window.testStage === 0) {
                window.testStage = 1
                window.prepareStreamingBenchmark()
                return
            }
            if (window.testStage === 1) {
                window.testStage = 2
                return
            }
            var html = markdownView.renderedText
            var normalizedHtml = html.toLowerCase()
            var streamingCoalesced = streamingView.renderedText === streamingView.sourceText
                    && streamingView.sourceText.length === 102400
                    && streamingView.markdownParseCount === streamingBaselineParses
                    && streamingView.streamFlushCount >= 1
                    && streamingView.streamFlushCount <= 3
                    && streamingUpdateElapsedMs < 1000
            var terminalStarted = Date.now()
            streamingView.streaming = false
            var terminalElapsedMs = Date.now() - terminalStarted
            var terminalFlushed = streamingView.markdownParseCount
                    === streamingBaselineParses + 1
                    && streamingView.renderedText.indexOf("<html>") === 0
                    && streamingView.renderedText.indexOf("&lt;unsafe-html&gt;") >= 0
                    && terminalElapsedMs < 2500
            var copied = markdownView.copyCodeBlockAt(0)
            var heading1 = findDescendantByObjectName(markdownView, "markdownHeadingLevel1")
            var heading2 = findDescendantByObjectName(markdownView, "markdownHeadingLevel2")
            var heading3 = findDescendantByObjectName(markdownView, "markdownHeadingLevel3")
            var ok = html.indexOf("<pre") >= 0
                    && html.indexOf("<table") >= 0
                    && html.indexOf("border=\"1\"") >= 0
                    && html.indexOf("white-space:pre;") >= 0
                    && html.indexOf("pre-wrap") < 0
                    && normalizedHtml.indexOf("#7c3aed") >= 0
                    && normalizedHtml.indexOf("#087f5b") >= 0
                    && normalizedHtml.indexOf("#0369a1") >= 0
                    && html.indexOf("<blockquote") < 0
                    && html.indexOf("<img") < 0
                    && html.indexOf("&lt;script") >= 0
                    && html.indexOf("javascript:") < 0
                    && html.indexOf("链接已拦截") >= 0
                    && html.indexOf("https://example.com") >= 0
                    && markdownView.safeHref("data:text/html,bad") === ""
                    && markdownView.safeHref("file:///C:/secret") === ""
                    && markdownView.codeBlockCount === 2
                     && markdownView.tableBlockCount === 1
                    && markdownView.headingBlockCount === 7
                    && markdownView.headingPixelSize(1) > markdownView.headingPixelSize(2)
                    && markdownView.headingPixelSize(2) > markdownView.headingPixelSize(3)
                    && markdownView.headingPixelSize(3) > markdownView.font.pixelSize
                    && html.indexOf("<h1>Heading 1</h1>") >= 0
                    && html.indexOf("<h2>Heading 2</h2>") >= 0
                    && html.indexOf("<h3>Heading 3</h3>") >= 0
                    && html.indexOf("<h2>紧凑中文标题</h2>") >= 0
                    && heading1 !== null && heading2 !== null && heading3 !== null
                    && heading1.visible && heading2.visible && heading3.visible
                    && heading1.contentHeight > 0 && heading2.contentHeight > 0
                    && heading3.contentHeight > 0
                    && heading1.font.pixelSize > heading2.font.pixelSize
                    && heading2.font.pixelSize > heading3.font.pixelSize
                    && markdownView.codeHorizontalOverflow
                    && markdownView.tableHorizontalOverflow
                    && copied
                    && markdownView.lastCopiedCode.indexOf("<tag>") >= 0
                    && markdownView.copiedCodeIndex === 0
                    && streamingCoalesced
                    && terminalFlushed
                    && markdownView.contentHeight > 0
            window.passed = ok
            if (!ok)
                console.log("MARKDOWN_RENDERED_HTML", html)
            console.log("MARKDOWN_100KB_UPDATE_MS", streamingUpdateElapsedMs,
                        "FINAL_PARSE_MS", terminalElapsedMs,
                        "STREAM_FLUSHES", streamingView.streamFlushCount)
            console.log(ok ? "MARKDOWN_SMOKE_PASS" : "MARKDOWN_SMOKE_FAIL")
            running = false
            Qt.quit()
        }
    }
}
