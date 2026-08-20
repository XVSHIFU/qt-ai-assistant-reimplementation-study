#include "backendmocks.h"

#include <QDebug>

WindowMock::WindowMock(QObject *parent) : QObject(parent) {}

void WindowMock::setX(qreal value) { if (qFuzzyCompare(m_x, value)) return; m_x = value; emit geometryChanged(); }
void WindowMock::setY(qreal value) { if (qFuzzyCompare(m_y, value)) return; m_y = value; emit geometryChanged(); }
void WindowMock::setWidth(qreal value) { if (qFuzzyCompare(m_width, value)) return; m_width = value; emit geometryChanged(); }
void WindowMock::setHeight(qreal value) { if (qFuzzyCompare(m_height, value)) return; m_height = value; emit geometryChanged(); }
void WindowMock::setAgreePolicy(bool value) { if (m_agreePolicy == value) return; m_agreePolicy = value; emit agreePolicyChanged(); }
void WindowMock::setTopScreen(bool value) { if (m_topScreen == value) return; m_topScreen = value; emit topScreenChanged(); }
void WindowMock::setSkinMode(int value) { if (m_skinMode == value) return; m_skinMode = value; emit skinModeChanged(); }
void WindowMock::openWebsite(const QString &url) { qInfo() << "MOCK openWebsite" << url; }
void WindowMock::setDialog(bool open) { qInfo() << "MOCK setDialog" << open; }
void WindowMock::setFirstStart(bool firstStart) { qInfo() << "MOCK setFirstStart" << firstStart; }
void WindowMock::setHistory(bool open) { qInfo() << "MOCK setHistory" << open; }
void WindowMock::setXandY(qreal x, qreal y, qreal width, qreal height)
{
    m_x = x; m_y = y; m_width = width; m_height = height; emit geometryChanged();
}
void WindowMock::showGuideWindow() { qInfo() << "MOCK showGuideWindow"; }
void WindowMock::updateSoft() { qInfo() << "MOCK updateSoft"; }

DialogManagerMock::DialogManagerMock(QObject *parent)
    : QObject(parent), m_dialogModel(this), m_dialogInfoModel(this)
{
    connect(&m_dialogModel, &ChatMessageModel::appended, this, [this] {
        emit appended();
        emit chatStateChanged();
    });
}

void DialogManagerMock::setReasonModeActive(bool value)
{
    if (m_reasonModeActive == value) return;
    m_reasonModeActive = value; emit reasonModeActiveChanged();
}

void DialogManagerMock::setSearchModeActive(bool value)
{
    if (m_searchModeActive == value) return;
    m_searchModeActive = value; emit searchModeActiveChanged();
}

void DialogManagerMock::setCurrentIndex(int value)
{
    if (m_currentIndex == value) return;
    m_currentIndex = value; emit currentIndexChanged();
}

void DialogManagerMock::addNewChat()
{
    m_dialogInfoModel.append(QStringLiteral("New mock chat"));
    m_currentIndex = m_dialogInfoModel.rowCount() - 1;
    emit currentIndexChanged(); emit historyStateChanged();
    m_dialogModel.clearAll(); emit chatStateChanged();
}

void DialogManagerMock::sendMessage(const QString &message)
{
    if (message.trimmed().isEmpty()) return;
    m_responseGenerated = false; emit responseGeneratedChanged();
    m_dialogModel.append(0, message);
    m_dialogModel.append(1, QStringLiteral("Mock reply: backend integration is intentionally disabled."),
                         m_reasonModeActive ? QStringLiteral("Local demonstration reasoning") : QString());
    m_responseGenerated = true; emit responseGeneratedChanged(); emit chatStateChanged();
}

void DialogManagerMock::regenerate()
{
    m_dialogModel.append(1, QStringLiteral("Regenerated local mock reply."));
    emit chatStateChanged();
}

void DialogManagerMock::copy(int index) { qInfo() << "MOCK copy message" << index; }

void DialogManagerMock::deleteChatAtIndex(int index)
{
    m_dialogInfoModel.removeAt(index);
    if (m_currentIndex >= m_dialogInfoModel.rowCount()) m_currentIndex = qMax(0, m_dialogInfoModel.rowCount() - 1);
    emit currentIndexChanged(); emit historyStateChanged();
}

void DialogManagerMock::modifyTitleAtIndex(const QString &title, int index)
{
    m_dialogInfoModel.setTitle(index, title);
}

void DialogManagerMock::parseJSONToCurrentChat()
{
    m_dialogModel.clearAll();
    m_dialogModel.append(1, QStringLiteral("Loaded local demonstration chat."));
    emit chatStateChanged();
}

void SettingMock::setStartUpAuto(bool value)
{
    if (m_startUpAuto == value) return;
    m_startUpAuto = value; emit startUpAutoChanged();
}

void SettingMock::checkUpdate() { checkUpdate(true); }
void SettingMock::checkUpdate(bool interactive) { qInfo() << "MOCK checkUpdate" << interactive; }

void ManagerMock::itemClicked(int buttonIndex)
{
    qInfo() << "MOCK tray itemClicked" << buttonIndex;
    emit itemSelected(buttonIndex);
}

