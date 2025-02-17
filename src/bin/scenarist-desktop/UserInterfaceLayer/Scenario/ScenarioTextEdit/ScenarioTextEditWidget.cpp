#include "ScenarioTextEditWidget.h"

#include "ScenarioFastFormatWidget.h"
#include "ScenarioReviewPanel.h"
#include "ScenarioReviewView.h"
#include "ScriptZenModeControls.h"

#include <UserInterfaceLayer/ScenarioTextEdit/ScenarioTextEdit.h>
#include <UserInterfaceLayer/ScenarioTextEdit/ScenarioTextEditHelpers.h>

#include <3rd_party/Helpers/ShortcutHelper.h>
#include <3rd_party/Widgets/FlatButton/FlatButton.h>
#include <3rd_party/Widgets/ScalableWrapper/ScalableWrapper.h>
#include <3rd_party/Widgets/SearchWidget/SearchWidget.h>
#include <3rd_party/Widgets/TabBar/TabBar.h>
#include <3rd_party/Widgets/WAF/Animation/Animation.h>

#include <BusinessLayer/ScenarioDocument/ScenarioTemplate.h>
#include <BusinessLayer/ScenarioDocument/ScenarioTextDocument.h>
#include <BusinessLayer/ScenarioDocument/ScenarioTemplate.h>
#include <BusinessLayer/ScenarioDocument/ScenarioTextBlockInfo.h>
#include <BusinessLayer/Chronometry/ChronometerFacade.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTextBlock>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

using UserInterface::ScenarioTextEditWidget;
using UserInterface::ScenarioReviewPanel;
using UserInterface::ScenarioReviewView;
using UserInterface::ScenarioTextEdit;
using UserInterface::ScriptZenModeControls;
using BusinessLogic::ScenarioTemplateFacade;
using BusinessLogic::ScenarioTemplate;
using BusinessLogic::ScenarioBlockStyle;
using BusinessLogic::SceneHeadingBlockInfo;

namespace {
    /**
     * @brief Нерабочая позиция курсора
     */
    const int INVALID_CURSOR_POSITION = -1;

    /**
     * @brief Исходная позиция курсора после загрузки программы
     */
    int g_initCursorPosition = INVALID_CURSOR_POSITION;

    /**
     * @brief Был ли показан редактор текста хоть раз
     */
    bool g_isBeforeFirstShow = true;

    /**
     * @brief Сформировать метку для разделения элементов в тулбаре
     */
    static QLabel* makeToolbarDivider(QWidget* _parent) {
        QLabel* divider = new QLabel(_parent);
        divider->setFixedWidth(1);
        divider->setProperty("inTopPanel", true);
        divider->setProperty("topPanelTopBordered", true);
        divider->setProperty("topPanelRightBordered", true);
        return divider;
    }
}


ScenarioTextEditWidget::ScenarioTextEditWidget(QWidget* _parent) :
    QFrame(_parent),
    m_editor(new ScenarioTextEdit(this)),
    m_editorWrapper(new ScalableWrapper(m_editor, this)),
    m_toolbar(new QWidget(this)),
    m_outline(new FlatButton(this)),
    m_textStyles(new QComboBox(this)),
    m_undo(new FlatButton(this)),
    m_redo(new FlatButton(this)),
    m_search(new FlatButton(this)),
    m_fastFormat(new FlatButton(this)),
    m_review(new ScenarioReviewPanel(m_editor, this)),
    m_lockUnlock(new FlatButton(this)),
    m_duration(new QLabel(this)),
    m_countersInfo(new QLabel(this)),
    m_searchLine(new SearchWidget(this, true)),
    m_fastFormatWidget(new ScenarioFastFormatWidget(this)),
    m_reviewView(new ScenarioReviewView(this)),
    m_zenControls(new ScriptZenModeControls(this))
{
    initView();
    initConnections();
    initStyleSheet();
}

QWidget* ScenarioTextEditWidget::toolbar() const
{
    return m_toolbar;
}

BusinessLogic::ScenarioTextDocument* ScenarioTextEditWidget::scenarioDocument() const
{
    return qobject_cast<BusinessLogic::ScenarioTextDocument*>(m_editor->document());
}

void ScenarioTextEditWidget::setScenarioDocument(BusinessLogic::ScenarioTextDocument* _document, bool _isDraft)
{
    removeEditorConnections();

    m_editor->setScenarioDocument(_document);
    m_editor->setWatermark(_isDraft ? tr("DRAFT") : QString::null);

    initEditorConnections();
}

void ScenarioTextEditWidget::setDuration(const QString& _duration)
{
    m_duration->setText(_duration);
    m_zenControls->setDuration(_duration);
}

void ScenarioTextEditWidget::setCountersInfo(const QStringList& _counters)
{
    m_countersInfo->setText(_counters.join("&nbsp;"));
    m_zenControls->setCountersInfo(_counters);
}

void ScenarioTextEditWidget::setShowScenesNumbers(bool _show)
{
    m_editor->setShowSceneNumbers(_show);
}

void ScenarioTextEditWidget::setSceneNumbersPrefix(const QString& _prefix)
{
    m_editor->setSceneNumbersPrefix(_prefix);
}

void ScenarioTextEditWidget::setShowDialoguesNumbers(bool _show)
{
    m_editor->setShowDialoguesNumbers(_show);
}

void ScenarioTextEditWidget::setHighlightBlocks(bool _highlight)
{
    m_editor->setHighlightBlocks(_highlight);
}

void ScenarioTextEditWidget::setHighlightCurrentLine(bool _highlight)
{
    m_editor->setHighlightCurrentLine(_highlight);
}

void ScenarioTextEditWidget::setAutoReplacing(bool _capitalizeFirstWord,
    bool _correctDoubleCapitals, bool _replaceThreeDots, bool _smartQuotes)
{
    m_editor->setAutoReplacing(_capitalizeFirstWord, _correctDoubleCapitals, _replaceThreeDots,
        _smartQuotes);
}

void ScenarioTextEditWidget::setUsePageView(bool _use)
{
    //
    // Установка постраничного режима так же тянет за собой ряд настроек
    //
    QMarginsF pageMargins(15, 5, 12, 5);
    Qt::Alignment pageNumbersAlign;
    if (_use) {
        m_editor->setPageFormat(ScenarioTemplateFacade::getTemplate().pageSizeId());
        pageMargins = ScenarioTemplateFacade::getTemplate().pageMargins();
        pageNumbersAlign = ScenarioTemplateFacade::getTemplate().numberingAlignment();
    }

    m_editor->setUsePageMode(_use);
    m_editor->setPageMargins(pageMargins);
    m_editor->setPageNumbersAlignment(pageNumbersAlign);

    //
    // В дополнение установим шрифт по умолчанию для документа (шрифтом будет рисоваться нумерация)
    //
    m_editor->document()->setDefaultFont(
        ScenarioTemplateFacade::getTemplate().blockStyle(ScenarioBlockStyle::Action).font());
}

void ScenarioTextEditWidget::setUseSpellChecker(bool _use)
{
    m_editor->setUseSpellChecker(_use);
}

void ScenarioTextEditWidget::setShowSuggestionsInEmptyBlocks(bool _show)
{
    m_editor->setShowSuggestionsInEmptyBlocks(_show);
}

void ScenarioTextEditWidget::setSpellCheckLanguage(int _language)
{
    m_editor->setSpellCheckLanguage((SpellChecker::Language)_language);
}

void ScenarioTextEditWidget::setTextEditColors(const QColor& _textColor, const QColor& _backgroundColor)
{
    m_editor->viewport()->setStyleSheet(QString("color: %1; background-color: %2;").arg(_textColor.name(), _backgroundColor.name()));
    m_editor->setStyleSheet(QString("#scenarioEditor { color: %1; }").arg(_textColor.name()));
}

void ScenarioTextEditWidget::setTextEditZoomRange(qreal _zoomRange)
{
    m_editorWrapper->setZoomRange(_zoomRange);
}

void ScenarioTextEditWidget::setFixed(bool _fixed)
{
    if (_fixed) {
        m_lockUnlock->setIcons(QIcon(":/Graphics/Iconset/lock.svg"));
    } else {
        m_lockUnlock->setIcons(QIcon(":/Graphics/Iconset/lock-open.svg"));
    }
}

int ScenarioTextEditWidget::cursorPosition() const
{
    return m_editor->textCursor().position();
}

void ScenarioTextEditWidget::setCursorPosition(int _position, bool _isReset, bool _forceScroll)
{
    //
    // Если виджет пока ещё не видно, откладываем событие назначения позиции до этого момента.
    // Делаем это потому что иногда установка курсора происходит до первой отрисовки обёртки
    // масштабирования, что приводит в свою очередь к тому, что полосы прокрутки остаются в начале.
    //
    // Но если редактор ещё ни разу не был показан и пришло новое событие установки, то нужно
    // использовать самую последнюю актуальную позицию
    //
    if (!_isReset) {
        g_initCursorPosition = _position;
    }
    if (!isVisible()) {
        if (g_isBeforeFirstShow) {
            QTimer::singleShot(300, Qt::PreciseTimer, [=] {
                setCursorPosition(g_initCursorPosition, true);
            });
            return;
        }
    } else {
        if (_isReset) {
            _position = g_initCursorPosition;
        }
        g_isBeforeFirstShow = false;
    }

    //
    // Устанавливаем позицию курсора
    //
    QTextCursor cursor = m_editor->textCursor();

    //
    // Если это новая позиция
    //
    if (cursor.position() != _position) {
        //
        // Устанавливаем реальную позицию
        //
        if (_position < m_editor->document()->characterCount()) {
            cursor.setPosition(_position);
        } else {
            cursor.movePosition(QTextCursor::End);
        }

        //
        // Возвращаем курсор в поле зрения
        //
        if (_forceScroll
                || !m_editor->visibleRegion().contains(m_editor->cursorRect(cursor))) {
            m_editor->ensureCursorVisible(cursor);
            m_editorWrapper->setFocus();
        }
    }
    //
    // Если нужно обновить в текущей позиции курсора просто имитируем отправку сигнала
    //
    else {
        emit m_editor->cursorPositionChanged();
    }
}

void ScenarioTextEditWidget::setCurrentBlockType(int _type)
{
    m_editor->changeScenarioBlockType((BusinessLogic::ScenarioBlockStyle::Type)_type);
}

void ScenarioTextEditWidget::addItem(int _position, int _type, const QString& _name,
    const QString& _header, const QString& _description, const QColor& _color)
{
    QTextCursor cursor = m_editor->textCursor();
    cursor.beginEditBlock();

    cursor.setPosition(_position);
    m_editor->setTextCursor(cursor);
    ScenarioBlockStyle::Type type = (ScenarioBlockStyle::Type)_type;

    //
    // Добавим новый блок
    //
    m_editor->addScenarioBlock(type);

    //
    // Устанавливаем текст в блок
    //
    m_editor->insertPlainText(!_header.isEmpty() ? _header : _name);

    //
    // Устанавливаем цвет и описание в параметры сцены
    //
    cursor = m_editor->textCursor();
    QTextBlockUserData* textBlockData = cursor.block().userData();
    //
    // ... если были данные, то у них нужно сменить идентификатор, т.к. это копия оставшаяся от предыдущего блока
    //
    SceneHeadingBlockInfo* info = dynamic_cast<SceneHeadingBlockInfo*>(textBlockData);
    if (info != nullptr) {
        info->rebuildUuid();
    } else {
        info = new SceneHeadingBlockInfo;
    }
    info->setName(_name);
    if (_color.isValid()) {
        info->setColors(_color.name());
    }
    info->setDescription(_description);
    cursor.block().setUserData(info);
    //
    // ... и в сам текст
    //
    if (!_description.isEmpty()) {
        m_editor->addScenarioBlock(ScenarioBlockStyle::SceneDescription);
        m_editor->insertPlainText(_description);
    }

    //
    // Если это группирующий блок, то вставим и закрывающий текст
    //
    if (ScenarioTemplateFacade::getTemplate().blockStyle(type).isEmbeddableHeader()) {
        ScenarioBlockStyle::Type footerType = ScenarioTemplateFacade::getTemplate().blockStyle(type).embeddableFooter();
        cursor = m_editor->textCursor();
        //
        // Но сначала дойдём до подготовленного заранее закрывающего блока
        //
        while (!cursor.atEnd()
               && ScenarioBlockStyle::forBlock(cursor.block()) != footerType) {
            cursor.movePosition(QTextCursor::NextBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        }
        cursor.insertText(Helpers::footerText(!_header.isEmpty() ? _header : _name));
    }

    //
    // А теперь скроем блоки с описанием сцены, если мы не в режиме аутлайна
    //
    const bool isSceneDescriptionVisible = m_editor->visibleBlocksTypes().contains(ScenarioBlockStyle::SceneDescription);
    if (!isSceneDescriptionVisible) {
        cursor.setPosition(_position);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, 2);
        while (ScenarioBlockStyle::forBlock(cursor.block()) == ScenarioBlockStyle::SceneDescription
               && !cursor.atEnd()) {
            cursor.block().setVisible(isSceneDescriptionVisible);
            cursor.movePosition(QTextCursor::EndOfBlock);
            cursor.movePosition(QTextCursor::NextBlock);
        }
    }

    cursor.endEditBlock();

    //
    // Фокусируемся на редакторе
    //
    m_editorWrapper->setFocus();
    m_editor->ensureCursorVisible(cursor);
}

void ScenarioTextEditWidget::editItem(int _startPosition, int _type, const QString& _name,
    const QString& _header, const QString& _colors)
{
    QTextCursor cursor = m_editor->textCursor();
    cursor.beginEditBlock();

    //
    // Идём в начало сцены
    //
    cursor.setPosition(_startPosition);
    m_editor->setTextCursor(cursor);

    //
    // Проверяем тип блока, если нужно - меняем на новый
    //
    ScenarioBlockStyle::Type type = (ScenarioBlockStyle::Type)_type;
    if (ScenarioBlockStyle::forBlock(cursor.block()) != type) {
        m_editor->changeScenarioBlockType(type);
    }

    //
    // Установим заголовок
    //
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(_header);

    //
    // Устанавливаем название блока и описание
    //
    if (SceneHeadingBlockInfo* blockInfo = dynamic_cast<SceneHeadingBlockInfo*>(cursor.block().userData())) {
        blockInfo->setName(_name);
        blockInfo->setColors(_colors);
        cursor.block().setUserData(blockInfo);
    }

    cursor.endEditBlock();
}

void ScenarioTextEditWidget::removeText(int _from, int _to)
{
    QTextCursor cursor = m_editor->textCursor();
    cursor.beginEditBlock();

    //
    // Стираем текст
    //
    cursor.setPosition(_from);
    cursor.setPosition(_to, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    //
    // Если остаётся пустой блок, стираем его тоже
    //
    if (cursor.block().text().isEmpty()) {
        if (cursor.atStart()) {
            cursor.deleteChar();
        } else {
            cursor.deletePreviousChar();
        }
    }

    //
    // Если в сценарии больше не осталось текста, сделаем его единственный блок заголовком сцены
    //
    if (scenarioDocument()->isEmpty()) {
        m_editor->changeScenarioBlockType(ScenarioBlockStyle::SceneHeading, true);
    }

    cursor.endEditBlock();
}

void ScenarioTextEditWidget::updateStylesElements()
{
    //
    // Обновить выпадающий список стилей сценария
    //
    initStylesCombo();

    //
    // Обновить виджет быстрого форматирования
    //
    m_fastFormatWidget->reinitBlockStyles();
    m_zenControls->reinitBlockStyles();
}

void ScenarioTextEditWidget::updateShortcuts()
{
    m_editor->updateShortcuts();
    updateStylesCombo();
}

void ScenarioTextEditWidget::setAdditionalCursors(const QMap<QString, int>& _cursors)
{
    m_editor->setAdditionalCursors(_cursors);
}

void ScenarioTextEditWidget::setCommentOnly(bool _isCommentOnly)
{
    m_textStyles->setEnabled(!_isCommentOnly);
    m_undo->setVisible(!_isCommentOnly);
    m_redo->setVisible(!_isCommentOnly);
    m_fastFormat->setVisible(!_isCommentOnly);
    m_editor->setReadOnly(_isCommentOnly);
    m_searchLine->setSearchOnly(_isCommentOnly);

    if (_isCommentOnly) {
        //
        // Закроем панель быстрого форматирования, если она была открыта
        //
        m_fastFormat->setChecked(false);
        m_fastFormatWidget->hide();
    }
}

void ScenarioTextEditWidget::scrollToAdditionalCursor(int _additionalCursorIndex)
{
    m_editor->scrollToAdditionalCursor(_additionalCursorIndex);
}

#ifdef Q_OS_MAC
void ScenarioTextEditWidget::buildEditMenu(QMenu* _menu)
{
    //
    // При переходе в режим битов и обратно, нужно обновить меню
    //
    static bool s_isConnectedToOutlineToggled = false;
    if (!s_isConnectedToOutlineToggled) {
        connect(m_outline, &FlatButton::toggled, [=] {
            _menu->clear();
            buildEditMenu(_menu);
        });
        s_isConnectedToOutlineToggled = true;
    }

    //
    // Добавляем стандартные пункты меню
    //
    _menu->addAction(tr("Undo"), this, &ScenarioTextEditWidget::undoRequest, QKeySequence::Undo);
    _menu->addAction(tr("Redo"), this, &ScenarioTextEditWidget::redoRequest, QKeySequence::Redo);
    _menu->addSeparator();
    _menu->addAction(tr("Find and replace"), this, &ScenarioTextEditWidget::prepareToSearch, QKeySequence::Find);
    _menu->addSeparator();

    //
    // Добавим пункты меню про формат блоков
    //
    ScenarioTemplate usedTemplate = ScenarioTemplateFacade::getTemplate();
    const bool BEAUTIFY_NAME = true;

    static QList<ScenarioBlockStyle::Type> s_types =
        QList<ScenarioBlockStyle::Type>()
            << ScenarioBlockStyle::SceneHeading
            << ScenarioBlockStyle::SceneCharacters
            << ScenarioBlockStyle::Action
            << ScenarioBlockStyle::Character
            << ScenarioBlockStyle::Dialogue
            << ScenarioBlockStyle::Parenthetical
            << ScenarioBlockStyle::Title
            << ScenarioBlockStyle::Note
            << ScenarioBlockStyle::Transition
            << ScenarioBlockStyle::NoprintableText
            << ScenarioBlockStyle::FolderHeader
            << ScenarioBlockStyle::SceneDescription;

    foreach (ScenarioBlockStyle::Type type, s_types) {
        if (usedTemplate.blockStyle(type).isActive()
            && m_editor->visibleBlocksTypes().contains(type)) {
            _menu->addAction(
                        ScenarioBlockStyle::typeName(type, BEAUTIFY_NAME),
                        [=] { m_editor->changeScenarioBlockType(type); },
                        QKeySequence(m_editor->shortcut(type))
            );
        }
    }
}
#endif

void ScenarioTextEditWidget::prepareToSearch()
{
    //
    // Если поиск виден, но в нём нет фокуса - установим фокус в него
    // В остальных случаях просто покажем, или скроем поиск
    //
    if (m_search->isChecked()
        && m_searchLine->isVisible()
        && !m_searchLine->hasFocus()) {
        m_searchLine->selectText();
        m_searchLine->setFocus();
    }
    //
    // В противном случае сменим видимость
    //
    else {
        m_search->toggle();
    }
}

void ScenarioTextEditWidget::aboutShowSearch()
{
    const bool visible = m_search->isChecked();
    if (m_searchLine->isVisible() != visible) {
        const bool FIX = true;
        const int slideDuration = WAF::Animation::slide(m_searchLine, WAF::FromBottomToTop, FIX, !FIX, visible);
        QTimer::singleShot(slideDuration, [=] { m_searchLine->setVisible(visible); });
    }

    if (visible) {
        m_searchLine->selectText();
        m_searchLine->setFocus();
    } else {
        m_editorWrapper->setFocus();
    }
}

void ScenarioTextEditWidget::aboutShowFastFormat()
{
    m_fastFormatWidget->setVisible(m_fastFormat->isChecked());
    if (m_fastFormatWidget->isVisible()) {
        m_fastFormatWidget->setFocus();
    }
}

void ScenarioTextEditWidget::setZenMode(bool _isZen)
{
    m_zenControls->activate(_isZen);

    //
    // При входе в джен режим скрываем дополнительные панели
    //
    if (_isZen) {
        if (m_fastFormat->isChecked()) {
            m_fastFormatWidget->hide();
        }
        if (m_review->isChecked()) {
            m_reviewView->hide();
        }
        if (m_search->isChecked()) {
            m_searchLine->hide();
        }
    }
    //
    // При выходе из дзен режима отключаем звуки клавиатуры и вновь показываем панели
    //
    else {
        m_editor->setKeyboardSoundEnabled(false);
        if (m_fastFormat->isChecked()) {
            m_fastFormatWidget->show();
        }
        if (m_review->isChecked()) {
            m_reviewView->show();
        }
        if (m_search->isChecked()) {
            m_searchLine->show();
        }
    }
}

void ScenarioTextEditWidget::updateTextMode(bool _outlineMode)
{
    m_editor->setOutlineMode(_outlineMode);

    initStylesCombo();

    //
    // Если возможно редактирование, то отобразим/скроем кнопку панели быстрого редактирования
    //
    if (!m_editor->isReadOnly()) {
        //
        // Включаем/выключаем доступ к панели быстрого форматирования
        //
        if (_outlineMode) {
            m_fastFormat->hide();
            m_fastFormatWidget->hide();
        } else {
            m_fastFormat->show();
            if (m_fastFormat->isChecked()) {
                m_fastFormatWidget->show();
            }
        }
    }

    emit textModeChanged();
}

void ScenarioTextEditWidget::aboutUpdateTextStyle()
{
    ScenarioBlockStyle::Type currentType = m_editor->scenarioBlockType();
    if (currentType == ScenarioBlockStyle::TitleHeader) {
        currentType = ScenarioBlockStyle::Title;
    } else if (currentType == ScenarioBlockStyle::FolderFooter) {
        currentType = ScenarioBlockStyle::FolderHeader;
    }

    for (int itemIndex = 0; itemIndex < m_textStyles->count(); ++itemIndex) {
        ScenarioBlockStyle::Type itemType =
                (ScenarioBlockStyle::Type)m_textStyles->itemData(itemIndex).toInt();
        if (itemType == currentType) {
            m_textStyles->setCurrentIndex(itemIndex);
            break;
        }
    }
}

void ScenarioTextEditWidget::aboutChangeTextStyle()
{
    ScenarioBlockStyle::Type type =
            (ScenarioBlockStyle::Type)m_textStyles->itemData(m_textStyles->currentIndex()).toInt();

    //
    // Меняем стиль блока, если это возможно
    //
    m_editor->changeScenarioBlockTypeForSelection(type);
    m_editorWrapper->setFocus();
}

void ScenarioTextEditWidget::aboutCursorPositionChanged()
{
    emit cursorPositionChanged(m_editor->textCursor().position());
}

void ScenarioTextEditWidget::aboutTextChanged()
{
    if (!m_editor->document()->isEmpty()) {
        emit textChanged();
    }
}

void ScenarioTextEditWidget::aboutStyleChanged()
{
    emit textChanged();
}

void ScenarioTextEditWidget::initView()
{
    m_outline->setObjectName("scenarioOutlineMode");
    m_outline->setIcons(QIcon(":/Graphics/Iconset/view-list.svg"));
    m_outline->setToolTip(tr("Outline mode"));
    m_outline->setCheckable(true);

    m_textStyles->setToolTip(tr("Current Text Block Style"));
    m_textStyles->setSizePolicy(m_textStyles->sizePolicy().horizontalPolicy(), QSizePolicy::Preferred);

    initStylesCombo();

    m_undo->setIcons(QIcon(":/Graphics/Iconset/undo.svg"));
    m_undo->setToolTip(ShortcutHelper::makeToolTip(tr("Undo last action"), "Ctrl+Z"));

    m_redo->setIcons(QIcon(":/Graphics/Iconset/redo.svg"));
    m_redo->setToolTip(ShortcutHelper::makeToolTip(tr("Redo last action"), "Shift+Ctrl+Z"));

    m_search->setObjectName("scenarioSearch");
    m_search->setIcons(QIcon(":/Graphics/Iconset/magnify.svg"));
    m_search->setToolTip(ShortcutHelper::makeToolTip(tr("Search and Replace"), "Ctrl+F"));
    m_search->setCheckable(true);

    m_fastFormat->setObjectName("scenarioFastFormat");
    m_fastFormat->setIcons(QIcon(":/Graphics/Iconset/layers.svg"));
    m_fastFormat->setToolTip(tr("Text Fast Format"));
    m_fastFormat->setCheckable(true);

    m_review->setObjectName("scenarioReview");

    m_duration->setToolTip(tr("Duration from Start to Cursor Position | Full Duration"));
    m_duration->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_duration->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_editor->setObjectName("scenarioEditor");
    m_editor->setPageFormat(ScenarioTemplateFacade::getTemplate().pageSizeId());

    m_searchLine->setEditor(m_editor);
    m_searchLine->hide();

    m_fastFormatWidget->setEditor(m_editor);
    m_fastFormatWidget->hide();

    m_reviewView->setObjectName("reviewView");
    m_reviewView->setEditor(m_editor);
    m_reviewView->setMinimumWidth(100);
    m_reviewView->hide();

    m_lockUnlock->setObjectName("scenarioLockUnlock");
    m_lockUnlock->setIcons(QIcon(":/Graphics/Iconset/lock-open.svg"));
    m_lockUnlock->setToolTip(tr("Lock/unlock scene numbers"));

    m_zenControls->setEditor(m_editor);

    QHBoxLayout* topLayout = new QHBoxLayout(m_toolbar);
    topLayout->setContentsMargins(QMargins());
    topLayout->setSpacing(0);
    topLayout->addWidget(m_outline);
    topLayout->addWidget(::makeToolbarDivider(this));
    topLayout->addWidget(m_undo);
    topLayout->addWidget(m_redo);
    topLayout->addWidget(m_textStyles);
    topLayout->addWidget(m_fastFormat);
    topLayout->addWidget(m_search);
    topLayout->addWidget(::makeToolbarDivider(this));
    topLayout->addWidget(m_review);
    topLayout->addWidget(m_lockUnlock);
    topLayout->addWidget(m_duration);
    topLayout->addWidget(m_countersInfo);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(QMargins());
    mainLayout->addWidget(m_toolbar);
    mainLayout->addWidget(m_editorWrapper);
    mainLayout->addWidget(m_searchLine);

    QSplitter* mainSplitter = new QSplitter(this);
    mainSplitter->setObjectName("mainScenarioSplitter");
    mainSplitter->setHandleWidth(1);
    mainSplitter->setOpaqueResize(false);
    QWidget* mainLayoutWidget = new QWidget(this);
    mainLayoutWidget->setObjectName("mainLayoutWidget");
    mainLayoutWidget->setLayout(mainLayout);
    mainSplitter->addWidget(mainLayoutWidget);
    mainSplitter->addWidget(m_reviewView);
    mainSplitter->setSizes(QList<int>() << 3 << 1);

    QHBoxLayout* layout = new QHBoxLayout;
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    layout->addWidget(mainSplitter);
    layout->addWidget(m_fastFormatWidget);

    setLayout(layout);
}

void ScenarioTextEditWidget::initStylesCombo()
{
    m_textStyles->clear();

    ScenarioTemplate usedTemplate = ScenarioTemplateFacade::getTemplate();
    const bool BEAUTIFY_NAME = true;

    static QList<ScenarioBlockStyle::Type> s_types =
        QList<ScenarioBlockStyle::Type>()
            << ScenarioBlockStyle::SceneHeading
            << ScenarioBlockStyle::SceneCharacters
            << ScenarioBlockStyle::Action
            << ScenarioBlockStyle::Character
            << ScenarioBlockStyle::Dialogue
            << ScenarioBlockStyle::Parenthetical
            << ScenarioBlockStyle::Title
            << ScenarioBlockStyle::Note
            << ScenarioBlockStyle::Transition
            << ScenarioBlockStyle::NoprintableText
            << ScenarioBlockStyle::FolderHeader
            << ScenarioBlockStyle::SceneDescription
            << ScenarioBlockStyle::Lyrics;

    foreach (ScenarioBlockStyle::Type type, s_types) {
        if (usedTemplate.blockStyle(type).isActive()
            && m_editor->visibleBlocksTypes().contains(type)) {
            m_textStyles->addItem(ScenarioBlockStyle::typeName(type, BEAUTIFY_NAME), type);
        }
    }

    updateStylesCombo();
}

void ScenarioTextEditWidget::updateStylesCombo()
{
    for (int index = 0; index < m_textStyles->count(); ++index) {
        ScenarioBlockStyle::Type blockType =
                (ScenarioBlockStyle::Type)m_textStyles->itemData(index).toInt();
        m_textStyles->setItemData(index, m_editor->shortcut(blockType), Qt::ToolTipRole);
    }
}

void ScenarioTextEditWidget::initConnections()
{
    QShortcut* shortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, &ScenarioTextEditWidget::prepareToSearch);

    connect(m_textStyles, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            this, &ScenarioTextEditWidget::aboutChangeTextStyle);
    connect(m_undo, &FlatButton::clicked, this, &ScenarioTextEditWidget::undoRequest);
    connect(m_redo, &FlatButton::clicked, this, &ScenarioTextEditWidget::redoRequest);
    connect(m_search, &FlatButton::toggled, this, &ScenarioTextEditWidget::aboutShowSearch);
    connect(m_fastFormat, &FlatButton::toggled, this, &ScenarioTextEditWidget::aboutShowFastFormat);
    connect(m_fastFormatWidget, &UserInterface::ScenarioFastFormatWidget::focusMovedToEditor,
            [=] { m_editorWrapper->setFocus(); });
    connect(m_review, &ScenarioReviewPanel::toggled, m_reviewView, &ScenarioReviewView::setVisible);
    connect(m_reviewView, &ScenarioReviewView::undoRequest, this, &ScenarioTextEditWidget::undoRequest);
    connect(m_reviewView, &ScenarioReviewView::redoRequest, this, &ScenarioTextEditWidget::redoRequest);
    connect(m_lockUnlock, &FlatButton::clicked, this, &ScenarioTextEditWidget::changeSceneNumbersLockingRequest);
    connect(m_zenControls, &ScriptZenModeControls::quitPressed, this, &ScenarioTextEditWidget::quitFromZenMode);

    initEditorConnections();
}

void ScenarioTextEditWidget::initEditorConnections()
{
    connect(m_outline, &FlatButton::toggled, this, &ScenarioTextEditWidget::updateTextMode);
    connect(m_editor, &ScenarioTextEdit::undoRequest, this, &ScenarioTextEditWidget::undoRequest);
    connect(m_editor, &ScenarioTextEdit::redoRequest, this, &ScenarioTextEditWidget::redoRequest);
    connect(m_editor, &ScenarioTextEdit::currentStyleChanged, this, &ScenarioTextEditWidget::aboutUpdateTextStyle);
    connect(m_editor, &ScenarioTextEdit::cursorPositionChanged, this, &ScenarioTextEditWidget::aboutUpdateTextStyle);
    connect(m_editor, &ScenarioTextEdit::cursorPositionChanged, this, &ScenarioTextEditWidget::aboutCursorPositionChanged);
    connect(m_editor, &ScenarioTextEdit::textChanged, this, &ScenarioTextEditWidget::aboutTextChanged);
    connect(m_editor, &ScenarioTextEdit::styleChanged, this, &ScenarioTextEditWidget::aboutStyleChanged);
    connect(m_editor, &ScenarioTextEdit::reviewChanged, this, &ScenarioTextEditWidget::textChanged);
    connect(m_editor, &ScenarioTextEdit::addBookmarkRequested, this, &ScenarioTextEditWidget::addBookmarkRequested);
    connect(m_editor, &ScenarioTextEdit::removeBookmarkRequested, this, &ScenarioTextEditWidget::removeBookmarkRequested);
    connect(m_editor, &ScenarioTextEdit::renameSceneNumberRequested, this, &ScenarioTextEditWidget::renameSceneNumberRequested);
    connect(m_editorWrapper, &ScalableWrapper::zoomRangeChanged, this, &ScenarioTextEditWidget::zoomRangeChanged);
    connect(m_review, &ScenarioReviewPanel::contextMenuActionsUpdated, m_editor, &ScenarioTextEdit::setReviewContextMenuActions);

    updateTextMode(m_outline->isChecked());
}

void ScenarioTextEditWidget::removeEditorConnections()
{
    disconnect(m_outline, &FlatButton::toggled, this, &ScenarioTextEditWidget::updateTextMode);
    disconnect(m_editor, &ScenarioTextEdit::undoRequest, this, &ScenarioTextEditWidget::undoRequest);
    disconnect(m_editor, &ScenarioTextEdit::redoRequest, this, &ScenarioTextEditWidget::redoRequest);
    disconnect(m_editor, &ScenarioTextEdit::currentStyleChanged, this, &ScenarioTextEditWidget::aboutUpdateTextStyle);
    disconnect(m_editor, &ScenarioTextEdit::cursorPositionChanged, this, &ScenarioTextEditWidget::aboutUpdateTextStyle);
    disconnect(m_editor, &ScenarioTextEdit::cursorPositionChanged, this, &ScenarioTextEditWidget::aboutCursorPositionChanged);
    disconnect(m_editor, &ScenarioTextEdit::textChanged, this, &ScenarioTextEditWidget::aboutTextChanged);
    disconnect(m_editor, &ScenarioTextEdit::styleChanged, this, &ScenarioTextEditWidget::aboutStyleChanged);
    disconnect(m_editor, &ScenarioTextEdit::reviewChanged, this, &ScenarioTextEditWidget::textChanged);
    disconnect(m_editor, &ScenarioTextEdit::addBookmarkRequested, this, &ScenarioTextEditWidget::addBookmarkRequested);
    disconnect(m_editor, &ScenarioTextEdit::removeBookmarkRequested, this, &ScenarioTextEditWidget::removeBookmarkRequested);
    disconnect(m_editor, &ScenarioTextEdit::renameSceneNumberRequested, this, &ScenarioTextEditWidget::renameSceneNumberRequested);
    disconnect(m_editorWrapper, &ScalableWrapper::zoomRangeChanged, this, &ScenarioTextEditWidget::zoomRangeChanged);
    disconnect(m_review, &ScenarioReviewPanel::contextMenuActionsUpdated, m_editor, &ScenarioTextEdit::setReviewContextMenuActions);
}

void ScenarioTextEditWidget::initStyleSheet()
{
    m_outline->setProperty("inTopPanel", true);

    m_textStyles->setProperty("inTopPanel", true);
    m_textStyles->setProperty("topPanelTopBordered", true);
    m_textStyles->setProperty("topPanelLeftBordered", true);
    m_textStyles->setProperty("topPanelRightBordered", true);

    m_undo->setProperty("inTopPanel", true);
    m_redo->setProperty("inTopPanel", true);
    m_fastFormat->setProperty("inTopPanel", true);
    m_search->setProperty("inTopPanel", true);
    m_lockUnlock->setProperty("inTopPanel", true);

    m_duration->setProperty("inTopPanel", true);
    m_duration->setProperty("topPanelTopBordered", true);

    m_countersInfo->setProperty("inTopPanel", true);
    m_countersInfo->setProperty("topPanelTopBordered", true);
    m_countersInfo->setProperty("topPanelRightBordered", true);

    m_editorWrapper->setProperty("mainContainer", true);
    m_reviewView->setProperty("mainContainer", true);
}
